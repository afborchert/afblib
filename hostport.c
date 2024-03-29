/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2019, 2021 Andreas Franz Borchert
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
      int type;
      int protocol;
      // parameters for bind() or connect()
      struct sockaddr_storage addr;
      socklen_t namelen;
      // next result for get_all_hostports, if any
      struct hostport* next;
   } hostport;

   bool get_hostport(const char* input, int type, in_port_t defaultport,
      hostport* hp);

   bool get_all_hostports(const char* input, int type, in_port_t defaultport,
   hostport* get_all_hostports(const char* input, int type,
      in_port_t defaultport);
   void free_hostport_list(struct hostport* hp);

   bool get_hostport_of_peer(int socket, hostport* hp);

   bool print_sockaddr(outbuf* out, struct sockaddr* addr, socklen_t namelen);
   bool print_hostport(outbuf* out, hostport* hp);

=head1 DESCRIPTION

I<get_hostport> and I<get_all_hostports>
support the textual specification of hosts,
either in domain style or as numerical IP address, and an
optional port number. The term hostport was coined within
RFC 2396 which provides the generic syntax for Uniform
Resource Identifiers (URI). This syntax was extended by RFC
2732 to support IPv6 addresses which were already specified
in RFC 2373.

Following syntax is supported:

   hostport        = host [ ":" port ]
   host            = hostname | IPv4address | IPv6reference
   hostname        = { domainlabel "." } toplabel [ "." ]
   domainlabel     = alphanum | alphanum { alphanum  |  "-"  }
		     alphanum
   toplabel        = alpha | alpha { alphanum | "-" } alphanum
   IPv4address     = { digit } "."  { digit } "." { digit } "." { digit }
   IPv6reference   = '[' IPv6address ']'
   IPv6address     = hexpart [ ":" IPv4address ]
   hexpart         = hexseq | hexseq "::" [ hexseq ] | "::" [ hexseq ]
   hexseq          = { hexdigit } | hexseq ":" { hexdigit }

I<get_hostport> and I<get_all_hostports> expect
in I<input> a string that conforms to
the given syntax and returns, in case of success, a I<hostport>
structure that can be used for subsequent calls of I<socket> and
I<bind> or I<connect>. The socket type can be specified
in I<type>, e.g. by chosing I<SOCK_STREAM> or I<SOCK_DGRAM>.
A zero value is permitted if the socket type remains unspecified.
A default port can be specified using I<defaultport>. This port
is taken if no port is specified within I<input>.

In addition, a hostport specification is permitted that begins
with '/' or '.'. This is then considered to be the path of a UNIX
domain socket.

A request can possibly deliver more than one result.
I<get_hostport>, if successful, stores the first result into I<hp>.
Alternatively, I<get_all_hostports> can be used to obtain
all results in the form of linear linked list, chained through
the I<next> member of the I<hostport> data structure. Such a
list can be free'd using I<free_hostport_list>.

I<get_hostport_of_peer> may be called for connected sockets
and returns, if successful, the peer's address of I<fd>
in I<*hp>. I<print_sockaddr> allows a socket address to be
printed (numerically) to I<out>. I<print_hostport> calls
I<print_sockaddr> with the socket address stored in I<hp>.

=head1 DIAGNOSTICS

All functions returns I<true> in case of success, and
I<false> otherwise.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <stralloc.h>
#include <string.h>
#include <sys/un.h>
#include <afblib/hostport.h>
#include <afblib/outbuf_printf.h>

typedef struct inbuf {
   const char* buf;
   size_t len;
   size_t pos;
} inbuf;

typedef struct host {
   enum {IPv4, IPv6, HOSTNAME} variant;
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
   bool ipv6_reference = false;
   bool colon_seen = false;
   bool double_colon_seen = false;
   int colon_count = 0;
   int digits = 0;
   int dots = 0;
   int last_ipv6_digits = 0;
   int ch = inbuf_getchar(ibuf);
   if (ch == '[') { /* per RFC 2732, section 3, IPv6reference */
      ipv6_reference = true;
      ch = inbuf_getchar(ibuf);
   }
   while (ch >= 0) {
      if (ch == ':' && !ipv6_reference) break;
      if (ch == ']' && ipv6_reference) {
	 ch = inbuf_getchar(ibuf);
	 break;
      }
      stralloc_readyplus(&h->text, 1);
      h->text.s[h->text.len++] = ch;
      if (ipv6_reference && ch == ':') {
	 if (last_ipv6_digits > 4) {
	    return false;
	 }
	 last_ipv6_digits = 0;
	 valid_dotted_decimal = false;
	 if (colon_seen) {
	    if (double_colon_seen) {
	       return false;
	    }
	    double_colon_seen = true;
	 }
	 colon_seen = true;
      } else {
	 if (colon_seen) {
	    ++colon_count;
	 }
	 colon_seen = false;
	 if (ipv6_reference &&
	       (ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F')) {
	    ++last_ipv6_digits;
	 } else if (ch >= 'a' && ch <= 'z' ||
	       ch >= 'A' && ch <= 'Z' || ch == '-') {
	    valid_dotted_decimal = false;
	    if (ipv6_reference) {
	       return false;
	    }
	 } else if (ch >= '0' && ch <= '9') {
	    ++digits;
	    ++last_ipv6_digits;
	 } else if (ch == '.') {
	    ++dots;
	    if (digits == 0 || dots > 3) {
	       valid_dotted_decimal = false;
	    }
	    if (ipv6_reference && !colon_count && !double_colon_seen) {
	       return false;
	    }
	 } else {
	    return false;
	 }
      }
      ch = inbuf_getchar(ibuf);
   }
   if (ch > 0) inbuf_back(ibuf);
   if (ipv6_reference) {
      h->variant = IPv6;
   } else {
      valid_dotted_decimal = valid_dotted_decimal && dots == 3 && digits > 0;
      if (valid_dotted_decimal) {
	 h->variant = IPv4;
      } else {
	 h->variant = HOSTNAME;
      }
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

bool check_for_unix_domain_socket(const char* input, int type,
      hostport* hp) {
   if (input[0] == '/' || input[0] == '.') {
      /* special case: UNIX domain socket */
      *hp = (hostport) {
	 .domain = PF_UNIX,
	 .type = type,
	 .protocol = 0,
	 .namelen = sizeof(struct sockaddr_un),
      };
      struct sockaddr_un* sp = (struct sockaddr_un*) &hp->addr;
      sp->sun_family = AF_UNIX;
      size_t pathlen = sizeof sp->sun_path;
      /* make sure that sun_path is 0-terminated as
	 some systems like Linux require this */
      sp->sun_path[pathlen-1] = 0;
      strncpy(sp->sun_path, input, pathlen-1);
      return true;
   }
   return false;
}

struct addrinfo* get_addrinfo_results(const char* input, int type,
      in_port_t defaultport) {
   inbuf ibuf = {input, strlen(input), 0};
   host h = {0};
   if (!parse_host(&ibuf, &h)) {
      stralloc_free(&h.text); return 0;
   }
   stralloc_0(&h.text); /* needed by inet_addr, inet_pton, and gethostbyname */
   in_port_t port;
   if (parse_delimiter(&ibuf, ':')) {
      if (!parse_port(&ibuf, &port)) {
	 stralloc_free(&h.text); return 0;
      }
   } else {
      port = defaultport;
   }

   struct addrinfo* aip = 0;
   struct addrinfo hints = {
      .ai_socktype = type,
   };
   switch (h.variant) {
      case IPv4:
	 hints.ai_family = AF_INET;
	 break;
      case IPv6:
	 hints.ai_family = AF_INET6;
      case HOSTNAME:
	 hints.ai_family = AF_UNSPEC;
	 hints.ai_flags = AI_ADDRCONFIG;
	 break;
   }

   char* service = 0;
   stralloc service_sa = {0};
   if (port) {
      stralloc_catlong0(&service_sa, port, 1);
      stralloc_0(&service_sa);
      service = service_sa.s;
   }
   if (getaddrinfo(h.text.s, service, &hints, &aip) || !aip) {
      aip = 0;
   }
   stralloc_free(&service_sa);
   stralloc_free(&h.text);
   return aip;
}

static void convert_ai_to_hp(struct addrinfo* aip, hostport* hp) {
   *hp = (hostport) {
      .domain = aip->ai_family,
      .type = aip->ai_socktype,
      .protocol = aip->ai_protocol,
      .namelen = aip->ai_addrlen,
      .next = 0,
   };
   memcpy(&hp->addr, aip->ai_addr, aip->ai_addrlen);
}

bool get_hostport(const char* input, int type, in_port_t defaultport,
      hostport* hp) {
   if (check_for_unix_domain_socket(input, type, hp)) {
      return true;
   }
   struct addrinfo* aip = get_addrinfo_results(input, type, defaultport);
   if (!aip) return false;
   convert_ai_to_hp(aip, hp);
   freeaddrinfo(aip);
   return true;
}

hostport* get_all_hostports(const char* input, int type,
      in_port_t defaultport) {
   hostport hp;
   hostport* head = 0;
   hostport* tail = 0;
   if (check_for_unix_domain_socket(input, type, &hp)) {
      head = malloc(sizeof(hostport));
      *head = hp;
   } else {
      struct addrinfo* aip = get_addrinfo_results(input, type, defaultport);
      for (struct addrinfo* res = aip; res; res = res->ai_next) {
	 hostport* hpres = malloc(sizeof(hostport));
	 convert_ai_to_hp(res, hpres);
	 if (tail) {
	    tail->next = hpres;
	 } else {
	    head = hpres;
	 }
	 tail = hpres;
      }
      freeaddrinfo(aip);
   }
   return head;
}

void free_hostport_list(struct hostport* hp) {
   while (hp) {
      hostport* member = hp;
      hp = hp->next;
      free(member);
   }
}

bool get_hostport_of_peer(int socket, hostport* hp) {
   struct sockaddr_storage addr;
   socklen_t namelen = sizeof(struct sockaddr_storage);
   if (getpeername(socket, (struct sockaddr*) &addr, &namelen) < 0) {
      return false;
   }
   int domain;
   switch (addr.ss_family) {
      case AF_INET:
	 domain = PF_INET; break;
      case AF_INET6:
	 domain = PF_INET6; break;
      case AF_UNIX:
	 domain = PF_UNIX; break;
      default:
	 return false;
   }
   *hp = (hostport) {
      .domain = domain,
      .addr = addr,
      .namelen = namelen,
   };
   return true;
}

bool print_sockaddr(outbuf* out, struct sockaddr* addr, socklen_t namelen) {
   switch (addr->sa_family) {
      case AF_INET:
	 {
	    if (namelen < sizeof(struct sockaddr_in)) return false;
	    char buf[INET_ADDRSTRLEN + 1];
	    struct sockaddr_in* ap = (struct sockaddr_in*) addr;
	    inet_ntop(AF_INET, &ap->sin_addr, buf, sizeof buf);
	    return outbuf_printf(out, "%s:%hu", buf, ntohs(ap->sin_port)) >= 0;
	 }
      case AF_INET6:
	 {
	    if (namelen < sizeof(struct sockaddr_in6)) return false;
	    char buf[INET6_ADDRSTRLEN + 1];
	    struct sockaddr_in6* ap = (struct sockaddr_in6*) addr;
	    inet_ntop(AF_INET6, &ap->sin6_addr, buf, sizeof buf);
	    return outbuf_printf(out, "[%s]:%hu", buf,
	       ntohs(ap->sin6_port)) >= 0;
	 }
      case AF_UNIX:
	 {
	    if (namelen < sizeof(struct sockaddr_un)) return false;
	    struct sockaddr_un* ap = (struct sockaddr_un*) &addr;
	    return outbuf_printf(out, "%s", ap->sun_path) >= 0;
	 }
      default:
	 return false;
   }
}

bool print_hostport(outbuf* out, hostport* hp) {
   return print_sockaddr(out, (struct sockaddr*) &hp->addr, hp->namelen);
}
