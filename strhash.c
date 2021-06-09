/*
   Small library of useful utilities
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

strhash -- hash tables for pairs of strings

=head1 SYNOPSIS

   #include <afblib/strhash.h>

   typedef struct strhash_entry {
      char* key;
      char* value;
      struct strhash_entry* next;
   } strhash_entry;

   typedef struct strhash {
      unsigned int size, length;
      strhash_entry** bucket;
      unsigned int it_index;
      strhash_entry* it_entry;
   } strhash;

   int strhash_alloc(strhash* hash, unsigned int size);
   int strhash_add(strhash* hash, char* key, char* value);
   int strhash_remove(strhash* hash, char* key);
   unsigned int strhash_length(strhash* hash);
   int strhash_exists(strhash* hash, char* key);
   int strhash_lookup(strhash* hash, char* key, char** value);
   int strhash_start(strhash* hash);
   int strhash_next(strhash* hash, char** key);
   int strhash_free(strhash* hash);

=head1 DESCRIPTION

Objects of type I<strhash> represent tables of string-valued
key/value-pairs. Individual entries can be looked up by key.
Multiple entries with an identical key are not permitted.

Each I<strhash> object must be initialized first by invoking
I<strhash_alloc> and by specifying the size of the bucket table
which depends on the expected number of to be stored pairs.
Tables can accomodate any number of entries independent from
this size but the performance degrades if the bucket table
is too small.

Entries can be added by I<strhash_add>. Entries with duplicate
keys are, however, not accepted. Existing entries can be
removed by I<strhash_remove>. The number of entries within
the hash table is returned by I<strhash_length>.
I<strhash_exists> allows to test whether a pair with the
given key exists within the table. I<strhash_lookup> returns
the pair with the given key, if existent.

I<strhash_start> and I<strhash_next> allow to iterate through
all entries of a hash table.

I<strhash_free> allows to free any memory associated with
I<hash>.

=head1 DIAGNOSTICS

All functions with the exception of I<strhash_exists>
return 1 in case of success and 0 otherwise.

=head1 AUTHOR

Andreas F. Borchert

(The hash function has been taken from Dan J. Bernstein.)

=cut

*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <afblib/strhash.h>

/* hash algorithm stolen from Dan Bernstein's cdbhash.c */
#define HASHSTART 5381

static int hashadd(int hashval, unsigned char ch) {
   hashval += hashval << 5;
   return hashval ^ ch;
}

static int compute_hash(const char* buf, unsigned int len) {
   int hashval = HASHSTART;
   while (len > 0) {
      hashval = hashadd(hashval, *buf++);
      --len;
   }
   return hashval;
}

static int strhash_find(strhash* hash, char* key, strhash_entry*** entry) {
   assert(hash->size > 0);
   unsigned int index = compute_hash(key, strlen(key)) % hash->size;
   *entry = &hash->bucket[index];
   while (**entry != 0 && strcmp((**entry)->key, key)) {
      *entry = &(**entry)->next;
   }
   return **entry != 0;
}

/* allocate a hash table with the given bucket size */
int strhash_alloc(strhash* hash, unsigned int size) {
   assert(size > 0);
   strhash_entry** bucket = malloc(size * sizeof(strhash_entry*));
   if (bucket == 0) return 0;
   for (int index = 0; index < size; ++index) {
      bucket[index] = 0;
   }
   hash->size = size; hash->bucket = bucket;
   hash->length = 0;
   hash->it_index = 0; hash->it_entry = 0;
   return 1;
}

/* add tuple (key,value) to the hash, key must be unique */
int strhash_add(strhash* hash, char* key, char* value) {
   strhash_entry** prev;
   /* check uniqueness */
   if (strhash_find(hash, key, &prev)) return 0;
   strhash_entry* entry = malloc(sizeof(strhash_entry));
   if (entry == 0) return 0;
   entry->key = key;
   entry->value = value;
   entry->next = 0;
   *prev = entry;
   ++hash->length;
   return 1;
}

/* remove tuple with the given key from the hash */
int strhash_remove(strhash* hash, char* key) {
   strhash_entry** prev;
   if (!strhash_find(hash, key, &prev)) return 0;
   strhash_entry* entry = *prev;
   *prev = entry->next;
   free(entry);
   return 1;
}

/* return number of elements */
unsigned int strhash_length(strhash* hash) {
   return hash->length;
}

/* check existance of a key */
int strhash_exists(strhash* hash, char* key) {
   strhash_entry** entry;
   if (!strhash_find(hash, key, &entry)) return 0;
   return 1;
}

/* lookup value by key */
int strhash_lookup(strhash* hash, char* key, char** value) {
   strhash_entry** entry;
   if (!strhash_find(hash, key, &entry)) return 0;
   *value = (*entry)->value;
   return 1;
}

/* start iterator */
int strhash_start(strhash* hash) {
   hash->it_index = 0; hash->it_entry = 0;
   return 1;
}

/* fetch next key from iterator; returns 0 on end */
int strhash_next(strhash* hash, char** key) {
   /* invariant:
      if hash->it_entry is non-zero, it points to the next entry
         to be returned
      hash->it_index points to the next chain of entries to be visited
   */
   if (hash->it_entry == 0) {
      while (hash->it_index < hash->size &&
            hash->bucket[hash->it_index] == 0) {
         ++hash->it_index;
      }
      if (hash->it_index >= hash->size) return 0;
      hash->it_entry = hash->bucket[hash->it_index];
      ++hash->it_index;
   }
   *key = hash->it_entry->key;
   hash->it_entry = hash->it_entry->next;
   return 1;
}

/* free allocated memory */
int strhash_free(strhash* hash) {
   if (hash->size == 0) return 0;
   for (int index = 0; index < hash->size; ++index) {
      strhash_entry* entry = hash->bucket[index];
      while (entry != 0) {
         strhash_entry* next = entry->next;
         free(entry);
         entry = next;
      }
   }
   free(hash->bucket);
   hash->size = 0;
   return 1;
}
