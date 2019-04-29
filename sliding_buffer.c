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

/*

=head1 NAME

sliding_buffer -- growing buffer where old contents can be expired

=head1 SYNOPSIS

   #include <afblib/sliding_buffer.h>

   typedef struct {
      size_t offset;
      stralloc sa;
   } sliding_buffer;

   bool sliding_buffer_ready(sliding_buffer* buffer, size_t minspace);
   void sliding_buffer_free(sliding_buffer* buffer);

=head1 DESCRIPTION

A sliding buffer is a I<stralloc>-based buffer that dynamically grows
but where just the contents from buffer position I<offset> to I<sa.len>
is possibly used and where the buffer contents before I<offset> is no
longer needed. New contents may be added anytime, causing I<sa.len> to
grow. I<offset> may be increased anytime but must not exceed
I<sa.len>. This allows to expire no longer needed buffer contents.

I<sliding_buffer_ready> makes sure that I<minspace> bytes are available
between I<sa.len> and I<sa.a>. To assure this, I<sliding_buffer_ready>
is free either to grow the buffer (i.e. by resizing I<sa.a> much like
I<stralloc_readyplus> would do it) or by moving its contents, causing
I<offset> to be set to 0, and I<sa.len> to shrink accordingly. On
success, B<true> is returned, in case of failures, i.e. out of memory,
B<false> is returned.

I<sliding_buffer_free> is a shorthand for I<stralloc_free> on
I<sa>.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <stralloc.h>
#include <string.h>
#include <afblib/sliding_buffer.h>

#define THRESHOLD 64  /* should be similar to the size of a cache line */

bool sliding_buffer_ready(sliding_buffer* buffer, size_t minspace) {
   size_t len = buffer->sa.len - buffer->offset;
   if (len == 0) {
      /* this is easy, seize the opportunity */
      buffer->offset = buffer->sa.len = 0;
   } else if (buffer->offset >= THRESHOLD && len <= THRESHOLD) {
      /* we have a good opportunity to shift the contents right away */
      memmove(buffer->sa.s, buffer->sa.s + buffer->offset, len);
      buffer->offset = 0; buffer->sa.len = len;
   }
   if (buffer->sa.a - buffer->sa.len >= minspace) return true;
   size_t space_left = buffer->sa.a - buffer->sa.len + buffer->offset;
   if (space_left < minspace) {
      /* we cannot avoid to allocate more space */
      return stralloc_readyplus(&buffer->sa, minspace);
   } else {
      /* we have sufficient space, we just need to shift */
      memmove(buffer->sa.s, buffer->sa.s + buffer->offset, len);
      buffer->offset = 0; buffer->sa.len = len;
      return true;
   }
}

void sliding_buffer_free(sliding_buffer* buffer) {
   stralloc_free(&buffer->sa);
}
