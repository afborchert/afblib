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

hostport -- support of host/port tuple specifications according to RFC 2396

=head1 SYNOPSIS

   #include <afblib/hostport.h>

   typedef struct hostport {
      // parameters for socket()
      int domain;
      int protocol;
      // parameters for bind() or connect()
      struct sockaddr_storage addr;
      int namelen;
   } hostport;

   bool parse_hostport(const char* input, hostport* hp, in_port_t defaultport);

=head1 DESCRIPTION

I<parse_hostport> supports the textual specification of hosts,
either in domain style or as numerical IP address, and an
optional port number. The term hostport was coined within
RFC 2396 which provides the generic syntax for Uniform
Resource Identifiers (URI). This syntax was extended by RFC
2732 to support IPv6 addresses which were already specified
in RFC 2373. (IPv6 addresses are, however, in the current
implementation not yet supported.)

Following syntax is supported:

   hostport        = host [ ":" port ]
   host            = hostname | IPv4address
   hostname        = { domainlabel "." } toplabel [ "." ]
   domainlabel     = alphanum | alphanum { alphanum  |  "-"  }
		     alphanum
   toplabel        = alpha | alpha { alphanum | "-" } alphanum
   IPv4address     = { digit } "."  { digit } "." { digit } "." { digit }

I<parse_hostport> expects in I<input> a string that conforms to
the given syntax and returns, in case of success, a I<hostport>
structure that can be used for subsequent calls of I<socket> and
I<bind> or I<connect>. A default port can be specified using
I<defaultport>. This port is taken if no port is specified within
I<input>.

In addition, a hostport specification is permitted that begins
with '/' or '.'. This is then considered to be the path of a UNIX
domain socket.

=head1 DIAGNOSTICS

I<parse_hostport> returns I<true> in case of success, and
I<false> otherwise.

=head1 BUGS

Support of IPv6 addresses is not yet included.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <arpa/inet.h>
#include <netdb.h>
#include <stralloc.h>
#include <string.h>
#include <sys/un.h>
#include <afblib/hostport.h>

typedef struct inbuf {
   const char* buf;
   unsigned int len;
   unsigned int pos;
} inbuf;

typedef struct host {
   enum {IPv4, HOSTNAME} variant;
   stralloc text;
} host;

static int inbuf_getchar(inbuf* ibuf) {
   if (ibuf->pos < ibuf->len) {
      return ibuf->buf[ibuf->pos++];
   }
   return -1;
}

static int inbuf_back(inbuf* ibuf) {
   if (ibuf->pos > 0) {
      --(ibuf->pos);
      return 1;
   } else {
      return 0;
   }
}

static bool parse_delimiter(inbuf* ibuf, char delimiter) {
   int ch = inbuf_getchar(ibuf);
   if (ch < 0) return false;
   if (ch == delimiter) return true;
   inbuf_back(ibuf);
   return false;
}

static bool parse_host(inbuf* ibuf, host* h) {
   bool valid_dotted_decimal = true;
   int digits = 0;
   int dots = 0;
   int ch;
   while ((ch = inbuf_getchar(ibuf)) >= 0 && ch != ':') {
      stralloc_readyplus(&h->text, 1);
      h->text.s[h->text.len++] = ch;
      if (ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch == '-') {
	 valid_dotted_decimal = false;
      } else if (ch >= '0' && ch <= '9') {
	 ++digits;
      } else if (ch == '.') {
	 ++dots;
	 if (digits == 0 || dots > 3) {
	    valid_dotted_decimal = false;
	 }
      } else {
	 return false;
      }
   }
   if (ch > 0) inbuf_back(ibuf);
   valid_dotted_decimal = dots == 3 && digits > 0;
   if (valid_dotted_decimal) {
      h->variant = IPv4;
   } else {
      h->variant = HOSTNAME;
   }
   return true;
}

static bool parse_port(inbuf* ibuf, in_port_t* port) {
   int portval = 0;
   int ch;
   while ((ch = inbuf_getchar(ibuf)) > 0 &&
	 ch >= '0' && ch <= '9') {
      portval = portval * 10 + ch - '0';
      if (portval > 65535) {
	 return false;
      }
   }
   if (ch > 0) inbuf_back(ibuf);
   if (portval == 0) return false;
   *port = portval;
   return true;
}

bool parse_hostport(const char* input, hostport* hp, in_port_t defaultport) {
   if (input[0] == '/' || input[0] == '.') {
      /* special case: UNIX domain socket */
      hp->domain = PF_UNIX;
      hp->protocol = 0;
      struct sockaddr_un* sp = (struct sockaddr_un*) &hp->addr;
      sp->sun_family = AF_UNIX;
      strncpy(sp->sun_path, input, sizeof sp->sun_path);
      hp->namelen = sizeof(struct sockaddr_un);
      return true;
   }
   inbuf ibuf = {input, strlen(input), 0};
   host h = {0};
   if (!parse_host(&ibuf, &h)) {
      stralloc_free(&h.text); return false;
   }
   stralloc_0(&h.text); /* needed by inet_addr and gethostbyname */
   in_port_t port;
   if (parse_delimiter(&ibuf, ':')) {
      if (!parse_port(&ibuf, &port)) {
	 stralloc_free(&h.text); return false;
      }
   } else {
      port = defaultport;
   }

   /* construct IPv4 socket address,
      other address spaces (like IPv6) are currently not yet supported */
   struct sockaddr_in sockaddr = {0};
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_port = htons(port);

   switch (h.variant) {
      case IPv4:
	 {
	    in_addr_t addr = inet_addr(h.text.s);
	    if (addr == (in_addr_t)(-1)) {
	       stralloc_free(&h.text); return false;
	    }
	    sockaddr.sin_addr.s_addr = addr;
	 }
	 break;
      case HOSTNAME:
	 {
	    struct hostent* hp;
	    if ((hp = gethostbyname(h.text.s)) == 0) {
	       stralloc_free(&h.text); return false;
	    }
	    char* hostaddr = hp->h_addr_list[0];
	    /* currently we support AF_INET only */
	    if (hp->h_addrtype != AF_INET) return false;
	    in_addr_t addr;
	    memmove((void *) &addr, (void *) hostaddr, hp->h_length);
	    sockaddr.sin_addr.s_addr = addr;
	 }
	 break;
   }
   stralloc_free(&h.text);

   hp->domain = PF_INET;
   hp->protocol = 0;
   memcpy(&hp->addr, &sockaddr, sizeof(sockaddr));
   hp->namelen = sizeof(sockaddr);
   return true;
}
