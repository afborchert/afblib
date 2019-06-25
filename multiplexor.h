/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2003, 2008, 2013, 2019 Andreas Franz Borchert
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

#ifndef AFBLIB_MULTIPLEXOR_H
#define AFBLIB_MULTIPLEXOR_H

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct connection {
   int fd;
   void* handle; /* may be freely used by the application */
   void* mpx_handle; /* corresponding parameter from run_multiplexor */
   /* private fields */
   struct multiplexor* mpx; /* internal link to global structure */
   bool eof;
   struct output_queue_member* oqhead;
   struct output_queue_member* oqtail;
   struct connection* next;
   struct connection* prev;
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

#endif
