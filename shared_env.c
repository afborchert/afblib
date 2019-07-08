/*
   Small library of useful utilities
   Copyright (C) 2019 Andreas Franz Borchert
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

shared_env -- pass shared communication domain parameters by environment

=head1 SYNOPSIS

   #include <afblib/shared_env.h>

   struct shared_env {
      char* name;
      unsigned int rank;
   };

   bool shared_env_store(struct shared_env*, const char* prefix);
   bool shared_env_load(struct shared_env*, const char* prefix);

=head1 DESCRIPTION

I<shared_env_store> and I<shared_env_load> allow to pass the
parameters required for I<shared_connect> (see L<shared_domain>)
through environment parameters.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <afblib/shared_env.h>

static bool store_env_string(const char* prefix, const char* name,
      const char* value) {
   size_t envparamlen = strlen(prefix) + strlen(name) + 2;
   char envparam[envparamlen];
   strncpy(envparam, prefix, envparamlen);
   strncat(envparam, "_", envparamlen);
   strncat(envparam, name, envparamlen);
   return setenv(envparam, value, 1) == 0;
}

static bool store_env_unsigned_int(const char* prefix, const char* name,
      unsigned int value) {
   int len = snprintf(0, 0, "%u", value);
   if (len <= 0) return false;
   char valstr[len + 1];
   snprintf(valstr, sizeof valstr, "%u", value);
   return store_env_string(prefix, name, valstr);
}

static char* load_env_string(const char* prefix, const char* name) {
   size_t envparamlen = strlen(prefix) + strlen(name) + 2;
   char envparam[envparamlen];
   strncpy(envparam, prefix, envparamlen);
   strncat(envparam, "_", envparamlen);
   strncat(envparam, name, envparamlen);
   return getenv(envparam);
}

static bool load_env_unsigned_int(const char* prefix, const char* name,
      unsigned int* valuep) {
   const char* s = load_env_string(prefix, name);
   if (!s) return false;
   errno = 0;
   char* endptr = 0;
   unsigned long value = strtoul(s, &endptr, 0);
   if (errno) return false;
   if (*endptr) return false;
   *valuep = value;
   return true;
}

bool shared_env_store(struct shared_env* params,
      const char* prefix) {
   return
      store_env_string(prefix, "NAME", params->name) &&
      store_env_unsigned_int(prefix, "RANK", params->rank);
}

bool shared_env_load(struct shared_env* params,
      const char* prefix) {
   char* name = load_env_string(prefix, "NAME");
   if (!name) return false;
   unsigned int rank;
   if (!load_env_unsigned_int(prefix, "RANK", &rank)) return false;
   *params = (struct shared_env) {
      .name = name,
      .rank = rank,
   };
   return true;
}
