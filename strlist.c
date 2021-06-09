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

strlist -- dynamically growing list of strings

=head1 SYNOPSIS

   #include <afblib/strlist.h>

   typedef struct strlist {
      char** list;
      size_t len;
      size_t allocated;
   } strlist;

   bool strlist_ready(strlist* list, size_t len);
   bool strlist_readyplus(strlist* list, size_t len);
   void strlist_clear(strlist* list);
   bool strlist_push(strlist* list, char* string);
   bool strlist_push0(strlist* list);
   void strlist_free(strlist* list);

=head1 DESCRIPTION

Objects of type I<strlist> represent lists of character strings.
Any number of strings may be added to such a list. Already added
list members cannot be removed except by clearing the whole list.
This data structure is mainly useful in building up arguments
lists, e.g. for I<execvp>.

There is no function that creates or initializes a list. Instead
an initializer is to be used:

   strlist argv = {0};

I<strlist> objects grow by themselves. However, if the number of
elements can be estimated or is known, it might be preferably to
use I<strlist_read> to assure that there is space ready allocated
for at least I<len> entries. I<strlist_readyplus> works similar
to I<strlist_ready> but adds implicitly the current number of
list members to I<len>.

I<strlist_clear> empties the entire list. I<strlist_push> adds
the given string to the list. I<strlist_push0> adds a null-pointer
to the list. This is particularly useful in case of null-pointer-terminated
argument or environment lists.

I<strlist_free> frees the allocated memory associated with I<list>.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

/*
 * Data structure for dynamic string lists that works
 * similar to the stralloc library.
 * Return values: 1 if successful, 0 in case of failures.
 * afb 4/2003
 */
#include <stdlib.h>
#include <afblib/strlist.h>

/* assure that there is at least room for len list entries */
bool strlist_ready(strlist* list, size_t len) {
   if (list->allocated < len) {
      size_t wanted = len + (len>>3) + 8;
      char** newlist = (char**) realloc(list->list,
         sizeof(char*) * wanted);
      if (newlist == 0) return false;
      list->list = newlist;
      list->allocated = wanted;
   }
   return true;
}

/* assure that there is room for len additional list entries */
bool strlist_readyplus(strlist* list, size_t len) {
   return strlist_ready(list, list->len + len);
}

/* truncate the list to zero length */
void strlist_clear(strlist* list) {
   list->len = 0;
}

/* append the string pointer to the list */
bool strlist_push(strlist* list, char* string) {
   if (!strlist_ready(list, list->len + 1)) return false;
   list->list[list->len++] = string;
   return true;
}

/* free the strlist data structure but not the strings */
void strlist_free(strlist* list) {
   free(list->list); list->list = 0;
   list->allocated = 0;
   list->len = 0;
}
