/*
   Small library of useful utilities
   Copyright (C) 2003, 2008, 2021 Andreas Franz Borchert
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

ssystem -- secure variant of system()

=head1 SYNOPSIS

   #include <afblib/ssystem.h>

   int ssystem(char* argv[]);

=head1 DESCRIPTION

B<ssystem> forks off a new process which execs to the path
found in C<argv[0]> and passes B<argv> as command line arguments
to it. The parent process waits until the spawned off process
exits.

B<SIGINT> and B<SIGQUIT> are ignored by the parent
process during the lifetime of the subprocess. B<fflush> is
invoked to flush all output streams.

In comparison with B<system>, B<ssystem> avoids the involvement
of a shell process which includes the possible interpretation of
meta characters. This is dangerous if the list of arguments
comes from untested user input.

=head1 DIAGNOSTICS

In case of errors, -1 is returned. Otherwise, the status of
the terminated subprocess is returned.

=head1 BUGS

As B<ssystem> does not know about opened pipelines and other
opened files, they are shared with the forked-off process.

=head1 AUTHOR

Andreas Borchert

=cut

*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <afblib/ssystem.h>

int ssystem(char* argv[]) {
   struct sigaction sigact = { .sa_handler = SIG_IGN };
   struct sigaction sigact_sigint;
   struct sigaction sigact_sigquit;
   if (sigaction(SIGINT, &sigact, &sigact_sigint) < 0 ||
	 sigaction(SIGQUIT, &sigact, &sigact_sigquit) < 0) {
      return -1;
   }
   fflush(0); /* flush all streams */

   pid_t child = fork();
   if (child == -1) return -1;
   if (child == 0) {
      sigaction(SIGINT, &sigact_sigint, 0);
      sigaction(SIGQUIT, &sigact_sigquit, 0);
      execvp(argv[0], argv);
      exit(255);
   }
   int stat;
   pid_t pid = waitpid(child, &stat, 0);

   sigaction(SIGINT, &sigact_sigint, 0);
   sigaction(SIGQUIT, &sigact_sigint, 0);

   if (pid != child) return -1;
   return stat;
}
