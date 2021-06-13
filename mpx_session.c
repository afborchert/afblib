/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2013, 2014, 2021 Andreas Franz Borchert
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

mpx_session -- run a multiplexed network service on a given port

=head1 SYNOPSIS

   #include <afblib/mpx_session.h>

   typedef struct session {
      void* handle;
      void* global_handle;
      const char* request;
      size_t request_len;
   } session;

   typedef void (*mpx_handler)(session* s);

   void run_mpx_service(hostport* hp, const char* regexp,
      mpx_handler ohandler, mpx_handler rhandler, mpx_handler hhandler,
      void* global_handle);

   int mpx_session_scan(session* s, ...);
   int mpx_session_printf(session* s, const char* restrict format, ...);
   int mpx_session_vprintf(session* s, const char* restrict format, va_list ap);
   void close_session(session* s);

=head1 DESCRIPTION

I<run_mpx_service> creates a socket that listens on the given hostport,
accepts all incoming connections, invokes I<ohandler>, if non-null,
for each incoming connection, I<rhandler> for
every input record that matches I<regexp> (see L<pcrepattern>),
and I<hhandler>, if non-null, whenever a network connection terminates.

Request handlers get a session reference with following fields:

=over 4

=item I<handle>

This field is initially null and may be freely used to maintain
a pointer to a custom session object.

=item I<global_handle>

This field depends on the I<global_handle> parameter passed
to I<run_mpx_service>.

=item I<request>

This is a pointer that points to the next input record. This
sequence is not nullbyte-terminated.

=item I<request_len>

Gives the length of the next input record in bytes.

=back

If capturing subpatterns have been given in the I<regexp> parameter
to I<run_mpx_service>, I<mpx_session_scan> can be used within
the I<rhandler> (and only there) to extract these captures into
stralloc objects. The number of capturing subpatterns must match
the number of additional C<stralloc*> paramters.

I<mpx_session_printf> works like I<printf> but sends the output
non-blocking to the network connection associated with I<s>.
I<mpx_session_vprintf> provides an alternative interface
that supports variadic argument lists that are already present
as I<va_list> reference.

I<close_session> may be called to initiate the shutdown 
of a session, i.e. no further input packets will be processed,
just the pending response packets will be taken care of.

I<run_mpx_service> runs normally infinitely and returns in
error cases only.

=head1 EXAMPLE

Following example implements a network service that allows
to increment whole numbers. Usually, every session has its
own number that gets incremented. But, by adding the keyword
B<global> to an increment, a global counter that is shared
over all sessions gets incremented.

   #include <afblib/hostport.h>
   #include <afblib/mpx_session.h>
   #include <assert.h>
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>

   struct global_status {
      int counter; // global counter
   };

   struct session_status {
      int counter; // per-session counter;
      stralloc cmd; // request command
      stralloc param; // request parameter
   };

   void ohandler(session* s) {
      struct session_status* sp = calloc(1, sizeof(struct session_status));
      if (!sp) {
	 close_session(s); return;
      }
      s->handle = sp;
   }

   void rhandler(session* s) {
      struct global_status* gs = s->global_handle;
      struct session_status* sp = s->handle;

      if (mpx_session_scan(s, &sp->cmd, &sp->param) != 2) {
	 close_session(s); return;
      }
      int* counterp;
      if (stralloc_diffs(&sp->cmd, "global") == 0) {
	 counterp = &gs->counter; // global counter
      } else {
	 counterp = &sp->counter; // session counter
      }
      stralloc_0(&sp->param); int incr_value = atoi(sp->param.s);
      *counterp += incr_value;
      mpx_session_printf(s, "%d\r\n", *counterp);
   }

   void hhandler(session* s) {
      struct session_status* sp = s->handle;
      stralloc_free(&sp->cmd); stralloc_free(&sp->param); free(sp);
   }

   int main(int argc, char** argv) {
      char* cmdname = *argv++; --argc;
      if (argc != 1) {
	 fprintf(stderr, "Usage: %s hostport\n", cmdname); exit(1);
      }
      char* hostport_string = *argv++; --argc;
      hostport hp;
      if (!get_hostport(hostport_string, SOCK_STREAM, 33013, &hp)) {
	 fprintf(stderr, "%s: hostport expected\n", cmdname); exit(1);
      }

      struct global_status gs = {0};
      run_mpx_service(&hp, "(?:(global) )?(-?\\d+)\r\n",
	 ohandler, rhandler, hhandler, &gs);
   }

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <assert.h>
#include <pcre.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <afblib/mpx_session.h>
#include <afblib/multiplexor.h>
#include <afblib/sliding_buffer.h>

