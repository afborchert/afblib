/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2003, 2008 Andreas Franz Borchert
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
 * Create and manage pipelines to given commands.
 * In comparison to popen(), neither stdio nor the shell are used.
 * afb 6/2003
 */
#ifndef AFBLIB_PCONNECT_H
#define AFBLIB_PCONNECT_H

#include <stdbool.h>
#include <unistd.h>

enum {PIPE_READ = 0, PIPE_WRITE = 1};

typedef struct pipe_end {
   int fd;
   pid_t pid;
   int wstat;
} pipe_end;

/*
 * create a pipeline to the given command;
 * mode should be either PIPE_READ or PIPE_WRITE;
 * return a filled pipe_end structure and true on success
 * and false in case of failures
 */
bool pconnect(const char* path, char* const* argv,
      int mode, pipe_end* pipe_con);

/*
 * like pconnect() but connect fd to the standard input
 * or output file descriptor that is not connected to the pipe
 */
bool pconnect2(const char* path, char* const* argv,
      int mode, int fd, pipe_end* pipe_con);

/*
 * close pipeline and wait for the forked-off process to exit;
 * the wait status is returned in pipe->wstat;
 * true is returned if successful, false otherwise
 */
bool phangup(pipe_end* pipe_end);

#endif
