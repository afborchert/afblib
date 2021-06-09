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

outbuf -- buffered output

=head1 SYNOPSIS

   #include <afblib/outbuf.h>

   typedef struct outbuf {
      int fd;
      stralloc buf;
   } outbuf;

   ssize_t outbuf_write(outbuf* obuf, void* buf, size_t size);
   int outbuf_putchar(outbuf* obuf, char ch);
   bool outbuf_flush(outbuf* obuf);
   void outbuf_free(outbuf* obuf);

=head1 DESCRIPTION

These functions provide a simple and efficient interface for
buffered output. They are mainly useful in connection with
pipelines and network connections. In case of bidirectional
communication channels, it might be useful to use the inbuf
interface as well. An input and outbuf buffer can be used in
parallel for the same file descriptor.

There is no such method called F<open> or F<init>. Instead,
the structure needs to be initialized, either using an
initializer, e.g.

   int fd;
   // fd = open(...);
   outbuf obuf = {fd}; // the file descriptor is the first component

or by using a compound-literal (C99), e.g.

   int fd;
   outbuf obuf;
   // fd = open(...);
   obuf = (outbuf) {fd};

Unlike the stdio, outbuf uses no buffers of a fixed size.  Instead, the
outbuf buffer grows dynamically until F<outbuf_flush> gets called. This
gives, in comparison to the stdio, the programmer full control when
output is sent over the file descriptor.

The function F<outbuf_write> is works like L<write(2)> but stores
everything into the output buffer. F<outbuf_putchar> is a convenient
shorthand for single character write operations. F<outbuf_flush>
copies the contents of the output buffer to the file descriptor.
F<outbuf_flush> invokes, if neccessary, L<write(2)> multiple times
until everything is written or an error occures. In case of EINTR
errors, the write operation is automatically retried again.
F<outbuf_free> frees the memory associated with the output buffer.

=head1 DIAGNOSTICS

F<outbuf_write> returns the number of bytes written, and -1 in
case of errors. F<outbuf_putchar> returns the character written,
or -1 in case of an error. F<outbuf_flush> returns true in case
of success, and false otherwise.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stralloc.h>
#include <string.h>
#include <afblib/outbuf.h>

/* works like write(2) but to obuf */
ssize_t outbuf_write(outbuf* obuf, void* buf, size_t size) {
   if (size == 0) return 0;
   if (!stralloc_readyplus(&obuf->buf, size)) return -1;
   memcpy(obuf->buf.s + obuf->buf.len, buf, size);
   obuf->buf.len += size;
   return size;
}

/* works like fputc but to obuf */
int outbuf_putchar(outbuf* obuf, char ch) {
   if (outbuf_write(obuf, &ch, sizeof ch) <= 0) return -1;
   return ch;
}

/* write contents of obuf to the associated fd */
bool outbuf_flush(outbuf* obuf) {
   ssize_t left = obuf->buf.len; ssize_t written = 0;
   while (left > 0) {
      ssize_t nbytes;
      do {
         errno = 0;
         nbytes = write(obuf->fd, obuf->buf.s + written, left);
      } while (nbytes < 0 && errno == EINTR);
      if (nbytes <= 0) return false;
      left -= nbytes; written += nbytes;
   }
   obuf->buf.len = 0;
   return true;
}

/* release storage associated with obuf */
void outbuf_free(outbuf* obuf) {
   stralloc_free(&obuf->buf);
}
