/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2013, 2015 Andreas Franz Borchert
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

inbuf_scan -- regular-expression-based input parsing

=head1 SYNOPSIS

   #include <afblib/inbuf_scan.h>

   int inbuf_scan(inbuf* ibuf, const char* regexp, ...);

   typedef struct {
      const char* captured;
      size_t captured_len;
      int callout_number;
   } inbuf_scan_callout_block;

   typedef int (*inbuf_scan_callout_function)(inbuf_scan_callout_block*,
	 void* callout_data);

   int inbuf_scan_with_callouts(inbuf* ibuf, const char* regexp,
	 inbuf_scan_callout_function callout, void* callout_data);

=head1 DESCRIPTION

I<inbuf_scan> allows to scan multiple items from I<ibuf> on base of
the regular expression I<regexp>. The syntax of the regular expression
is expected to conform to those of Perl as the Perl Compatible Regular
Expression Library (libpxre) is used (-lpcre is required for linkage).
For each capturing sub-expression a pointer to an initialized
I<stralloc> object has to be passed. The behaviour is undefined if too
few parameters are given.

If I<n> capturing sub-expressions are given in the regular expression,
then I<inbuf_scan> returns in case of a successful match a value I<m>
<= I<n> which gives the actual number of captures. The first I<m>
stralloc objects whose pointers have been passed behind I<regexp> are
then filled with the contents of the individual captures.  If the
copy-out of a capture is to be suppressed, a null pointer may be given
instead.  If a capturing sub-expression matches repeatedly, the last
capture is stored into the corresponding stralloc object.

While usually regular expressions are used on already existing input
stored into some string, I<inbuf_scan> automatically requests more
input when the regular expression matches the current buffer contents
just partially. (See the example below that returns the last input
line.)

The regular expression is considered to be anchored, i.e. it must match
the input right from the beginning.  If, for example, there is possibly
leading whitespace, "C<\s*>" should be at the beginning of the
regular expression.

The regular expression and the input can span multiple lines. "C<.>"
does not match newlines as usual, but "C<^>" and "C<$>" match the
beginning or the end of an input line. Note that "C<\s>" matches all
whitespace characters including line terminators.  If trailing
whitespace and a newline are to be removed from the input, do it
non-greedily, i.e.  use "C<\s*?\n>" instead of a simple "C<\s*\n>"
which would remove all subsequent empty lines.

I<inbuf_scan_with_callouts> provides an alternative interface that
allows to take advantage of the callout feature. The callout function
is called whenever "C<(?C)>" constructs are matched within the regular
expression. This callout feature supports also an optional decimal
non-negative integer as in "C<(?C7)>".

Callout functions allow to record all occurrences of a repeating
capture. Where I<inbuf_scan> just returns the last capture,
I<inbuf_scan_with_callouts> allows to see all captures. Consider
following regular expressions that captures all colon-separated
fields within an input line:

   (([^:\n]*)(?C))(:([^:\n]*)(?C))*\n

In this case, I<inbuf_scan> delivers two fields at maximum
(i.e. the first and the last field). The callout handler
of I<inbuf_scan_with_callouts> gets called for each capture.
The callout handler gets two parameters where the first
points to following callout block:

=over 4

=item C<const char* captured>

Points to the beginning of the last capture. Is 0 if there
was no capture yet. Note that the string is not nullbyte-terminated.

Be aware that the pointer is no longer valid when
I<inbuf_scan_with_callouts> returns. Hence, the
captured content must be saved somewhere if it is to
be used after the invocation of I<inbuf_scan_with_callouts>.

=item C<size_t captured_len>

Gives the length of the capture in bytes. Note that
there is no nullbyte-termination and that nullbytes
are permitted within the input.

=item C<int callout_number>

Tells the callout number. By default this is 0 but if,
for example, "C<(?C7)>" would have been given, it would be 7.

=back

The second parameter to the callout function is the last
parameter which has been passed to I<inbuf_scan_with_callouts>.

=head1 DIAGNOSTICS

On success, I<inbuf_scan> returns the number of captures.

