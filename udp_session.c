/*
   Small library of useful utilities
   Copyright (C) 2015, 2021 Andreas Franz Borchert
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

udp_session -- run an UDP service with sessions

=head1 SYNOPSIS

   #include <afblib/udp_session.h>

   typedef struct udp_connection {
      void* handle;
      void* global_handle;
      // ...
   } udp_connection;

   typedef void (*udp_connection_handler)(udp_connection* link);

   void run_udp_service(hostport* hp,
      int timeout, unsigned int max_retries,
      udp_connection_handler open_handler,
      udp_connection_handler input_handler,
      udp_connection_handler close_handler,
      void* global_handle);

   bool write_to_udp_link(udp_connection* link, void* buf, size_t len);
   ssize_t read_from_udp_link(udp_connection* link, void* buf, size_t len);
   void close_udp_link(udp_connection* link);

=head1 DESCRIPTION

I<run_udp_service> creates a socket that listens on the
given hostport and waits for incoming packets. Each packet to
the main socket opens a new session which is responded to
using a new port which is used throughout the session
(as in the TFTP protocol, see RFC 1350).

On each incoming packet to the main socket, I<open_handler>
is invoked which is expected to read one incoming packet
using I<read_from_udp_link>. For each incoming packet
for one of the session sockets, I<input_handler> is
invoked which is likewise expected to invoke
I<read_from_udp_link>. Packets may be sent using
I<write_to_udp_link>. These packets are retransmitted up to
I<max_retries> times if no response comes within the
timeperiod of I<timeout> milliseconds. If multiple packets
are sent without responding packets, just the last
packet passed to I<write_to_udp_link> will be retransmitted.

Connections are considered closed
when I<close_udp_link> is called, one associated I/O operation
fails, or when the number of retransmissions hits the
maximal number of retries I<max_retries>. Pending output
packets will be sent but no further input accepted.
As soon a connection is no longer in use, I<close_handler>
will be invoked, allowing it to free the resources associated
with that connection link.

A connection link provides following fields:

=over 4

=item I<handle>

This field is initially null and may be freely used to maintain
a pointer to a custom session object.

=item I<global_handle>

This field depends on the I<global_handle> parameter passed
to I<run_udp_service>.

=back

I<run_udp_service> runs normally infinitely and returns in
error cases only.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <afblib/udp_session.h>

typedef struct output_queue_member {
   void* buf;
   size_t len;
   unsigned int attempts; /* how often has it been sent? */
   unsigned int timeouts; /* how often did it timeout? */
   struct output_queue_member* next; /* next in queue */
} output_queue_member;

typedef struct udp_multiplexor {
   int socket;
   hostport hp;
   unsigned int next_timeout; /* for next invocation of poll */
   unsigned int max_retries; /* maximal number of retries */
   udp_connection_handler ohandler, ihandler, chandler;
   void* global_handle; /* handle passed to run_udp_service */
   udp_connection* head; /* double-linked linear list of connections */
   udp_connection* tail; /* its last element */
   bool socketok; /* becomes false if recvfrom fails */
   int count; /* number of connections */
   int timeout; /* minimal timeout value of the connections */
   struct pollfd* pollfds; /* parameter for poll() */
   unsigned int pollfdslen; /* allocated len of pollfds */
   udp_connection** pollcs; /* of the same len as pollfds */
} multiplexor;

/* discard head of output queue */
static void discard_oq_head(udp_connection* link) {
   if (link->oqhead) {
      output_queue_member* old = link->oqhead;
      link->oqhead = link->oqhead->next;
      free(old);
      if (!link->oqhead) link->oqtail = 0;
   }
}

/* discard output queue of the given link */
static void discard_oq(udp_connection* link) {
   while (link->oqhead) {
      discard_oq_head(link);
   }
}

/* remove a connection from the double-linked linear
   list of connections
*/
static void remove_link(multiplexor* mpx, udp_connection* link) {
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
   discard_oq(link);
   free(link);
   --mpx->count;
}

/* prepare fields pollfds and pollfdslen in mpx in
   dependence of the current set of connections */
