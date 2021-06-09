/*
   Small library of useful utilities
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

/*

=head1 NAME

inbuf_readline -- read line from an input buffer

=head1 SYNOPSIS

   #include <afblib/inbuf_readline.h>

   char* inbuf_readline(inbuf* ibuf);

=head1 DESCRIPTION

I<inbuf_readline> reads a sequence of characters from the given input
buffer until a line terminator (LF) or end of file is encountered.
In case of success, a pointer to a character string containing the read
line (without the terminating newline) is returned that was allocated
through I<malloc>. It is the responsibility of the caller to free this
character string as soon as it is no longer required.

In case of errors, 0 is returned. If a read error is encountered
while some input was already gathered, a pointer to the partially
read input is provided. If no memory is available, 0 is always
returned, even if some input was already read.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/
/*
 * Read a string of arbitrary length from a
 * given input buffer. LF is accepted as terminator.
 * 0 is returned in case of errors.
 * afb 3/2003
 */

#include <stdlib.h>
#include <afblib/inbuf_readline.h>

static const int INITIAL_LEN = 8;

char* inbuf_readline(inbuf* ibuf) {
   size_t len = 0; /* current length of string */
   size_t alloc_len = INITIAL_LEN; /* allocated length */
   char* buf = malloc(alloc_len);
   int ch;

   if (buf == 0) return 0;
   while ((ch = inbuf_getchar(ibuf)) >= 0 && ch != '\n') {
      if (len + 1 >= alloc_len) {
         alloc_len *= 2;
         char* newbuf = realloc(buf, alloc_len);
         if (newbuf == 0) {
            free(buf);
            return 0;
         }
         buf = newbuf;
      }
      buf[len++] = ch;
   }
   buf[len++] = '\0';
   return realloc(buf, len);
}
