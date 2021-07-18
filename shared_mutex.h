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

#ifndef AFBLIB_SHARED_MUTEX_H
#define AFBLIB_SHARED_MUTEX_H

#include <signal.h>
#include <stdbool.h>
#include <pthread.h>

/* support of mutex variables in shared memory areas
   that are accessed by multiple processes;
   one of the processes should assume ownership
   and invoke create_shared_mutex and free_shared_mutex
   when released at a memory location which is aligned
   according to alignof_shared_mutex() and where at least
   sizeof_shared_mutex() bytes are reserved;
   all other processes must invoke only lock_shared_mutex
   and unlock_shared_mutex */

typedef struct {
   pthread_mutex_t mutex;
   sigset_t blocked_sigset;
   sigset_t old_sigset;
   bool block_signals;
} shared_mutex;

bool shared_mutex_create(shared_mutex* mutex);
bool shared_mutex_create_with_sigmask(shared_mutex* mutex,
   const sigset_t* sigmask);
bool shared_mutex_free(shared_mutex* mutex);

bool shared_mutex_lock(shared_mutex* mutex);
bool shared_mutex_unlock(shared_mutex* mutex);

bool shared_mutex_consistent(shared_mutex* mutex);

#endif
