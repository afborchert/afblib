/*
   Small library of useful utilities
   Copyright (C) 2013, 2014, 2021 Andreas Franz Borchert
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
#ifndef AFBLIB_PREFORKED_SERVICE_H
#define AFBLIB_PREFORKED_SERVICE_H

#include <afblib/hostport.h>

typedef void (*session_handler)(int fd, void* service_handle);

/* listen on the given hostport and invoke the handler for each
    incoming connection in a separate process */
void run_preforked_service(hostport* hp, session_handler handler,
   unsigned int number_of_processes, void* service_handle);

#endif
