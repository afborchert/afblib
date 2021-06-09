/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2015 Andreas Franz Borchert
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

inbuf_sareadline -- read line from an input buffer into a stralloc object

=head1 SYNOPSIS

   #include <afblib/inbuf_sareadline.h>

   bool inbuf_sareadline(inbuf* ibuf, stralloc* sa);

=head1 DESCRIPTION

I<inbuf_sareadline> reads a sequence of characters from the given input
buffer until a line terminator (LF) or end of file is encountered.
In case of success, 1 is returned, and I<sa> is filled with the
input line (but no newline and also no null-byte).

In case of errors, 0 is returned. If a read error is encountered
while some input was already gathered, the so far processed input
is to be found in F<sa>.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <stralloc.h>
#include <afblib/inbuf_sareadline.h>

bool inbuf_sareadline(inbuf* ibuf, stralloc* sa) {
   sa->len = 0;
   for(;;) {
      int ch;
      if ((ch = inbuf_getchar(ibuf)) < 0) return false;
      if (ch == '\n') break;
      if (!stralloc_readyplus(sa, 1)) return false;
      sa->s[sa->len++] = ch;
   }
   return true;
}
