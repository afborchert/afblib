/*
   Small library of useful utilities based on the dietlib by fefe
   Copyright (C) 2016 Andreas Franz Borchert
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

#ifndef AFBLIB_TRANSMIT_FD_H
#define AFBLIB_TRANSMIT_FD_H

#include <stdbool.h>

/* send and receive file descriptors over UNIX domain sockets */

/* send file descriptor fd over socket sfd
   along with a buffer buf of buflen bytes */
ssize_t send_fd_and_message(int sfd, int fd, void* buf, size_t buflen);

/* receive file descriptor and message from sfd,
   if successful the file descriptor is stored in *fd_ptr
   and the message at buf with up to buflen bytes */
ssize_t recv_fd_and_message(int sfd, int* fd_ptr, void* buf, size_t buflen);

/* just send file descriptor fd over the socket sfd */
bool send_fd(int sfd, int fd);

/* receive and return file descriptor from sfd,
   returns -1 in case of errors */
int recv_fd(int sfd);

#endif