Otherwise, if the input does not match or if the
regular expression is invalid, -1 is returned.
How much from the input got consumed in case of a non-matching
regular expression depends on how often the input buffer had
to be refilled. Any consumed buffer fillings are lost but
the most recent buffer filling is left untouched.

This ambiguity can be avoided by making trailing parts optional. For
example, instead of

   "\\s*(\\d+)\\s*(\\d+)"

to capture two integers it is better to use

   "\\s*(\\d+)\\s*(\\d+)?"

and then to see if the return value is 1 (one integer read)
or 2 (both integers read). If -1 is returned, some leading
whitespace is possibly lost but nothing else.

I<inbuf_scan_with_callouts> operates similar but returns
the sum of the integer values returned by the callout function.
If any of the callout functions calls returns -1, further
processing is aborted and -1 returned.

=head1 EXAMPLES

Read an input line and discard the line terminator:

   stralloc line = {0};
   int count = inbuf_scan(&ibuf, "(.*)\n", &line);

Like before but discarding leading and trailing whitespace:

   int count = inbuf_scan(&ibuf, "[ \t]*(.*?)\\s*?\n", &line);

Read an arbitrary number of colon-separated fields until
and including the line terminator:

   stralloc field = {0}; int count;
   do {
      count = inbuf_scan(&ibuf, "([^:\n]*)(:)?", &field, 0);
      if (count >= 1) {
	 // process field
      }
   } while (count == 2);
   if (count == 1) inbuf_scan(&ibuf, "\n");

Collect all colon-separated fields in a list using F<inbuf_scan_with_callouts>:

   void collect_field(inbuf_scan_callout_block* block, void* data) {
      strlist* list = (strlist*) data;
      if (block->captured) {
	 size_t len = block->captured_len;
	 char* s = malloc(len + 1);
	 if (s) {
	    memcpy(s, block->captured, len); s[len] = 0;
	    strlist_push(list, s);
	 }
      }
   }

   // ...
   strlist captures = {0};
   int count = inbuf_scan_with_callouts(&ibuf,
      "(([^:\n]*)(?C))(:([^:\n]*)(?C))*\n",
      handler, &captures);

Read and convert decimal floating point values from
an input buffer:

   bool scan_double(inbuf* ibuf, double* val) {
      const char fpre[] =
	 "\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)";
      stralloc sa = {0};
      bool ok = inbuf_scan(ibuf, fpre, &sa) == 1 &&
	        stralloc_0(&sa) && sscanf(sa.s, "%lg", val) == 1;
      stralloc_free(&sa);
      return ok;
   }

Read the last line from the input:

   stralloc line = {0};
   int count = inbuf_scan(&ibuf, "(?:.*\n)*(.*)\n", &line);

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <assert.h>
#include <pcre.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stralloc.h>
#include <afblib/inbuf_scan.h>

struct pcre_handle;
typedef int (*pcre_callout_function)(pcre_callout_block*);
typedef void (*reset_callouts_function)(struct pcre_handle*);

struct callout_block_list {
   inbuf_scan_callout_block block;
   struct callout_block_list* next;
};

struct pcre_handle {
   inbuf* ibuf;
   stralloc input; /* input buffer, feeded from ibuf */
   pcre* compiled; /* result of pcre_compile */
   pcre_extra* extra; /* result of pcre_study, may be 0 */
   bool jit; /* if true, we have JIT support and want to use it */
   reset_callouts_function reset_callouts; /* may be 0 */
   pcre_callout_function callout; /* may be 0 */
   void* callout_handle; /* passed to callout function */
   int capture_count;
   int ovecsize;
   int* ovector;
   /* used by inbuf_scan_with_callouts */
   inbuf_scan_callout_function external_callout;
   void* callout_data;
   struct callout_block_list* head;
   struct callout_block_list* tail;
};

static void pcre_handle_free_block_list(struct pcre_handle* handle) {
   struct callout_block_list* p = handle->head;
   handle->head = 0; handle->tail = 0;
   while (p) {
      struct callout_block_list* old = p; p = p->next;
      free(old);
   }
}

