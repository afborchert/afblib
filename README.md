# afblib

This is a small C library with some utilities based on the POSIX standard.
Included is

 * support for buffered input and output
   ([_inbuf_](https://github.com/afborchert/afblib/wiki/inbuf) and
   [_outbuf_](https://github.com/afborchert/afblib/wiki/outbuf)) with
   some functions based on that including
   [_inbuf\_scan](https://github.com/afborchert/afblib/wiki/inbuf_scan)
   that supports regular-expression-based input parsing,
 * various options for network services with parallel sessions, and
 * support for communication and synchronization among multiple
   processes using shared memory and
 * more random stuff.

The main focus of the library is not for production but for
demonstrating some aspects of the POSIX standard. It comes
without any warranty but can be freely distributed under the
terms of the GPL 2.0. See COPYING for details.

Due to the assorted characteristics of the library, it is named
after the initials of its author.

# Documentation

Manual pages can be found in the associated
[wiki](https://github.com/afborchert/afblib/wiki).

# Downloading

Please clone this repository recursively using

   git clone --recursive https://github.com/afborchert/afblib.git

# Requirements

Following libraries are required:

 * _stralloc_ by Dan J. Bernstein: <http://cr.yp.to/lib/stralloc.html>.
   There exist multiple libraries supporting _stralloc_. One popular option
   is the _diet libc_, see <https://www.fefe.de/dietlibc/>.
   Under Debian and Ubuntu you will need the _libowfat-dev_ package.
   For MacOS/Homebrew you will need
   [_libowfat_](https://formulae.brew.sh/formula/libowfat).
 * _pcre_ by Philip Hazel: <https://pcre.org/>. Under Debian and
   Ubuntu you will need _libpcre3-dev_. For MacOS/Homebrew you
   will need [_pcre_](https://formulae.brew.sh/formula/pcre).

The _Makefile_ includes support for Linux, Solaris, and MacOS. In case
of MacOS you will need to check the _BREW\_ROOT_ variable for the
Brew home directory.
