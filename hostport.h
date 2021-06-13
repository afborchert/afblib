/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2019, 2021 Andreas Franz Borchert
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

#ifndef AFBLIB_HOSTPORT_H
#define AFBLIB_HOSTPORT_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <afblib/outbuf.h>

typedef struct hostport {
   /* parameters for socket() */
   int domain;
   int type;
   int protocol;
   /* parameters for bind() or connect() */
   struct sockaddr_storage addr;
   socklen_t namelen;
   /* next result for get_all_hostports, if any */
   struct hostport* next;
} hostport;

bool get_hostport(const char* input, int type, in_port_t defaultport,
   hostport* hp);
hostport* get_all_hostports(const char* input, int type, in_port_t defaultport);
void free_hostport_list(struct hostport* hp);

bool get_hostport_of_peer(int socket, hostport* hp);
bool print_sockaddr(outbuf* out, struct sockaddr* addr, socklen_t namelen);
bool print_hostport(outbuf* out, hostport* hp);

#endif
