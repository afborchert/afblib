/*
   Small library of useful utilities
   Copyright (C) 2021 Andreas Franz Borchert
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

run_mt_service -- run a TCP-based service on a given hostport using a thread per session

=head1 SYNOPSIS

   #include <afblib/mt-service.h>

   typedef void (*session_handler)(int fd, void* service_handle);

   void run_mt_service(hostport* hp, session_handler handler,
      void* service_handle);

=head1 DESCRIPTION

I<run_mt_service> creates a socket with the given address specified by hp
(see L<hostport>), and accepts connections. For each accepted connection,
a thread is created that invokes the provided I<handler> which is called
with the provided handle I<service_handle>.  As soon the session
handler returns, the spawned off process terminates with an exit code
of 0.

I<run_mt_service> terminates only in case of errors.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <afblib/mt-service.h>

struct session_parameters {
   session_handler handler;
   int fd;
   void* service_handle;
};

void* spawn_session(void* arg) {
   struct session_parameters* pp = arg;
   pp->handler(pp->fd, pp->service_handle);
   free(pp);
   return 0;
}

/* listen on the given port and invoke the handler for each
   incoming connection in a new thread */
void run_mt_service(hostport* hp, session_handler handler,
      void* service_handle) {
   if (!hp->type) {
      hp->type = SOCK_STREAM;
   }
   int sfd = socket(hp->domain, hp->type, hp->protocol);
   int optval = 1;
   if (sfd < 0 ||
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof optval) < 0 ||
        bind(sfd, (struct sockaddr *) &hp->addr, hp->namelen) < 0 ||
        listen(sfd, SOMAXCONN) < 0) {
      close(sfd);
      return;
   }

   int fd;
   while ((fd = accept(sfd, 0, 0)) >= 0) {
      struct session_parameters* pp = malloc(sizeof(struct session_parameters));
      if (!pp) break;
      *pp = (struct session_parameters) {
	 .handler = handler,
	 .fd = fd,
	 .service_handle = service_handle,
      };
      pthread_t thread;
      if (pthread_create(&thread, 0, spawn_session, pp) ||
	    pthread_detach(thread)) {
	 break;
      }
   }
}
