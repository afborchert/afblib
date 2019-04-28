/*
   Small library of useful utilities
   Copyright (C) 2013, 2014 Andreas Franz Borchert
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

run_preforked_service -- run a TCP-based service on a given hostport

=head1 SYNOPSIS

   #include <afblib/preforked_service.h>

   typedef void (*session_handler)(int fd, int argc, char** argv);

   void run_preforked_service(hostport* hp, session_handler handler,
	 unsigned int number_of_processes, int argc, char** argv) {

=head1 DESCRIPTION

I<run_preforked service> creates a socket with the given address specified
by hp (see L<hostport>) and creates the given number of processes to
which the socket is inherited. Each of these processes waits until
a new connection is dispatched to it. Then it will run the session
and another preforked process will be created as replacement.
This means that there will be always I<number_of_processes> processes
ready to accept a connection.

If the main process gets a SIGTERM signal, this will be distributed
to all children, causing all processes to terminate. Running sessions,
however, will not be interrupted.

I<run_preforked_service> terminates only in case of errors or
in case of SIGTERM (errno is EINTR in this case).
The forked-off processes do not become zombies and their exit
status will not be available through I<wait>.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <afblib/preforked_service.h>

static volatile sig_atomic_t terminate = 0;

static void termination_handler(int sig) {
   terminate = 1;
}

/* create preforked process that
    - waits until accept() returns
    - and signals that a session was accepted by closing the pipe */
static pid_t spawn_preforked_process(int sfd, int pipefds[2],
      session_handler handler, int argc, char** argv) {
   if (pipe(pipefds) < 0) return -1;
   pid_t child = fork();
   if (child) {
      close(pipefds[1]);
      return child;
   }
   close(pipefds[0]);

   int fd = accept(sfd, 0, 0);
   close(sfd);
   if (fd < 0) exit(1);
   /* now close the writing side of the pipe to indicate that
      we are busy with running a session */
   close(pipefds[1]);
   /* run the session and exit */
   handler(fd, argc, argv);
   exit(0);
}

/*
 * listen on the given hostport and invoke the handler for each
 * incoming connection in a separate process
 */
void run_preforked_service(hostport* hp, session_handler handler,
      unsigned int number_of_processes, int argc, char** argv) {
   assert(number_of_processes > 0);
   int sfd = socket(hp->domain, SOCK_STREAM, hp->protocol);
   int optval = 1;
   if (sfd < 0 ||
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof optval) < 0 ||
        bind(sfd, (struct sockaddr *) &hp->addr, hp->namelen) < 0 ||
        listen(sfd, SOMAXCONN) < 0) {
      close(sfd);
      return;
   }

   /* setup termination handler */
   struct sigaction action = {
      .sa_handler = termination_handler,
   };
   if (sigaction(SIGTERM, &action, 0) != 0) {
      return;
   }

   /* our children shall not become zombies */
   struct sigaction action2 = {
      .sa_handler = SIG_IGN,
      .sa_flags = SA_NOCLDWAIT,
   };
   if (sigaction(SIGCHLD, &action2, 0) < 0) exit(1);

   /* create preforked processes */
   pid_t child_pid[number_of_processes];
   struct pollfd pollfds[number_of_processes];
   for (int i = 0; i < number_of_processes; ++i) {
      /* a pipe is used to signal that one of the
	 preforked processes accepted a connection */
      int pipefds[2];
      pid_t pid = spawn_preforked_process(sfd, pipefds, handler, argc, argv);
      pollfds[i] = (struct pollfd) { .fd = pipefds[0], .events = POLLIN};
      if (pid < 0) return;
      child_pid[i] = pid;
   }

   /* start a new preforked process for every one terminating */
   while (!terminate) {
      if (poll(pollfds, number_of_processes, -1) <= 0) break;
      for (int i = 0; i < number_of_processes; ++i) {
	 if (pollfds[i].revents == 0) continue;
	 /* close reading end of the pipe */
	 close(pollfds[i].fd);
	 /* start a new preforked process for every process
	    that accepted a connection */
	 int pipefds[2];
	 pid_t pid = spawn_preforked_process(sfd, pipefds, handler, argc, argv);
	 if (pid < 0) return;
	 pollfds[i] = (struct pollfd) { .fd = pipefds[0], .events = POLLIN};
	 child_pid[i] = pid;
      }
   }

   /* terminate everything */
   for (int i = 0; i < number_of_processes; ++i) {
      if (child_pid[i] > 0) {
	 kill(child_pid[i], SIGTERM);
      }
   }
}
