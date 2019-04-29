/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2003, 2008 Andreas Franz Borchert
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

typedef struct hostport {
   /* parameters for socket() */
   int domain;
   int protocol;
   /* parameters for bind() or connect() */
   struct sockaddr_storage addr;
   int namelen;
} hostport;

bool parse_hostport(const char* input, hostport* hp, in_port_t defaultport);

#endif
