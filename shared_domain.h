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

#ifndef AFBLIB_SHARED_DOMAIN_H
#define AFBLIB_SHARED_DOMAIN_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

struct shared_domain* sd_setup(size_t nbytes, unsigned int nofprocesses);
struct shared_domain* sd_setup_with_extra_space(size_t bufsize,
      unsigned int nofprocesses, size_t extra_space_size,
      const sigset_t* sigmask);
struct shared_domain* sd_connect(char* name, unsigned int rank);
void sd_free(struct shared_domain* sd);

unsigned int sd_get_rank(struct shared_domain* sd);
unsigned int sd_get_nofprocesses(struct shared_domain* sd);
char* sd_get_name(struct shared_domain* sd);
size_t sd_get_extra_space_size(struct shared_domain* sd);
void* sd_get_extra_space(struct shared_domain* sd);

bool sd_barrier(struct shared_domain* sd);
bool sd_write(struct shared_domain* sd, unsigned int recipient,
   const void* buf, size_t nbytes);
bool sd_read(struct shared_domain* sd, void* buf, size_t nbytes);

bool sd_shutdown(struct shared_domain* sd);
bool sd_terminating(struct shared_domain* sd);

#endif
