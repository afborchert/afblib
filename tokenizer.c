/*
   Small library of useful utilities based on the dietlib by fefe
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

tokenizer -- split a 0-terminated stralloc object into a list of strings

=head1 SYNOPSIS

   #include <afblib/tokenizer.h>

   bool tokenizer(stralloc* input, strlist* tokens);

=head1 DESCRIPTION

The function I<tokenizer> accepts a nullbyte-terminated stralloc object
and splits it into whitespace-separated tokens. All characters for
which F<isspace> (see the L<ctype> manpage) returns true are treated
as whitespace. Sequences of whitespaces are considered as one field
separator. All whitespaces within I<input> are replaced by nullbytes.

I<tokens> is initially emptied, and then subsequently filled
with all tokens found. Each of the tokens consists of at least
one non-whitespace character. Hence, the list of tokens will
be empty if I<input> is empty (beside the terminating nullbyte)
or is filled with whitespaces only.

=head1 DIAGNOSTICS

In case of failures, I<tokenizer> returns false. Otherwise the return
value is true.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

/*
 * Simple tokenizer: Take a 0-terminated stralloc object and return a
 * list of pointers in tokens that point to the individual tokens.
 * Whitespace is taken as token-separator and all whitespaces within
 * the input are replaced by null bytes.
 * afb 4/2003
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stralloc.h>
#include <afblib/strlist.h>
#include <afblib/tokenizer.h>

bool tokenizer(stralloc* input, strlist* tokens) {
   char* cp;
   int white = 1;

   strlist_clear(tokens);
   for (cp = input->s; *cp && cp < input->s + input->len; ++cp) {
      if (isspace((int) *cp)) {
         *cp = '\0'; white = 1; continue;
      }
      if (!white) continue;
      white = 0;
      if (!strlist_push(tokens, cp)) return false;
   }
   return true;
}
