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

#ifndef AFBLIB_SHARED_RTS
#define AFBLIB_SHARED_RTS

#include <stdbool.h>
#include <stddef.h>
#include <afblib/shared_domain.h>

bool shared_rts_run(unsigned int nofprocesses,
   size_t bufsize, size_t extra_space_size,
   const char* path, char** argv);

struct shared_domain* shared_rts_init();
void shared_rts_finish(struct shared_domain* sd);

#endif