static int setup_polls(multiplexor* mpx) {
   /* weed out links which have been closed or which timed out
      and where our output queue is empty */
   udp_connection* link = mpx->head;
   while (link) {
      udp_connection* next = link->next;
      if (link->oqhead) {
	 output_queue_member* head = link->oqhead;
	 if (head->attempts >= mpx->max_retries) {
	    /* timeout after maximal number of retries */
	    discard_oq(link);
	    link->closed = true; /* due to timeout */
	 } else if (head->attempts > 0 && link->closed) {
	    /* do not resend packets when we are closing */
	    discard_oq_head(link);
	 }
      }
      assert(link->closed || link->fd >= 0);
      if (link->closed && link->oqhead == 0) remove_link(mpx, link);
      link = next;
   }
   int len = mpx->count;
   if (mpx->socketok) ++len;
   if (len == 0) return 0;
   /* allocate or enlarge pollfds, if necessary */
   if (mpx->pollfdslen < len) {
      mpx->pollfds = realloc(mpx->pollfds, sizeof(struct pollfd) * len);
      if (mpx->pollfds == 0) return 0;
      mpx->pollcs = realloc(mpx->pollcs, sizeof(udp_connection*) * len);
      if (mpx->pollcs == 0) return 0;
      mpx->pollfdslen = len;
   }
   int index = 0;
   /* look for new packets to the main socket */
   if (mpx->socketok) {
      mpx->pollcs[index] = 0;
      mpx->pollfds[index++] = (struct pollfd) {mpx->socket, POLLIN};
   }
   /* look for incoming network connections and
      check whether we can write any pending output packets
      without blocking */
   mpx->next_timeout = -1;
   link = mpx->head;
   while (link) {
      assert(link->initialized);
      short events = 0;
      if (!link->closed) events |= POLLIN;
      if (link->oqhead) {
	 /* if we are waiting for an already sent packet to
	    be acknowledged, we need to specify a timeout for poll */
	 if (!link->closed && link->oqhead->attempts > 0) {
	    mpx->next_timeout = mpx->timeout;
	 }
	 /* include POLLOUT only when this is the first attempt or
	    if the last attempt already timed out */
	 if (link->oqhead && link->oqhead->timeouts == link->oqhead->attempts) {
	    events |= POLLOUT;
	 }
      }
      mpx->pollcs[index] = link;
      mpx->pollfds[index++] = (struct pollfd) {link->fd, events};
      link = link->next;
   }
   return index;
}

/* write one pending output packet to the given network connection */
static void write_to_socket(multiplexor* mpx, udp_connection* link) {
   output_queue_member* head = link->oqhead;
   ssize_t nbytes = send(link->fd, head->buf, head->len, 0);
   if (nbytes < 0) {
      remove_link(mpx, link);
   } else if (head->next) {
      /* do not keep it for resending if there are more
         packets in our queue */
      discard_oq_head(link);
   } else {
      /* keep it to permit retransmissions */
      ++head->attempts;
   }
}

/* add a new connection to the double-linked linear
   list of connections */
static bool add_connection(multiplexor* mpx) {
   udp_connection* link = malloc(sizeof(udp_connection));
   if (link == 0) return false;
   *link = (udp_connection) {
      .fd = mpx->socket,
      .handle = 0,
      .global_handle = mpx->global_handle,
      .closed = false,
      .initialized = false,
      .oqhead = 0, .oqtail = 0,
      .next = 0, .prev = mpx->tail,
      .hp = mpx->hp,
      .mpx = mpx,
   };
   if (mpx->tail) {
      mpx->tail->next = link;
   } else {
      mpx->head = link;
   }
   mpx->tail = link;
   ++mpx->count;
   if (mpx->ohandler) {
      (*mpx->ohandler)(link); /* is expected to handle first packet */
   } else {
      (*mpx->ihandler)(link); /* handle first packet */
   }
   /* first packet should have been handled */
   assert(link->closed || link->initialized);
   return true;
}

