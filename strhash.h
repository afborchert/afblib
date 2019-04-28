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
 * Hashes for strings, i.e. keys and values are of type char*
 * afb 6/2003
 */

#ifndef AFBLIB_STRHASH_H
#define AFBLIB_STRHASH_H

typedef struct strhash_entry {
   char* key;
   char* value;
   struct strhash_entry* next;
} strhash_entry;

typedef struct strhash {
   unsigned int size, length;
   strhash_entry** bucket; /* hash table */
   unsigned int it_index;
   strhash_entry* it_entry;
} strhash;

/* allocate a hash table with the given bucket size */
int strhash_alloc(strhash* hash, unsigned int size);

/* add tuple (key,value) to the hash, key must be unique */
int strhash_add(strhash* hash, char* key, char* value);

/* remove tuple with the given key from the hash */
int strhash_remove(strhash* hash, char* key);

/* return number of elements */
unsigned int strhash_length(strhash* hash);

/* check existance of a key */
int strhash_exists(strhash* hash, char* key);

/* lookup value by key */
int strhash_lookup(strhash* hash, char* key, char** value);

/* start iterator */
int strhash_start(strhash* hash);

/* fetch next key from iterator; returns 0 on end */
int strhash_next(strhash* hash, char** key);

/* free allocated memory */
int strhash_free(strhash* hash);

#endif
