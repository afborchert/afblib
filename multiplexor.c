/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2013, 2017, 2019 Andreas Franz Borchert
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

multiplexor -- handle concurrent network sessions

=head1 SYNOPSIS

   #include <afblib/multiplexor.h>

   typedef struct connection {
      int fd;
      void* handle;
      void* mpx_handle;
      // additional private fields
   } connection;

   typedef void (*multiplexor_handler)(connection* link);

   void run_multiplexor(int socket,
      multiplexor_handler open_handler,
      multiplexor_handler input_handler,
      multiplexor_handler close_handler,
      void* mpx_handle);
   bool write_to_link(connection* link, char* buf, size_t len);
   ssize_t read_from_link(connection* link, char* buf, size_t len);
   void close_link(connection* link);

=head1 DESCRIPTION

These functions allow to handle multiple network connections within
one address space by one process without resorting to threads.

I<run_multiplexor> accepts any number of incoming stream-oriented
network connections coming through I<socket>, monitors the existing
connections for incoming data, invokes I<ohandler>, if not null, for
each new connection, invokes I<ihandler> whenever the next input packet
can be read using I<read_from_link>, and allows through
I<write_to_link> to file response packets in a non-blocking way. The
close handler I<chandler>, if non-null, is invoked by
I<run_multiplexor> as soon as a session terminates. This function runs
infinitely and returns in case of errors only.

All handlers get a connection handler that allows to distinguish
between individual sessions. Behind I<link> following fields are
provided that might be of interest for the handlers:

=over 4

=item I<fd>

This integer value represents the file descriptor of the network
connection. It may be used for functions like I<getpeername>
but not for I/O operations like I<read> or I<write>.

=item I<handle>

This field is initially null and remains untouched by I<run_multiplexor>.
This handle allows the input handler to recognize new sessions (if the
handle is null) and to assign a pointer to an object representing the
associated session.  If this handle is being used and points to a
dynamically allocated object, it should be disposed by the close handler.

=item I<mpx_handle>

This handle points to an optional handle representing the network
service that has been passed to I<run_multiplexor>. It can be set to
null if it is not needed.

=back

Input handlers that want to generate a response packet must use the
I<write_to_link> function that works like I<write> but takes a
connection link (passed to the input handler) instead of a file
descriptor. Calls to I<write_to_link> never block. The only possible
failure which can be expected from a I<write_to_link> invocation is
to run out of memory.

The input handler must not use I<read> but I<read_from_link> to
read the next input packet. Multiple calls are not permitted,
i.e. each input handler, if invoked, must call I<read_from_link>
exactly once. If I<read_from_link> gets no more input as the
connection has been closed, the close handler will be invoked,
if defined.

The output buffer that is passed to I<write_to_link> is, if
I<write_to_link> returns B<true>, subsequently owned by this module and
freed when it is no longer needed. It must not be reused or freed by
the caller.

I<close_link> allows to shutdown the reading side of a connection,
i.e. the input handler will no longer be called, just the pending
list of response packets will be handled.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <afblib/multiplexor.h>

typedef struct output_queue_member {
   char* buf;
   size_t len;
   size_t pos;
   struct output_queue_member* next;
} output_queue_member;

typedef struct multiplexor {
   /* parameters passed to run_multiplexor */
   int socket;
   multiplexor_handler ohandler, ihandler, chandler;
   void* mpx_handle;
   /* additional administrative fields */
   bool socketok; /* becomes false when accept() fails */
   connection* head; /* double-linked linear list of connections */
   connection* tail; /* its last element */
   size_t count; /* number of connections */
   struct pollfd* pollfds; /* parameter for poll() */
   size_t pollfdslen; /* allocated len of pollfds */
   connection** pollcs; /* of the same len as pollfds */
} multiplexor;

/* remove a connection from the double-linked linear
   list of connections
*/
static void remove_link(multiplexor* mpx, connection* link) {
   close(link->fd);
   if (link->prev) {
      link->prev->next = link->next;
   } else {
      mpx->head = link->next;
   }
   if (link->next) {
      link->next->prev = link->prev;
   } else {
      mpx->tail = link->prev;
   }
   if (mpx->chandler) (*mpx->chandler)(link);
   free(link);
   --mpx->count;
}

/* prepare fields pollfds and pollfdslen in mpx in
   dependence of the current set of connections */