/* global data structure which is passed through the mpx_handle pointer */
typedef struct mpx_service {
   void* global_handle; /* last parameter of run_mpx_service */
   /* handlers */
   mpx_handler ohandler;
   mpx_handler rhandler;
   mpx_handler hhandler;
   /* common pcre stuff */
   pcre* compiled;
   pcre_extra* extra;
   int capture_count;
   int ovecsize;
} mpx_service;

void mpx_open_handler(connection* link) {
   mpx_service* mpxs = (mpx_service*) link->mpx_handle;
   assert(mpxs);
   session* newsession = malloc(sizeof(session));
   if (newsession == 0) {
      close_link(link); return;
   }
   int* ovector = calloc(mpxs->ovecsize, sizeof(int));
   if (ovector == 0) {
      free(newsession); close_link(link); return;
   }
   *newsession = (session) {
      .ovector = ovector,
      .ovecsize = mpxs->ovecsize,
      .global_handle = mpxs->global_handle,
   };
   link->handle = (void*) newsession;
   newsession->link = link;
   if (mpxs->ohandler) (*mpxs->ohandler)(newsession);
}

static void mpx_input_handler(connection* link) {
   assert(link->handle);
   session* s = (session*) link->handle;
   mpx_service* mpxs = (mpx_service*) link->mpx_handle;

   /* read next input packet */
   if (!sliding_buffer_ready(&s->buffer, 2048)) {
      close_link(link); return;
   }
   ssize_t nbytes = read_from_link(link,
      s->buffer.sa.s + s->buffer.sa.len, s->buffer.sa.a - s->buffer.sa.len);
   if (nbytes > 0) s->buffer.sa.len += nbytes;

   /* process every complete request found in the current input buffer */
   int options = PCRE_BSR_ANYCRLF;
   if (nbytes > 0) options |= PCRE_PARTIAL_HARD | PCRE_NOTEOL;
   int rval = 0;
   while (s->buffer.offset < s->buffer.sa.len &&
	    (rval = pcre_exec(mpxs->compiled, mpxs->extra,
	       s->buffer.sa.s, s->buffer.sa.len, s->buffer.offset,
	       options, s->ovector, mpxs->ovecsize)) >= 0) {
      s->count = rval - 1;
      int pos = s->ovector[1];
      assert(pos >= s->buffer.offset && pos <= s->buffer.sa.len);
      /* process individual request */
      s->request = s->buffer.sa.s + s->buffer.offset;
      s->request_len = pos - s->buffer.offset;
      s->offset = s->buffer.offset;
      (*mpxs->rhandler)(s);
      /* mark it as processed */
      s->buffer.offset = pos;
   }
   if (rval < 0 && rval != PCRE_ERROR_PARTIAL) {
      /* we run into another problem,
	 like, for example, getting input that is not matched */
      close_link(link); return;
   }
}

static void mpx_close_handler(connection* link) {
   session* s = (session*) link->handle;
   mpx_service* mpxs = (mpx_service*) link->mpx_handle;
   if (s) {
      if (mpxs->hhandler) {
	 (*mpxs->hhandler)(s);
      }
      sliding_buffer_free(&s->buffer);
      free(s->ovector);
      free(s);
   }
}

