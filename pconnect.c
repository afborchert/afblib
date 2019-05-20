/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2003, 2008, 2017, 2019 Andreas Franz Borchert
   --------------------------------------------------------------------
   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*

=head1 NAME

pconnect -- create a pipeline to a given command

=head1 SYNOPSIS

   #include <afblib/pconnect.h>

   enum {PIPE_READ = 0, PIPE_WRITE = 1};

   typedef struct pipe_end {
      int fd;
      pid_t pid;
      int wstat;
   } pipe_end;

   bool pconnect(const char* path, char* const* argv,
	 int mode, pipe_end* pipe_con);
   bool pconnect2(const char* path, char* const* argv,
         int mode, int fd, pipe_end* pipe_con);
   bool phangup(pipe_end* pipe_con);
   bool pshare(pipe_end* pipe_con);
   bool pcut(pipe_end* pipe_con);
   bool pwait(pipe_end* pipe_con);

=head1 DESCRIPTION

I<pconnect> provides a safe alternative to I<popen> by avoiding
the invocation of a shell to interpret a command line. Instead of
a command string, a path for the command and an argument list is
passed to I<pconnect> that are passed directly to L<execvp(2)>
in the forked-off process. The I<mode> which has to be either
I<PIPE_READ> or I<PIPE_WRITE> selects the end of the pipe that
is left to the invoking process. Pipelines created by I<pconnect>
are to be closed by I<phangup> which waits for the forked-off
process to terminate and returns its status in the I<wstat> field.

I<pconnect2> works similar to I<pconnect> but connects in the spawned off
process I<fd> to the remaining standard input or output file descriptor
not connected to the pipeline. This allows pipelines to be chained.
However, I<fd> needs to be shared using I<pshare> (see below) if
this file descriptor belong to another pipe end returned by
I<pconnect> or I<pconnect2>.

All pipe ends returned by I<pconnect> and I<pconnect2> are protected
against being inherited through I<fork> or I<exec>. This protection
can be lifted using I<pshare>. This is necessary if a pipe end
is to be passed on to a child. In case of a shared pipe end
I<pcut> and I<pwait> can be used instead of I<phangup>. I<pcut>
closes the file descriptor associated with the pipe end while
I<pwait> waits for the associated process to terminate.
This separation allows I<pcut> to be invoked immediately after
the I<fork> on the parent side.

=head1 DIAGNOSTICS

All functions return true on success and false otherwise.

=head1 EXAMPLE

The following example retrieves the output of ``ps -fu login''
where F<login> is some given login name:

   char ps_path[] = "/usr/bin/ps";
   strlist argv = {0};
   strlist_push(&argv, ps_path);
   strlist_push(&argv, "-fu");
   strlist_push(&argv, login);
   strlist_push0(&argv);
   pipe_end pipe;
   bool ok = pconnect(ps_path, argv.list, PIPE_READ, &pipe);
   strlist_free(&argv);
   if (!ok) return 0;

   stralloc ps_output = {0};
   ssize_t nbytes;
   char buf[512];
   while ((nbytes = read(pipe.fd, buf, sizeof buf)) > 0) {
      stralloc_catb(&ps_output, buf, nbytes);
   }
   phangup(&pipe);

=head1 AUTHOR

Andreas F. Borchert

=cut

*/
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <afblib/pconnect.h>

static bool initialized = false;
static fd_set pipes;

static void child_after_fork_handler() {
   /* close all pipes that were opened by pconnect/pconnect2 */
   for (int fd = 0; fd < FD_SETSIZE; ++fd) {
      if (FD_ISSET(fd, &pipes)) {
	 close(fd);
      }
   }
   FD_ZERO(&pipes);
}

static bool add_fd(int fd) {
   if (!initialized) {
      FD_ZERO(&pipes);
      if (pthread_atfork(0, 0, child_after_fork_handler) < 0) {
	 return false;
      }
      initialized = true;
   }
   FD_SET(fd, &pipes);
   return true;
}

static bool remove_fd(int fd) {
   if (FD_ISSET(fd, &pipes)) {
      FD_CLR(fd, &pipes);
      return true;
   } else {
      return false;
   }
}

static bool share_fd(int fd) {
   if (!remove_fd(fd)) {
      return false;
   }
   int flags = fcntl(fd, F_GETFD);
   flags &= ~FD_CLOEXEC;
   fcntl(fd, F_SETFD, flags);
   return true;
}

/* create a pipeline to the given command;
   mode should be either PIPE_READ or PIPE_WRITE;
   return a filled pipe_end structure and true on success
   and false in case of failures */
bool pconnect(const char* path, char* const* argv,
      int mode, pipe_end* pipe_con) {
   return pconnect2(path, argv, mode, mode, pipe_con);
}

/* like pconnect() but connect fd to the standard input
   or output file descriptor that is not connected to the pipe */
bool pconnect2(const char* path, char* const* argv,
      int mode, int fd, pipe_end* pipe_con) {
   int pipefds[2];
   if (pipe(pipefds) < 0) return false;
   int parent_side = mode; int child_side = 1 - mode;
   pid_t child = fork();
   if (child < 0) {
      close(pipefds[0]); close(pipefds[1]);
      return false;
   }
   if (child == 0) {
      close(pipefds[parent_side]);
      dup2(pipefds[child_side], child_side);
      close(pipefds[child_side]);
      if (fd != parent_side) {
	 dup2(fd, parent_side); close(fd);
      }
      execvp(path, argv); exit(255);
   }
   close(pipefds[child_side]);
   /* make sure that our side is closed for forked-off childs */
   if (!add_fd(pipefds[parent_side])) return false;
   /* make sure that our side is closed when we exec */
   int flags = fcntl(pipefds[parent_side], F_GETFD);
   flags |= FD_CLOEXEC;
   fcntl(pipefds[parent_side], F_SETFD, flags);
   pipe_con->pid = child;
   pipe_con->fd = pipefds[parent_side];
   pipe_con->wstat = 0;
   return true;
}

/* close pipeline and wait for the forked-off process to exit;
   the wait status is returned in pipe->wstat;
   true is returned if successful, false otherwise */
bool phangup(pipe_end* pipe) {
   remove_fd(pipe->fd);
   if (close(pipe->fd) < 0) return false;
   if (waitpid(pipe->pid, &pipe->wstat, 0) < 0) return false;
   return true;
}

bool pshare(pipe_end* pipe) {
   return share_fd(pipe->fd);
}

bool pcut(pipe_end* pipe) {
   int fd = pipe->fd;
   if (FD_ISSET(fd, &pipes)) {
      return false;
   }
   return close(fd) >= 0;
}

bool pwait(pipe_end* pipe) {
   if (FD_ISSET(pipe->fd, &pipes)) {
      return false;
   }
   return waitpid(pipe->pid, &pipe->wstat, 0) >= 0;
}