static size_t setup_polls(multiplexor* mpx) {
   size_t len = mpx->count;
   if (mpx->socketok) ++len;
   if (len == 0) return 0;
   /* weed out links which have been closed
      and where our output queue is empty */
   connection* link = mpx->head;
   while (link) {
      connection* next = link->next;
      if (link->eof && link->oqhead == 0) remove_link(mpx, link);
      link = next;
   }
   /* allocate or enlarge pollfds, if necessary */
   if (mpx->pollfdslen < len) {
      mpx->pollfds = realloc(mpx->pollfds, sizeof(struct pollfd) * len);
      if (mpx->pollfds == 0) return 0;
      mpx->pollcs = realloc(mpx->pollcs, sizeof(connection*) * len);
      if (mpx->pollcs == 0) return 0;
      mpx->pollfdslen = len;
   }
   size_t index = 0;
   /* look for new network connections as long accept()
      returned no errors so far */
   if (mpx->socketok) {
      mpx->pollcs[index] = 0;
      mpx->pollfds[index++] = (struct pollfd) {mpx->socket, POLLIN};
   }
   /* look for incoming network connections and
      check whether we can write any pending output packets
      without blocking */
   link = mpx->head;
   while (link) {
      short events = 0;
      if (!link->eof) events |= POLLIN;
      if (link->oqhead) events |= POLLOUT;
      mpx->pollcs[index] = link;
      mpx->pollfds[index++] = (struct pollfd) {link->fd, events};
      link = link->next;
   }
   return index;
}

/* add a new connection to the double-linked linear
   list of connections */
static bool add_connection(multiplexor* mpx) {
   int newfd;
   if ((newfd = accept(mpx->socket, 0, 0)) < 0) {
      mpx->socketok = false; return true;
   }
   connection* link = malloc(sizeof(connection));
   if (link == 0) return false;
   *link = (connection) {
      .fd = newfd,
      .handle = 0,
      .mpx = mpx,
      .mpx_handle = mpx->mpx_handle,
      .eof = false,
      .oqhead = 0, .oqtail = 0,
      .next = 0, .prev = mpx->tail,
   };
   if (mpx->tail) {
      mpx->tail->next = link;
   } else {
      mpx->head = link;
   }
   mpx->tail = link;
   ++mpx->count;
   if (mpx->ohandler) (*mpx->ohandler)(link);
   return true;
}

/* read one input packet from the given network connection */
ssize_t read_from_link(connection* link, char* buf, size_t len) {
   if (link->eof) return 0;
   ssize_t nbytes = read(link->fd, buf, len);
   if (nbytes <= 0) {
      link->eof = true;
      if (link->oqhead == 0) remove_link(link->mpx, link);
   }
   return nbytes;
}

/* write one pending output packet to the given network connection */
static void write_to_socket(multiplexor* mpx, connection* link) {
   ssize_t nbytes = write(link->fd,
      link->oqhead->buf + link->oqhead->pos,
      link->oqhead->len - link->oqhead->pos);
   if (nbytes <= 0) {
      remove_link(mpx, link);
   } else {
      link->oqhead->pos += nbytes;
      if (link->oqhead->pos == link->oqhead->len) {
	 output_queue_member* old = link->oqhead;
	 link->oqhead = old->next;
	 if (link->oqhead == 0) {
	    link->oqtail = 0;
	 }
	 free(old->buf); free(old);
	 if (link->oqhead == 0 && link->eof) {
	    remove_link(mpx, link);
	 }
      }
   }
}

void run_multiplexor(int socket,
      multiplexor_handler open_handler,
      multiplexor_handler input_handler,
      multiplexor_handler close_handler,
      void* mpx_handle) {
   /* ignore SIGPIPE as we might receive this signal
      on writing to connections which were already
      closed by our client */
   struct sigaction sigact = {.sa_handler = SIG_IGN};
   struct sigaction old_sigact = {0};
   if (sigaction(SIGPIPE, &sigact, &old_sigact) < 0) return;

   multiplexor mpx = {
      .socket = socket,
      .ohandler = open_handler,
      .ihandler = input_handler,
      .chandler = close_handler,
      .mpx_handle = mpx_handle,
      .socketok = true,
   };
   size_t count;
   while ((count = setup_polls(&mpx)) > 0) {
      if (poll(mpx.pollfds, count, -1) <= 0) return;
      for (size_t index = 0; index < count; ++index) {
	 if (mpx.pollfds[index].revents == 0) continue;
	 int fd = mpx.pollfds[index].fd;
	 if (fd == mpx.socket) {
	    if (!add_connection(&mpx)) return;
	 } else {
	    connection* link = mpx.pollcs[index]; assert(link);
	    if (mpx.pollfds[index].revents & POLLIN) {
	       (*mpx.ihandler)(link);
	    }
	    if (mpx.pollfds[index].revents & POLLOUT) {
	       write_to_socket(&mpx, link);
	    }
	 }
      }
   }

   /* restore previous SIGPIPE handler */
   sigaction(SIGPIPE, &old_sigact, 0);
}

bool write_to_link(connection* link, char* buf, size_t len) {
   assert(len >= 0);
   if (len == 0) {
      free(buf); return true;
   }
   output_queue_member* member = malloc(sizeof(output_queue_member));
   if (!member) return false;
   member->buf = buf; member->len = len; member->pos = 0;
   member->next = 0;
   if (link->oqtail) {
      link->oqtail->next = member;
   } else {
      link->oqhead = member;
   }
   link->oqtail = member;
   return true;
}

void close_link(connection* link) {
   link->eof = true;
   shutdown(link->fd, SHUT_RD);
}