/* release all data structures associated with handle */
static void pcre_handle_free(struct pcre_handle* handle) {
   stralloc_free(&handle->input);
   if (handle->extra) {
      pcre_free_study(handle->extra);
   }
   pcre_free(handle->compiled);
   if (handle->ovector) {
      free(handle->ovector);
   }
   pcre_handle_free_block_list(handle);
}

/* internal wrapper of pcre_compile */
static bool inbuf_scan_prepare(const char* regexp, int options,
      struct pcre_handle* handle) {
   const char* errormsg; int errpos; // unused
   pcre* compiled = pcre_compile(regexp, options, &errormsg, &errpos, 0);
   if (!compiled) {
      /* parsing of regular expression failed */
      return false;
   }
   int capture_count = 0;
   if (pcre_fullinfo(compiled, 0, PCRE_INFO_CAPTURECOUNT, &capture_count)) {
      pcre_free(compiled);
      return false;
   }
   int ovecsize = (capture_count + 1) << 2;
   int* ovector = calloc(ovecsize, sizeof(int));

   handle->compiled = compiled;
   handle->extra = 0;
   handle->capture_count = capture_count;
   handle->ovecsize = ovecsize;
   handle->ovector = ovector;
   return true;
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

static bool inbuf_scan_study(int options, struct pcre_handle* handle) {
   const char* errptr = 0;
#ifdef PCRE_STUDY_EXTRA_NEEDED
   if (handle->callout_handle) {
      options |= PCRE_STUDY_EXTRA_NEEDED;
   }
#endif
#ifdef PCRE_STUDY_JIT_PARTIAL_HARD_COMPILE
   if (have_jit_support()) {
      options |= PCRE_STUDY_JIT_PARTIAL_HARD_COMPILE;
   }
#endif
   pcre_extra* extra = pcre_study(handle->compiled, options, &errptr);
#ifndef PCRE_STUDY_EXTRA_NEEDED
   if (!extra && errptr == 0 && handle->callout_handle) {
      /* we need to setup or own extra block */
      extra = calloc(1, sizeof(pcre_extra));
   }
#endif
   if (extra && errptr == 0) {
      handle->extra = extra;
#ifdef PCRE_STUDY_JIT_PARTIAL_HARD_COMPILE
      handle->jit = have_jit_support();
#else
      handle->jit = false;
#endif
      if (handle->callout_handle) {
	 extra->callout_data = handle->callout_handle;
	 extra->flags |= PCRE_EXTRA_CALLOUT_DATA;
      }
      return true;
   } else {
      return false;
   }
}

/* internal function that calls pcre_exec with
   the parameters configured in the given handle;
   this function handles the repeated calls to pcre_exec
   while we have just partial matches */
static int inbuf_scan_exec(struct pcre_handle* handle) {
   inbuf* ibuf = handle->ibuf;
   stralloc* input = &handle->input;
   int count = -1; /* return value */
   unsigned int offset = 0;
   input->len = 0;
   bool eof = false;
   int options;
   bool first_run = true;
   for(;;) {
      /* make sure that the buffer is filled (again) */
      if (ibuf->pos >= ibuf->buf.len) {
	 if (inbuf_getchar(ibuf) >= 0) inbuf_back(ibuf);
      }
      /* see how much is left to be read in the buffer */
      unsigned int left = ibuf->buf.len - ibuf->pos;
      if (left == 0) {
	 eof = true;
      }
      /* add it to our input buffer */
      if (!eof && !stralloc_catb(input, ibuf->buf.s + ibuf->pos, left)) {
	 break;
      }

      options = PCRE_BSR_ANYCRLF;
      if (!eof) options |= PCRE_PARTIAL_HARD | PCRE_NOTEOL;

      if (first_run) {
	 first_run = false;
      } else {
	 /* mark previous callouts as obsolete */
	 if (handle->reset_callouts) {
	    handle->reset_callouts(handle);
	 }
      }

      pcre_callout_function previous_callout = pcre_callout;
      if (handle->callout) {
	 pcre_callout = handle->callout;
      }
      int rval = pcre_exec(handle->compiled, handle->extra,
	    input->s, input->len, 0, options,
	    handle->ovector, handle->ovecsize);
      if (handle->callout) {
	 pcre_callout = previous_callout;
      }

      if (rval >= 0) {
	 /* we are done, no more input is required;
	    rval gives us the number of pairs in ovector
	    including the first pair for the entire matched string
	 */
	 count = rval - 1;
	 int pos = handle->ovector[1];
	 assert(pos >= offset && pos - offset <= left);
	 ibuf->pos += pos - offset;
	 break;
      }
      if (rval != PCRE_ERROR_PARTIAL) {
	 /* pattern match failed or some other problem */
	 break;
      }
      /* considering the entire inbuf contents as consumed */
      offset += left;
      ibuf->pos = ibuf->buf.len;
   }

   return count;
}

int inbuf_scan(inbuf* ibuf, const char* regexp, ...) {
   struct pcre_handle handle = {.ibuf = ibuf};
   if (!inbuf_scan_prepare(regexp, PCRE_ANCHORED | PCRE_MULTILINE, &handle)) {
      /* parsing of regular expression failed */
      return -1;
   }
   int count = inbuf_scan_exec(&handle);

   if (count >= 0) {
      assert(2*count + 1 < handle.ovecsize);
      va_list ap;
      va_start(ap, regexp);
      for (int i = 1; i <= count; ++i) {
	 stralloc* sa = va_arg(ap, stralloc*);
	 if (sa) {
	    int start = handle.ovector[2*i];
	    int len = handle.ovector[2*i+1] - handle.ovector[2*i];
	    if (start == -1) {
	       /* no capture: return an empty string */
	       sa->len = 0;
	    } else {
	       /* extract captured substring */
	       assert(start + len <= handle.input.len);
	       if (!stralloc_copyb(sa, handle.input.s + start, len)) {
		  count = -1; break;
	       }
	    }
	 }
      }
      va_end(ap);
   }

   pcre_handle_free(&handle);
   return count;
}

/* pcre_callout handler with is configured by inbuf_scan_with_callouts
   and called by pcre_exec */
static int pcre_callout_handler(pcre_callout_block* block) {
   if (!block || !block->callout_data) return 0;
   struct pcre_handle* handle = (struct pcre_handle*) block->callout_data;
   if (!handle->external_callout) return 0;
   const char* captured = 0;
   size_t captured_len = 0;
   if (block->capture_last >= 0) {
      int i = block->capture_last * 2;
      captured = handle->input.s + block->offset_vector[i];
      captured_len = block->offset_vector[i+1] - block->offset_vector[i];
   }

   /* notice all the callout parameters of relevance
      but do not call the actual callout handler yet
      as this might be preliminary in case of a partial
      match */
   struct callout_block_list* element =
      malloc(sizeof(struct callout_block_list));
   if (element == 0) return -1; /* abort it due to lack of memory */
   element->block = (inbuf_scan_callout_block) {
      .captured = captured,
      .captured_len = captured_len,
      .callout_number = block->callout_number,
   };
   element->next = 0;
   if (handle->tail) {
      handle->tail->next = element;
   } else {
      handle->head = element;
   }
   handle->tail = element;
   return 0;
}

static void reset_handler(struct pcre_handle* handle) {
   pcre_handle_free_block_list(handle);
}

int inbuf_scan_with_callouts(inbuf* ibuf, const char* regexp,
      inbuf_scan_callout_function callout, void* callout_data) {
   struct pcre_handle handle = {
      .ibuf = ibuf,
      .callout = pcre_callout_handler,
      .external_callout = callout,
      .callout_data = callout_data,
      .reset_callouts = reset_handler,
   };
   handle.callout_handle = &handle;
   if (!inbuf_scan_prepare(regexp, PCRE_ANCHORED | PCRE_MULTILINE, &handle) ||
	 !inbuf_scan_study(0, &handle)) {
      /* parsing of regular expression failed */
      return -1;
   }
   int count = inbuf_scan_exec(&handle);

   if (count >= 0) {
      /* process delayed callouts */
      count = 0;
      for (struct callout_block_list* p = handle.head; p; p = p->next) {
	 int rval = callout(&p->block, callout_data);
	 if (rval < 0) {
	    count = -1; break;
	 }
	 count += rval;
      }
   }

   pcre_handle_free(&handle);
   return count;
}
