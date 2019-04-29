/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2013, 2014 Andreas Franz Borchert
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
#ifndef AFBLIB_MPX_SESSION_H
#define AFBLIB_MPX_SESSION_H

#include <stdarg.h>
#include <stralloc.h>
#include <sys/types.h>
#include <afblib/hostport.h>
#include <afblib/multiplexor.h>
#include <afblib/sliding_buffer.h>

typedef struct session {
   void* handle; /* can be freely used by the application */
   void* global_handle; /* last parameter of run_mpx_service */
   const char* request;
   size_t request_len;
   /* private fields */
   connection* link;
   size_t offset;
   int ovecsize;
   int* ovector;
   int count;
   sliding_buffer buffer;
} session;

typedef void (*mpx_handler)(session* s);

int mpx_session_scan(session* s, ...);
   /* must not be invoked outside the associated read handler and
      may be called only once;
      it works like inbuf_scan and considers the regular expression
      given to run_mpx_service */

int mpx_session_printf(session* s, const char* restrict format, ...);
int mpx_session_vprintf(session* s, const char* restrict format, va_list ap);
void close_session(session* s);

void run_mpx_service(hostport* hp, const char* regexp,
   mpx_handler ohandler, mpx_handler rhandler, mpx_handler hhandler,
   void* global_handle);

#endif
