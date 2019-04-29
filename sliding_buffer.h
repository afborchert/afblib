/*
   Small library of useful utilities
   Copyright (C) 2013 Andreas Franz Borchert
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
#ifndef AFBLIB_SLIDING_BUFFER_H
#define AFBLIB_SLIDING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stralloc.h>

/*
   this sliding buffer is a datastructure where we just add data
   without altering previously added data;
   offset marks the point before which the contents has been obsoleted;
   if necessary, the contents gets reshifted
*/
typedef struct {
   size_t offset;
      /* sa.s[0] .. sa.s[offset-1] is no longer needed;
         offset <= sa.len */
   stralloc sa;
} sliding_buffer;

bool sliding_buffer_ready(sliding_buffer* buffer, size_t minspace);
   /* makes sure that s.a - s.len >= minspace;
      if this condition does not hold yet, it is achieved either by
        - shifting its contents to get more space or by
	- enlarging the buffer space */

void sliding_buffer_free(sliding_buffer* buffer);

#endif
