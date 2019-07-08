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

shared_mutex -- POSIX mutex variable that is shared among multiple processes

=head1 SYNOPSIS

   #include <afblib/shared_mutex.h>

   bool shared_mutex_create(shared_mutex* mutex);
   bool shared_mutex_free(shared_mutex* mutex);

   bool shared_mutex_lock(shared_mutex* mutex);
   bool shared_mutex_unlock(shared_mutex* mutex);

=head1 DESCRIPTION

A shared mutex variable is one that can be used in a memory
segment that is shared among multiple processes. By default,
POSIX mutex variables must not be shared among multiple processes.
Instead the special attribute I<PTHREAD_PROCESS_SHARED> has to
be set to support this setup.

I<shared_mutex_create> and I<shared_mutex_free> must be called
by one process only, usually the process that configures the
shared memory segment. The other processes must not invoke
any of the other operations as long as the mutex variable
has not been created properly with I<shared_mutex_create>
and the mutex variable must no longer be used once it
has been free'd using I<shared_mutex_free>.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <pthread.h>
#include <afblib/shared_mutex.h>

bool shared_mutex_create(shared_mutex* mutex) {
   pthread_mutexattr_t mxattr;
   pthread_mutexattr_init(&mxattr);
   bool ok = true;
   if (pthread_mutexattr_setpshared(&mxattr, PTHREAD_PROCESS_SHARED)) {
      ok = false;
   }
   if (ok && pthread_mutex_init(mutex, &mxattr)) {
      ok = false;
   }
   pthread_mutexattr_destroy(&mxattr);
   return ok;
}

bool shared_mutex_free(shared_mutex* mutex) {
   return pthread_mutex_destroy(mutex) == 0;
}

bool shared_mutex_lock(shared_mutex* mutex) {
   return pthread_mutex_lock(mutex) == 0;
}

bool shared_mutex_unlock(shared_mutex* mutex) {
   return pthread_mutex_unlock(mutex) == 0;
}