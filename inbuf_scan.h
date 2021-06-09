/*
   Small library of useful utilities
   Copyright (C) 2013, 2015 Andreas Franz Borchert
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
#ifndef AFBLIB_INBUF_SCAN_H
#define AFBLIB_INBUF_SCAN_H

#include <stddef.h>
#include <afblib/inbuf.h>

int inbuf_scan(inbuf* ibuf, const char* regexp, ...);

typedef struct {
   /* pointer to last captured substring */
   const char* captured; /* is 0, if nothing has been captured */
   size_t captured_len; /* length of captured substring */
   int callout_number; /* callout number if given, 0 otherwise */
} inbuf_scan_callout_block;
typedef int (*inbuf_scan_callout_function)(inbuf_scan_callout_block*,
      void* callout_data);

int inbuf_scan_with_callouts(inbuf* ibuf, const char* regexp,
      inbuf_scan_callout_function callout, void* callout_data);

#endif
