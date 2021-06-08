/*
   Small library of useful utilities
   Copyright (C) 2015 Andreas Franz Borchert
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
#ifndef AFBLIB_UDP_SESSION_H
#define AFBLIB_UDP_SESSION_H

#include <stdbool.h>
#include <afblib/hostport.h>

typedef struct udp_connection {
   int fd;
   void* handle; /* can be freely used by the application */
   void* global_handle; /* last parameter of of run_udp_service */
   hostport hp;
   bool closed;
   bool initialized; /* false if initial packet has not been processed yet */
   struct output_queue_member* oqhead;
   struct output_queue_member* oqtail;
   struct udp_multiplexor* mpx;
   struct udp_connection* next;
   struct udp_connection* prev;
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

#endif
