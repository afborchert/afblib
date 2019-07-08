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

#ifndef AFBLIB_SHARED_ENV_H
#define AFBLIB_SHARED_ENV_H

#include <stdbool.h>
#include <stddef.h>

struct shared_env {
   char* name;
   unsigned int rank;
};

bool shared_env_store(struct shared_env*, const char* prefix);
bool shared_env_load(struct shared_env*, const char* prefix);

#endif
