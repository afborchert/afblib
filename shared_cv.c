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

shared_cv -- POSIX condition variables that are shared among multiple processes

=head1 SYNOPSIS

   #include <afblib/shared_cv.h>

   bool shared_cv_create(shared_cv* cv);
   bool shared_cv_free(shared_cv* cv);

   bool shared_cv_wait(shared_cv* cv, shared_mutex* mutex);
   bool shared_cv_notify_one(shared_cv* cv);
   bool shared_cv_notify_all(shared_cv* cv);

=head1 DESCRIPTION

A shared condition variable is one that can be used in a memory
segment that is shared among multiple processes. By default,
POSIX condition variables must not be shared among multiple processes.
Instead the special attribute I<PTHREAD_PROCESS_SHARED> has to
be set to support this setup.

I<shared_cv_create> and I<shared_cv_free> must be called
by one process only, usually the process that configures the
shared memory segment. The other processes must not invoke
any of the other operations as long as the condition variable
has not been created properly with I<shared_cv_create>
and the condition variable must no longer be used once it
has been free'd using I<shared_cv_free>.

The mutex variable that is passed to I<shared_cv_wait> must
be likewise a shared one, created by I<shared_mutex_create>.

I<shared_cv_notify_one> notifies one waiting process, if any,
while I<shared_cv_notify_all> notifies all waiting processes.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <afblib/shared_cv.h>

bool shared_cv_create(shared_cv* cv) {
   pthread_condattr_t condattr;
   pthread_condattr_init(&condattr);
   bool ok = true;
   if (pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED)) {
      ok = false;
   }
   if (ok && pthread_cond_init(cv, &condattr)) {
      ok = false;
   }
   pthread_condattr_destroy(&condattr);
   return ok;
}

bool shared_cv_free(shared_cv* cv) {
   return pthread_cond_destroy(cv) == 0;
}

bool shared_cv_wait(shared_cv* cv, shared_mutex* mutex) {
   return pthread_cond_wait(cv, mutex) == 0;
}

bool shared_cv_notify_one(shared_cv* cv) {
   return pthread_cond_signal(cv) == 0;
}

bool shared_cv_notify_all(shared_cv* cv) {
   return pthread_cond_broadcast(cv) == 0;
}
