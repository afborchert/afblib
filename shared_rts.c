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

shared_rts -- runtime system for a shared communication domain

=head1 SYNOPSIS

   #include <afblib/shared_rts.h>

   bool shared_rts_run(unsigned int nofprocesses,
      size_t bufsize, size_t extra_space_size,
      const char* path, char** argv);

   struct shared_domain* shared_rts_init();
   void shared_rts_finish(struct shared_domain* sd);

=head1 DESCRIPTION

This module provides a runtime system for processes that want to
communicate through a shared communication domain.

One process acts as master who creates the shared communication domain,
starts the individual processes that are connected to the communication
domain, and finishes everything as soon as all processes are terminated.

I<shared_rts_run> is to be called by the master process
and configures a shared communication for I<nofprocesses> where
the individual communication buffers have a size of I<bufsize>
bytes and optionally with some extra space in the shared memory
segment of I<extra_space_size> bytes. The I<nofprocesses> worker
processes are started (through I<fork> and I<exec>) where the
parameters of the shared communication
domain are passed through the environment (see L<shared_env>).
I<shared_rts_run> blocks until all child processes are finished.
If one of the child processes aborts or exists with a non-zero exit
code, all other child processes are terminated using signal I<SIGTERM>.

The individual worker processes invoke I<init_sm_rts> at
the beginning to connect to the communication domain and
I<finish_sm_rts> once they no longer need the connection.

=head1 EXAMPLE

This module allows to create a small utility to launch
worker processes that share a communication domain:

   #include <errno.h>
   #include <stdbool.h>
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <afblib/shared_rts.h>

   bool convert_val(const char* str, unsigned long long* resp) {
      char* endptr;
      errno = 0;
      unsigned long long val = strtoull(str, &endptr, 0);
      if (errno) return false;
      if (*endptr) return false;
      *resp = val;
      return true;
   }

   char* cmdname;
   void usage() {
      fprintf(stderr,
         "Usage: %s "
         "[-bufsize bufsize] "
         "[-extra extra_bytes] "
         "[-np nofprocesses] "
         "cmd args...\n",
         cmdname);
      exit(1);
   }

   int main(int argc, char** argv) {
      cmdname = *argv++; --argc;
      unsigned int nofprocesses = 2; size_t bufsize = 1024;
      size_t extra_space_size = 0;
      while (argc > 1 && **argv == '-') {
         if (strcmp(*argv, "-np") == 0) {
            ++argv; --argc;
            unsigned long long val;
            if (!convert_val(*argv, &val)) usage();
            nofprocesses = val;
         } else if (strcmp(*argv, "-bufsize") == 0) {
            ++argv; --argc;
            unsigned long long val;
            if (!convert_val(*argv, &val)) usage();
            bufsize = val;
         } else if (strcmp(*argv, "-extra") == 0) {
            ++argv; --argc;
            unsigned long long val;
            if (!convert_val(*argv, &val)) usage();
            extra_space_size = val;
         } else {
            usage();
         }
         ++argv; --argc;
      }
      if (argc == 0) usage();
      if (shared_rts_run(nofprocesses, bufsize, extra_space_size, *argv, argv)) {
         exit(0);
      } else {
         fprintf(stderr, "%s: execution failed\n", cmdname);
         exit(1);
      }
   }


And following example demonstrates the view of the worker
processes where all processes with the exception of rank 0
write a message to 0 with their rank:

   #include <stdio.h>
   #include <stdlib.h>
   #include <afblib/shared_domain.h>
   #include <afblib/shared_rts.h>

   int main() {
      struct shared_domain* sd = shared_rts_init();
      if (!sd) {
         printf("wasn't invoked by smrun\n"); exit(1);
      }
      unsigned int rank = sd_get_rank(sd);
      unsigned int nofprocesses = sd_get_nofprocesses(sd);
      if (rank == 0) {
         for (unsigned int i = 1; i < nofprocesses; ++i) {
            unsigned int other;
            if (sd_read(sd, &other, sizeof other)) {
               printf("process 0 received message from %u\n", other);
            }
         }
      } else {
         sd_write(sd, 0, &rank, sizeof rank);
      }
      shared_rts_finish(sd);
   }

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <afblib/shared_domain.h>
#include <afblib/shared_env.h>
#include <afblib/shared_rts.h>

#define PREFIX ("SHARED")

bool shared_rts_run(unsigned int nofprocesses,
      size_t bufsize, size_t extra_space_size,
      const char* path, char** argv) {
   if (nofprocesses == 0) return true;
   if (bufsize == 0) return false;
   pid_t childs[nofprocesses];
   struct shared_domain* sd = sd_setup_with_extra_space(bufsize,
      nofprocesses, extra_space_size);
   if (!sd) return false;
   struct shared_env params = {
      .name = sd_get_name(sd),
   };
   if (!sd) return false;
   pid_t group = 0;
   for (unsigned int rank = 0; rank < nofprocesses; ++rank) {
      pid_t pid = fork();
      if (pid < 0) {
	 if (group) {
	    kill(-group, SIGTERM);
	 }
	 sd_free(sd);
	 return false;
      }
      if (pid == 0) {
	 params.rank = rank;
	 shared_env_store(&params, PREFIX);
	 execvp(path, argv);
	 exit(255);
      }
      setpgid(pid, group);
      if (group == 0) {
	 group = pid;
      }
      childs[rank] = pid;
   }
   pid_t pid; int wstat; unsigned int childs_left = nofprocesses;
   bool aborted = false;
   while (childs_left && (pid = waitpid(-group, &wstat, 0)) > 0) {
      unsigned int rank = 0;
      while (rank < nofprocesses && childs[rank] != pid) {
	 ++rank;
      }
      assert(rank < nofprocesses);
      childs[rank] = 0; --childs_left;
      if (!WIFEXITED(wstat) || WEXITSTATUS(wstat)) {
	 /* abort remaining processes */
	 aborted = true;
	 if (childs_left) {
	    kill(-group, SIGTERM);
	 }
      }
   }
   sd_free(sd);
   return !aborted;
}

struct shared_domain* shared_rts_init() {
   struct shared_env params;
   if (!shared_env_load(&params, PREFIX)) {
      return 0;
   }
   return sd_connect(params.name, params.rank);
}

void shared_rts_finish(struct shared_domain* sd) {
   sd_free(sd);
}
