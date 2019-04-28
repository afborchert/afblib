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

/*

=head1 NAME

transmit_fd -- transmit file descriptors over UNIX domain sockets

=head1 SYNOPSIS

   #include <afblib/transmit_fd.h>

   ssize_t send_fd_and_message(int sfd, int fd, void* buf, size_t buflen);
   ssize_t recv_fd_and_message(int sfd, int* fd_ptr, void* buf, size_t buflen);

   bool send_fd(int sfd, int fd);
   int recv_fd(int sfd);

=head1 DESCRIPTION

File descriptors and the associated access rights can be shared
between processes that are connected through a UNIX domain socket.
If a common ancestor exists, the connection can be created using
I<socketpair>:

   int sfds[2];
   if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, sfds) < 0) {
      // failure
   }

It is advisable to use SOCK_SEQPACKET here to make sure that
packets consisting of a message and the file descriptor
remain intact. This should have no extra costs in case of
UNIX domain sockets.

File descriptors are always sent along with a message.
In cae of I<send_fd_and_message> and I<recv_fd_and_message>
the message can be explicitly sent and received. Otherwise,
if no message is needed, I<send_fd> and I<recv_fd> provide
a simpler interface. A small message consisting of one byte
is sent nonetheless to distinguish a successful delivery from
end of input.

All four functions accept the socket file descriptor _sfd_
as first parameter. This must be a UNIX domain socket.
I<send_fd_and_message> and I<send_fd> accept a
file descriptor _fd_ as second argument which is sent over
the socket. Each of these function must be matched by
the corresponding function on the receiving side.
While I<recv_fd_and_message> stores the received file
descriptor at the storage pointed to by I<fd_ptr>,
I<recv_fd> simply returns it.

=head1 DIAGNOSTICS

The return values of I<send_fd_and_message> and
I<recv_fd_and_message> correspond to the system
calls I<write> and I<read>. -1 is returned in
case of failures.

I<send_fd> returns a simple Boolean value, indicating
the success. I<recv_fd> returns the file descriptor
in case of success, and -1 otherwise.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <stdbool.h>
#include <sys/socket.h>
#include <afblib/transmit_fd.h>

/* send and receive file descriptors over sockets */

struct fd_cmsg {
   struct cmsghdr cm;
   int fd;
};

ssize_t send_fd_and_message(int sfd, int fd, void* buf, size_t buflen) {
   struct fd_cmsg cmsg = {
      .cm = {
	 .cmsg_len = sizeof cmsg,
	 .cmsg_level = SOL_SOCKET,
	 .cmsg_type = SCM_RIGHTS
      },
      .fd = fd
   };
   struct iovec iovec[1] = {
      {
	 .iov_base = buf,
	 .iov_len = buflen
      }
   };
   struct msghdr msg = {
      .msg_iov = iovec,
      .msg_iovlen = sizeof(iovec)/sizeof(iovec[0]),
      .msg_control = &cmsg.cm,
      .msg_controllen = sizeof cmsg,
   };
   return sendmsg(sfd, &msg, /* flags = */ 0);
}

ssize_t recv_fd_and_message(int sfd, int* fd_ptr, void* buf, size_t buflen) {
   struct fd_cmsg cmsg = {{0}};
   struct iovec iovec[1] = {
      {
	 .iov_base = buf,
	 .iov_len = buflen
      }
   };
   struct msghdr msg = {
      .msg_iov = iovec,
      .msg_iovlen = sizeof(iovec)/sizeof(iovec[0]),
      .msg_control = &cmsg.cm,
      .msg_controllen = sizeof cmsg,
   };
   ssize_t nbytes = recvmsg(sfd, &msg, MSG_WAITALL);
   if (nbytes < 0) return -1;
   if (fd_ptr) *fd_ptr = cmsg.fd;
   return nbytes;
}

bool send_fd(int sfd, int fd) {
   char msg = 'F';
   ssize_t nbytes = send_fd_and_message(sfd, fd, &msg, sizeof msg);
   return nbytes > 0;
}

int recv_fd(int sfd) {
   int fd; char msg;
   ssize_t nbytes = recv_fd_and_message(sfd, &fd, &msg, sizeof msg);
   if (nbytes <= 0 || msg != 'F') return -1;
   return fd;
}
