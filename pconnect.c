/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2003, 2008, 2017 Andreas Franz Borchert
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
   bool phangup(pipe_end* pipe_end);

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
#include <stdio.h>
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

static void remove_fd(int fd) {
   FD_CLR(fd, &pipes);
}

/*
 * create a pipeline to the given command;
 * mode should be either PIPE_READ or PIPE_WRITE;
 * return a filled pipe_end structure and true on success
 * and false in case of failures
 */
bool pconnect(const char* path, char* const* argv,
      int mode, pipe_end* pipe_con) {
   return pconnect2(path, argv, mode, mode, pipe_con);
}

/*
 * like pconnect() but connect fd to the standard input
 * or output file descriptor that is not connected to the pipe
 */
bool pconnect2(const char* path, char* const* argv,
      int mode, int fd, pipe_end* pipe_con) {
   int pipefds[2];
   if (pipe(pipefds) < 0) return 0;
   int myside = mode; int otherside = 1 - mode;
   pid_t child = fork();
   if (child < 0) {
      close(pipefds[0]); close(pipefds[1]);
      return 0;
   }
   if (child == 0) {
      close(pipefds[myside]);
      dup2(pipefds[otherside], otherside);
      close(pipefds[otherside]);
      if (fd != myside) {
	 dup2(fd, myside); close(fd);
      }
      execvp(path, argv); exit(255);
   }
   close(pipefds[otherside]);
   /* make sure that our side is closed for forked-off childs */
   if (!add_fd(pipefds[myside])) return false;
   /* make sure that our side is closed when we exec */
   int flags = fcntl(pipefds[myside], F_GETFD);
   flags |= FD_CLOEXEC;
   fcntl(pipefds[myside], F_SETFD, flags);
   pipe_con->pid = child;
   pipe_con->fd = pipefds[myside];
   pipe_con->wstat = 0;
   return true;
}

/*
 * close pipeline and wait for the forked-off process to exit;
 * the wait status is returned in wstat (if non-null);
 * true is returned if successful, false otherwise
 */
bool phangup(pipe_end* pipe) {
   remove_fd(pipe->fd);
   if (close(pipe->fd) < 0) return false;
   if (waitpid(pipe->pid, &pipe->wstat, 0) < 0) return false;
   return true;
}
