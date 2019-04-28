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

/*

=head1 NAME

inbuf -- buffered reading

=head1 SYNOPSIS

   #include <afblib/inbuf.h>

   typedef struct inbuf {
      int fd;
      stralloc buf;
      size_t pos;
   } inbuf;

   bool inbuf_alloc(inbuf* ibuf, size_t size);
   ssize_t inbuf_read(inbuf* ibuf, void* buf, size_t size);
   int inbuf_getchar(inbuf* ibuf);
   bool inbuf_back(inbuf* ibuf);
   void inbuf_free(inbuf* ibuf);

=head1 DESCRIPTION

These function provides a simple and efficient input buffering
for reading from file descriptors. They are mainly useful for
pipelines and network connections. In case of bidirectional
communication channels, it might be useful to use the outbuf
interface as well. An input and an output buffer can be used
in parallel for the same file descriptor.

There is no such method called F<open> or F<init>. Instead,
the structure needs to be initialized, either using an
initializer, e.g.

   int fd;
   // fd = open(...);
   inbuf ibuf = {fd}; // the file descriptor is the first component

or by using a compound-literal (C99), e.g.

   int fd;
   inbuf ibuf;
   // fd = open(...);
   ibuf = (inbuf) {fd};

The function F<inbuf_alloc> may be invoked to select a non-standard
buffer size. Otherwise, a buffer of 512 bytes is allocated on the
next invocation of F<inbuf_read> or F<inbuf_getchar>.

Reading is possible through the functions F<inbuf_read> and
F<inbuf_back>. The former is close to the system call L<read(2)>
while F<inbuf_getchar> is a convenient shortform for reading
individual characters. Whenever a read operation was successful,
at least one byte might be reread after the invocation of
F<inbuf_back>. F<inbuf_free> frees the input buffer.

F<inbuf_read>, if the input buffer needs to be filled, invokes
F<read> repeatedly whenever the system call got interrupted
(errno = EINTR).

=head1 DIAGNOSTICS

F<inbuf_alloc> returns 1 in case of success, 0 otherwise.
F<inbuf_read> return positive amounts indicating the number
of bytes read in case of success, 0 in case of end of file,
and -1 in case of errors. F<inbuf_getchar> returns -1 in
case of errors or end of file. Non-negative values represent
the character read. F<inbuf_back> returns 1 on success and
0 otherwise.

=head1 EXAMPLE

The following sample reads character-wise from standard input
(file descriptor 0):

   inbuf ibuf = {0};
   int ch;
   while ((ch = inbuf_getchar(&ibuf)) >= 0) {
      // process ch
   }

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <afblib/inbuf.h>

/* set size of input buffer */
bool inbuf_alloc(inbuf* ibuf, size_t size) {
   return stralloc_ready(&ibuf->buf, size);
}

/* works like read(2) but from ibuf */
ssize_t inbuf_read(inbuf* ibuf, void* buf, size_t size) {
   if (size == 0) return 0;
   if (ibuf->pos >= ibuf->buf.len) {
      if (ibuf->buf.a == 0 && !inbuf_alloc(ibuf, 512)) return -1;
      /* fill input buffer */
      ssize_t nbytes;
      do {
         errno = 0;
         nbytes = read(ibuf->fd, ibuf->buf.s, ibuf->buf.a);
      } while (nbytes < 0 && errno == EINTR);
      if (nbytes <= 0) return nbytes;
      ibuf->buf.len = nbytes;
      ibuf->pos = 0;
   }
   ssize_t nbytes = ibuf->buf.len - ibuf->pos;
   if (size < nbytes) nbytes = size;
   memcpy(buf, ibuf->buf.s + ibuf->pos, nbytes);
   ibuf->pos += nbytes;
   return nbytes;
}

/* works like fgetc but from ibuf */
int inbuf_getchar(inbuf* ibuf) {
   char ch;
   ssize_t nbytes = inbuf_read(ibuf, &ch, sizeof ch);
   if (nbytes <= 0) return -1;
   return ch;
}

/* move backward one position */
bool inbuf_back(inbuf* ibuf) {
   if (ibuf->pos == 0) return false;
   ibuf->pos--;
   return true;
}

/* release storage associated with ibuf */
void inbuf_free(inbuf* ibuf) {
   stralloc_free(&ibuf->buf);
}
