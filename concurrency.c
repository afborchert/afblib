/*
   Small library of useful utilities
   Copyright (C) 2021 Andreas Franz Borchert
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

concurrency -- retrieve hardware concurrency

=head1 SYNOPSIS

   #include <afblib/concurrency.h>

   unsigned get_hardware_concurrency();

=head1 DESCRIPTION

I<get_hardware_concurrency> returns the number of concurrent
threads supported by the current system. This function strives to
return a similar value like I<std::thread::hardware_concurrency()>
in C++. As there is no portable approach to do this, this
function possibly returns 0 if this information is not available.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#if defined(__GLIBC__)
#include <sys/sysinfo.h>
#endif

#include <afblib/concurrency.h>

/* code derived from boost project thread.cpp,
   function thread::hardware_concurrency:
   https://www.boost.org/doc/libs/1_76_0/libs/thread/src/pthread/thread.cpp */

unsigned get_hardware_concurrency() {
   #if defined(PTW32_VERSION) || defined(__hpux)
     /* see https://docstore.mik.ua/manuals/hp-ux/en/B2355-60130/pthread_processor_bind_np.3T.html */
     return pthread_num_processors_np();
   #elif defined(__APPLE__) || defined(__FreeBSD__)
      int count;
      size_t size = sizeof(count);
      return sysctlbyname("hw.ncpu", &count, &size, 0, 0)? 0: count;
   #elif defined(_SC_NPROCESSORS_ONLN)
      /* see https://man7.org/linux/man-pages/man3/sysconf.3.html
	 https://docs.oracle.com/cd/E88353_01/html/E37843/sysconf-3c.html */
      int count = sysconf(_SC_NPROCESSORS_ONLN);
      return (count > 0)? count: 0;
   #elif defined(__GLIBC__)
      /* see https://linux.die.net/man/3/get_nprocs */
      return get_nprocs();
   #else
      return 0;
   #endif
}
