/*
   Small library of useful utilities based on the dietlib by fefe
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

outbuf_printf -- formatted output to an outbuf object

=head1 SYNOPSIS

   #include <afblib/outbuf_printf.h>

   int outbuf_printf(outbuf* obuf, const char* restrict format, ...);

=head1 DESCRIPTION

The function F<outbuf_printf> works similar to F<fprintf> but
prints to an outbuf instead of a FILE object.

=head1 DIAGNOSTICS

F<outbuf_printf> returns the number of bytes written. In case
of errors, -1 is returned.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <afblib/outbuf_printf.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

int outbuf_printf(outbuf* obuf, const char* restrict format, ...) {
   va_list ap;
   va_start(ap, format);
   /* for each call of vsnprintf we need a separate copy of ap;
      see http://www.bailopan.net/blog/?p=30 */
   va_list ap2; va_copy(ap2, ap);
   /* let vsnprintf compute how many bytes are generated */
   int nbytes = vsnprintf(0, 0, format, ap);

   if (nbytes > 0) {
      char* buf = malloc(nbytes + 1);
      if (!buf) {
	 va_end(ap); return -1;
      }
      nbytes = vsnprintf(buf, nbytes + 1, format, ap2);
      if (nbytes > 0) {
	 nbytes = outbuf_write(obuf, buf, nbytes);
      }
      free(buf);
   }
   va_end(ap); va_end(ap2);
   return nbytes;
}