void close_session(session* s) {
   /* this triggers our close handler */
   close_link(s->link);
}

static bool have_jit_support() {
   static bool initialized = false;
   static bool have_support = false;
   if (!initialized) {
      int support = 0;
      pcre_config(PCRE_CONFIG_JIT, &support);
      have_support = support;
   }
   return have_support;
}

int mpx_session_scan(session* s, ...) {
   assert(2*s->count + 1 < s->ovecsize);
   int count = s->count;
   va_list ap;
   va_start(ap, s);
   for (int i = 1; i <= count; ++i) {
      stralloc* sa = va_arg(ap, stralloc*);
      if (sa) {
	 int start = s->ovector[2*i] - s->offset;
	 if (start == -1) {
	    /* no capture, return an empty string */
	    sa->len = 0;
	 } else {
	    /* capture succeeded, extract captured substring */
	    int len = s->ovector[2*i+1] - s->ovector[2*i];
	    assert(start >= 0 && len <= s->request_len);
	    if (!stralloc_copyb(sa, s->request + start, len)) {
	       count = -1; break;
	    }
	 }
      }
   }
   va_end(ap);
   return count;
}

int mpx_session_vprintf(session* s, const char* restrict format, va_list ap) {
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
	 if (!write_to_link(s->link, buf, nbytes)) {
	    free(buf); nbytes = -1;
	 }
      }
   }
   va_end(ap2);
   return nbytes;
}

int mpx_session_printf(session* s, const char* restrict format, ...) {
   va_list ap;
   va_start(ap, format);
   int nbytes = mpx_session_vprintf(s, format, ap);
   va_end(ap);
   return nbytes;
}

void run_mpx_service(hostport* hp, const char* regexp,
      mpx_handler ohandler, mpx_handler rhandler, mpx_handler hhandler,
      void* global_handle) {
   if (!hp->type) {
      hp->type = SOCK_STREAM;
   }
   int sfd = socket(hp->domain, hp->type, hp->protocol);
   int optval = 1;
   if (sfd < 0 ||
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof optval) < 0 ||
        bind(sfd, (struct sockaddr *) &hp->addr, hp->namelen) < 0 ||
        listen(sfd, SOMAXCONN) < 0) {
      close(sfd);
      return;
   }

   /* prepare regular expression directed input parsing */
   int options = PCRE_ANCHORED | PCRE_MULTILINE;
   const char* errormsg; int errpos; // unused
   pcre* compiled = pcre_compile(regexp, options, &errormsg, &errpos, 0);
   if (!compiled) {
      /* parsing of regular expression failed */
      close(sfd);
      return;
   }
   int capture_count = 0;
   if (pcre_fullinfo(compiled, 0, PCRE_INFO_CAPTURECOUNT, &capture_count)) {
      pcre_free(compiled);
      close(sfd);
      return;
   }
   int ovecsize = (capture_count + 1) << 2;
   options = 0;
#ifdef PCRE_STUDY_JIT_PARTIAL_HARD_COMPILE
   if (have_jit_support()) {
      options |= PCRE_STUDY_JIT_PARTIAL_HARD_COMPILE;
   }
#endif
   const char* errptr = 0;
   pcre_extra* extra = pcre_study(compiled, options, &errptr);

   /* set up our handle */
   mpx_service* mpxs = malloc(sizeof(mpx_service));
   if (mpxs == 0) {
      pcre_free(compiled); close(sfd); free(mpxs);
   }
   *mpxs = (mpx_service) {
      .global_handle = global_handle,
      .ohandler = ohandler,
      .rhandler = rhandler,
      .hhandler = hhandler,
      .compiled = compiled,
      .extra = extra,
      .capture_count = capture_count,
      .ovecsize = ovecsize,
   };

   run_multiplexor(sfd, mpx_open_handler, mpx_input_handler,
      mpx_close_handler, (void*) mpxs);
}