void run_udp_service(hostport* hp,
      int timeout, unsigned int max_retries,
      udp_connection_handler open_handler,
      udp_connection_handler input_handler,
      udp_connection_handler close_handler,
      void* global_handle) {
   assert(timeout > 0);
   if (!hp->type) {
      hp->type = SOCK_DGRAM;
   }
   int sfd = socket(hp->domain, hp->type, hp->protocol);
   if (sfd < 0 || bind(sfd, (struct sockaddr *) &hp->addr, hp->namelen) < 0) {
      return;
   }
   multiplexor mpx = {
      .socket = sfd,
      .hp = *hp,
      .timeout = timeout, .max_retries = max_retries,
      .ohandler = open_handler,
      .ihandler = input_handler,
      .chandler = close_handler,
      .global_handle = global_handle,
      .socketok = true,
   };
   /* poll-based event loop */
   int count;
   while ((count = setup_polls(&mpx)) > 0) {
      int res = poll(mpx.pollfds, count, mpx.next_timeout);
      if (res < 0) return;
      if (res > 0) {
	 /* at least one event occurred */
	 for (int index = 0; index < count; ++index) {
	    if (mpx.pollfds[index].revents == 0) continue;
	    int fd = mpx.pollfds[index].fd;
	    if (fd == mpx.socket) {
	       if (!add_connection(&mpx)) return;
	    } else {
	       udp_connection* link = mpx.pollcs[index]; assert(link);
	       if (mpx.pollfds[index].revents & POLLIN) {
		  if (link->oqhead && link->oqhead->attempts > 0) {
		     /* consider last output packet as being confirmed */
		     discard_oq_head(link);
		  }
		  (*mpx.ihandler)(link);
	       }
	       if (mpx.pollfds[index].revents & POLLOUT) {
		  write_to_socket(&mpx, link);
	       }
	    }
	 }
      } else {
	 /* timeout: make sure that packets get resent where necessary */
	 for (int index = 0; index < count; ++index) {
	    int fd = mpx.pollfds[index].fd;
	    if (fd != mpx.socket) {
	       udp_connection* link = mpx.pollcs[index]; assert(link);
	       output_queue_member* head = link->oqhead;
	       if (head && head->timeouts < head->attempts &&
		     mpx.pollfds[index].events & POLLOUT) {
		  /* packet gets resent at the next opportunity */
		  ++head->timeouts;
	       }
	    }
	 }
      }
   }
}

/* send buf as packet via the given link;
   the buffer is released as soon as it is sent & acknowledged */
bool write_to_udp_link(udp_connection* link, void* buf, size_t len) {
   assert(len >= 0);
   output_queue_member* member = malloc(sizeof(output_queue_member));
   if (!member) return false;
   member->buf = buf; member->len = len;
   member->attempts = 0; member->timeouts = 0;
   member->next = 0;
   if (link->oqtail) {
      link->oqtail->next = member;
   } else {
      link->oqhead = member;
   }
   link->oqtail = member;
   return true;
}

/* read next packet from the next link;
   this function is to be called once within the open and input handler */
ssize_t read_from_udp_link(udp_connection* link, void* buf, size_t len) {
   if (link->closed) return 0;
   ssize_t nbytes;
   if (link->initialized) {
      nbytes = read(link->fd, buf, len);
   } else {
      /* retrieve sender's address into link->hp such
         that we are able to connect to that address */
      socklen_t namelen;
      nbytes = recvfrom(link->mpx->socket, buf, len, 0,
	 (struct sockaddr*) &link->hp.addr, &namelen);
      if (nbytes >= 0) {
	 /* hp.namelen is signed while namelen is unsigned */
	 link->hp.namelen = namelen;
	 /* create new socket that is directly connected to
	    our new peer; note that the new socket gets
	    a port assigned by the system which identifies
	    this session */
	 int fd = socket(link->hp.domain, link->hp.type, link->hp.protocol);
	 if (fd < 0 ||
	       connect(fd, (struct sockaddr*)&link->hp.addr,
		  link->hp.namelen) < 0) {
	    nbytes = -1;
	 } else {
	    link->fd = fd;
	    link->initialized = true;
	 }
      }
   }
   /* drop new link if we were not able to establish a new connection */
   if (nbytes < 0) {
      link->closed = true;
      if (link->oqhead == 0) remove_link((multiplexor*)link->mpx, link);
   }
   return nbytes;
}

/* close connection when the pending output packets have been sent */
void close_udp_link(udp_connection* link) {
   link->closed = true;
   if (link->fd >= 0) {
      shutdown(link->fd, SHUT_RD);
   }
}
