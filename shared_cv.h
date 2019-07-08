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

#ifndef AFBLIB_SHARED_CV_H
#define AFBLIB_SHARED_CV_H

#include <stdbool.h>
#include <pthread.h>
#include <afblib/shared_mutex.h>

/* support of condition variables variables in shared memory areas
   that are accessed by multiple processes;
   one of the processes should assume ownership
   and invoke shared_condition_variable_create and
   shared_condition_variable_free
   when released at a memory location which is aligned
   according to shared_condition_variable_alignof() and
   where at least shared_condition_variable_sizeof() bytes are reserved;
   all other processes must invoke only the other
   functions (wait, notify_one, notify_all);
   shared_condition_variable_wait() expects a mutex
   created by shared_mutex_create */

typedef pthread_cond_t shared_cv;

bool shared_cv_create(shared_cv* cv);
bool shared_cv_free(shared_cv* cv);

bool shared_cv_wait(shared_cv* cv, shared_mutex* mutex);
bool shared_cv_notify_one(shared_cv* cv);
bool shared_cv_notify_all(shared_cv* cv);

#endif
