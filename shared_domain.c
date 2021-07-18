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

shared_domain -- communication domain based on shared memory

=head1 SYNOPSIS

   #include <afblib/shared_domain.h>

   struct shared_domain* sd_setup(size_t nbytes,
      unsigned int nofprocesses);
   struct shared_domain* sd_setup_with_extra_space(size_t bufsize,
	 unsigned int nofprocesses, size_t extra_space_size,
	 const sigset_t* sigmask);
   struct shared_domain* sd_connect(char* name, unsigned int rank);
   void sd_free(struct shared_domain* sd);

   unsigned int sd_get_rank(struct shared_domain* sd);
   unsigned int sd_get_nofprocesses(struct shared_domain* sd);
   char* sd_get_name(struct shared_domain* sd);
   size_t sd_get_extra_space_size(struct shared_domain* sd);
   void* sd_get_extra_space(struct shared_domain* sd);

   bool sd_barrier(struct shared_domain* sd);
   bool sd_write(struct shared_domain* sd, unsigned int recipient,
      const void* buf, size_t nbytes);
   bool sd_read(struct shared_domain* sd, void* buf, size_t nbytes);

   bool sd_shutdown(struct shared_domain* sd);
   bool sd_terminating(struct shared_domain* sd);

=head1 DESCRIPTION

A shared communication domain allows I<nofprocesses> processes
to communicate and synchronize with each other using a shared memory
segment.

A shared communication domain for I<nofprocesses> processes is created
by I<sd_setup>. The internal per-process buffers have a size of
I<nbytes> each. Each shared communication domain is accessible to other
processes belonging to the same user through a temporary file in F</tmp>
whose name can be retrieved using I<sd_get_name>. A shared communication
domain can be released using I<sd_free>. While I<sd_setup> must be
called by one process only, I<sd_free> is to be called by all
participating processes when access is no longer required.

I<sd_setup_with_extra_space> works like I<sd_setup> but
allocates a shared memory segment with extra space of
I<extra_space_size> bytes. This extra space can be accessed
through I<sd_get_extra_space> and its size can be retrieved
using I<sd_get_extra_space_size>. If I<sigmask> is non-null,
all included signals will be blocked whenever shared mutexes
are locked.

Other processes are free to connect to an already existing
shared communication domain using I<sd_connect> where the I<name>
and the rank (in the range of 0 to I<nofprocesses>-1) are to
be specified.

Communication among the processes of a shared communication domain
is possible through I<sd_write> and I<sd_read>. Both functions block
until the full amount has been written or read. Both operations
work atomically even if the number of bytes exceeds that of the
internal buffer size. This means that multiple write operations
to the same process will never mix nor do concurrent read operations
by one process (possible through multiple threads) interfere with
each other.

I<sd_barrier> provides a simple synchronization mechanism among
all processes of a shared communication domain. Each process
who calls I<sd_barrier> is suspended until all processes
have invoked I<sd_barrier>.

The process which invoked I<sd_setup> is free to call
I<sd_shutdown>. This will wake up all processes waiting
in I<sd_barrier>, I<sd_write>, or I<sd_read> and causing
them to fail. I<sd_terminating> can be used by all
processes to check if I<sd_shutdown> has been called before.

=head1 AUTHOR

Andreas F. Borchert

=cut

*/

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef __STDC_NO_ATOMICS__
   #include <stdatomic.h>
   #ifdef ATOMIC_BOOL_LOCK_FREE
      #define SD_ATOMIC
   #endif
#endif

#include <afblib/shared_cv.h>
#include <afblib/shared_domain.h>
#include <afblib/shared_mutex.h>

/* header of a shared memory region */
struct shared_mem_header {
   /* configuration of shared memory domain */
   unsigned int nofprocesses;
   size_t bufsize; // size of the buffers
   /* support of barriers */
   shared_mutex mutex;
   shared_cv wait_for_barrier;
   unsigned int sync_count; // for barrier
   /* support of shared extra space */
   size_t extra_space_size;
   ptrdiff_t extra_space_offset;
   /* signal termination: set to 1 in case of a shutdown */
#ifdef SD_ATOMIC
   atomic_bool terminating;
#else
   volatile sig_atomic_t terminating;
#endif
};

/* per-process buffer in the shared memory region */
struct shared_mem_buffer {
   shared_mutex mutex;
   shared_cv ready_for_reading;
   shared_cv ready_for_writing;
   /* needed as we do not want to mix write operations coming
      from different senders: */
   shared_cv ready_for_writing_alone;
   bool writing; /* true if a sender is currently active */
   /* needed if multiple threads access concurrently a buffer: */
   shared_cv ready_for_reading_alone;
   bool reading; /* true if a recipient is currently active */
   /* ring buffer */
   size_t filled;
   size_t read_index;
   size_t write_index;
};

/* local data structure (not in shared memory) */
struct shared_domain {
   bool creator; /* true for the creator of the shared memory buffer */
   unsigned int rank;
   unsigned int nofprocesses;
   size_t bufsize;
   char* name;
   void* sharedmem;
   struct shared_mem_header* header;
   struct shared_mem_buffer* first_buffer;
   ptrdiff_t buffer_stride;
   size_t extra_space_size;
   void* extra_space_ptr;
};

static size_t alignto(size_t size, size_t alignment) {
   return (size + alignment - 1) & ~(alignment - 1);
}

static ptrdiff_t compute_shared_mem_buffer_stride(size_t bufsize) {
   return
      alignto(sizeof(struct shared_mem_buffer) + bufsize,
	 alignof(struct shared_mem_buffer));
}

static size_t compute_shared_mem_size(size_t bufsize,
      unsigned int nofprocesses, size_t extra_space_size) {
   size_t mem_size =
      alignto(sizeof(struct shared_mem_header),
	 alignof(struct shared_mem_buffer)) +
      compute_shared_mem_buffer_stride(bufsize) * nofprocesses;
   if (extra_space_size) {
      mem_size = alignto(mem_size, alignof(max_align_t)) +
	 extra_space_size;
   }
   return mem_size;
}

/* initialize a shared memory buffer;
   this must be called by one process only;
   if successful this has to be undone by free_buffer */
static bool init_buffer(struct shared_mem_buffer* buffer,
      const sigset_t* sigmask) {
   bool ok;
   ok = shared_mutex_create_with_sigmask(&buffer->mutex, sigmask);
   if (!ok) return false;
   shared_cv* cvs[] = {
      &buffer->ready_for_reading,
      &buffer->ready_for_writing,
      &buffer->ready_for_writing_alone,
      &buffer->ready_for_reading_alone,
      0
   };
   for (shared_cv** cvp = cvs; *cvp; ++cvp) {
      ok = shared_cv_create(*cvp);
      if (!ok) {
	 /* undo everything done so far */
	 for (shared_cv** cvp2 = cvs; cvp2 != cvp; ++cvp2) {
	    shared_cv_free(*cvp2);
	 }
	 shared_mutex_free(&buffer->mutex);
	 return false;
      }
   }

   buffer->writing = buffer->reading = false;
   buffer->filled = buffer->read_index = buffer->write_index = 0;
   return true;
}

/* free all resources associated with a shared memory buffer;
   this must be called by one process only */
static bool free_buffer(struct shared_mem_buffer* buffer) {
   bool ok;
   shared_cv* cvs[] = {
      &buffer->ready_for_reading,
      &buffer->ready_for_writing,
      &buffer->ready_for_writing_alone,
      &buffer->ready_for_reading_alone,
      0
   };
   for (shared_cv** cvp = cvs; *cvp; ++cvp) {
      ok = shared_cv_free(*cvp) && ok;
   }
   ok = shared_mutex_free(&buffer->mutex) && ok;
   return ok;
}

/* return address of the id-th buffer of a shared memory domain */
static struct shared_mem_buffer* get_buffer(struct shared_domain* sd,
      unsigned int id) {
   if (id >= sd->nofprocesses) return 0;
   return (struct shared_mem_buffer*)
      ((char*) sd->first_buffer + sd->buffer_stride * id);
}

/* initialize a shared_mem_header struct;
   this must be called by one process only */
static bool init_header(struct shared_mem_header* hp,
      unsigned int nofprocesses,
      size_t bufsize, size_t extra_space_size,
      const sigset_t* sigmask) {
   bool ok;
   ok = shared_mutex_create_with_sigmask(&hp->mutex, sigmask);
   if (!ok) return false;
   ok = shared_cv_create(&hp->wait_for_barrier);
   if (!ok) {
      shared_mutex_free(&hp->mutex);
      return false;
   }
   hp->nofprocesses = nofprocesses;
   hp->bufsize = bufsize;
   hp->sync_count = 0;
   hp->extra_space_size = extra_space_size;
   hp->extra_space_offset = (ptrdiff_t)
      (compute_shared_mem_size(bufsize, nofprocesses, extra_space_size) -
      extra_space_size);
#ifdef SD_ATOMIC
   atomic_init(&hp->terminating, false);
#else
   hp->terminating = false;
#endif
   return true;
}

static bool free_header(struct shared_mem_header* hp) {
   bool ok;
   ok = shared_cv_free(&hp->wait_for_barrier);
   ok = shared_mutex_free(&hp->mutex) && ok;
   return ok;
}

struct shared_domain* sd_setup(size_t bufsize, unsigned int nofprocesses) {
   return sd_setup_with_extra_space(bufsize, nofprocesses, 0, 0);
}

struct shared_domain* sd_setup_with_extra_space(size_t bufsize,
      unsigned int nofprocesses, size_t extra_space_size,
      const sigset_t* sigmask) {
   char* path = strdup("/tmp/.SHARED-XXXXXX");
   int fd = mkstemp(path);
   if (fd < 0) {
      free(path); return 0;
   }
   size_t sharedmem_size = compute_shared_mem_size(bufsize,
      nofprocesses, extra_space_size);
   if (ftruncate(fd, sharedmem_size) < 0) {
      close(fd); unlink(path); free(path); return 0;
   }
   struct shared_domain* sd = malloc(sizeof(struct shared_domain));
   if (!sd) {
      close(fd); unlink(path); free(path); return 0;
   }

   void* sm = mmap(0, sharedmem_size, PROT_READ|PROT_WRITE,
      MAP_SHARED, fd, 0);
   close(fd);
   if (sm == MAP_FAILED) {
      unlink(path); free(path); return 0;
   }

   struct shared_mem_header* header = (struct shared_mem_header*) sm;
   if (!init_header(header, nofprocesses,
	 bufsize, extra_space_size, sigmask)) {
      goto fail;
   }
   struct shared_mem_buffer* first_buffer = (struct shared_mem_buffer*) (
      (char*) sm +
	 alignto(sizeof(struct shared_mem_header),
	    alignof(struct shared_mem_buffer))
   );
   ptrdiff_t buffer_stride = compute_shared_mem_buffer_stride(bufsize);
   for (unsigned int i = 0; i < nofprocesses; ++i) {
      struct shared_mem_buffer* buffer = (struct shared_mem_buffer*) (
	 (char*) first_buffer + i * buffer_stride
      );
      if (!init_buffer(buffer, sigmask)) {
	 for (unsigned int j = 0; j < i; ++j) {
	    struct shared_mem_buffer* buffer = (struct shared_mem_buffer*) (
	       (char*) first_buffer + i * buffer_stride
	    );
	    free_buffer(buffer);
	 }
	 free_header(header);
	 goto fail;
      }
   }
   
   void* extra_space_ptr = 0;
   if (extra_space_size) {
      extra_space_ptr = (void*)((char*) sm + header->extra_space_offset);
   }
   
   *sd = (struct shared_domain) {
      .creator = true,
      .rank = 0,
      .nofprocesses = nofprocesses,
      .bufsize = bufsize,
      .name = path,
      .sharedmem = sm,
      .header = header,
      .first_buffer = first_buffer,
      .buffer_stride = buffer_stride,
      .extra_space_ptr = extra_space_ptr,
      .extra_space_size = extra_space_size,
   };
   return sd;

fail:
   free(sd);
   unlink(path);
   free(path);
   munmap(sm, sharedmem_size);
   return 0;
}

struct shared_domain* sd_connect(char* name, unsigned int rank) {
   int fd = open(name, O_RDWR);
   if (fd < 0) return 0;
   struct shared_mem_header hbuf;
   if (read(fd, &hbuf, sizeof hbuf) != sizeof hbuf) {
      close(fd); return 0;
   }
   unsigned int nofprocesses = hbuf.nofprocesses;
   size_t bufsize = hbuf.bufsize;
   if (rank >= nofprocesses) {
      close(fd); return 0;
   }
   size_t extra_space_size = hbuf.extra_space_size;
   size_t sharedmem_size = compute_shared_mem_size(bufsize,
      nofprocesses, extra_space_size);
   void* sm = mmap(0, sharedmem_size, PROT_READ|PROT_WRITE,
      MAP_SHARED, fd, 0);
   close(fd);
   if (sm == MAP_FAILED) return 0;

   struct shared_mem_header* header = (struct shared_mem_header*) sm;
   struct shared_mem_buffer* first_buffer = (struct shared_mem_buffer*) (
      (char*) sm +
	 alignto(sizeof(struct shared_mem_header),
	    alignof(struct shared_mem_buffer))
   );
   ptrdiff_t buffer_stride = compute_shared_mem_buffer_stride(bufsize);

   struct shared_domain* sd = malloc(sizeof(struct shared_domain));
   if (!sd) goto fail;

   void* extra_space_ptr = 0;
   if (extra_space_size) {
      extra_space_ptr = (void*)((char*) sm + header->extra_space_offset);
   }

   *sd = (struct shared_domain) {
      .creator = false,
      .rank = rank,
      .nofprocesses = nofprocesses,
      .bufsize = bufsize,
      .name = name,
      .sharedmem = sm,
      .header = header,
      .first_buffer = first_buffer,
      .buffer_stride = buffer_stride,
      .extra_space_ptr = extra_space_ptr,
      .extra_space_size = extra_space_size,
   };
   if (sd_terminating(sd)) {
      free(sd); goto fail;
   }
   return sd;

fail:
   munmap(sm, sharedmem_size);
   return 0;
}

void sd_free(struct shared_domain* sd) {
   if (sd->creator) {
      for (unsigned int i = 0; i < sd->nofprocesses; ++i) {
	 struct shared_mem_buffer* buffer = get_buffer(sd, i);
	 free_buffer(buffer);
      }
      free_header(sd->header);
      unlink(sd->name);
      free(sd->name);
   }
   size_t sharedmem_size = compute_shared_mem_size(sd->bufsize,
      sd->nofprocesses, sd->extra_space_size);
   munmap(sd->sharedmem, sharedmem_size);
   free(sd);
}

unsigned int sd_get_rank(struct shared_domain* sd) {
   return sd->rank;
}

unsigned int sd_get_nofprocesses(struct shared_domain* sd) {
   return sd->nofprocesses;
}

char* sd_get_name(struct shared_domain* sd) {
   return sd->name;
}

size_t sd_get_extra_space_size(struct shared_domain* sd) {
   return sd->header->extra_space_size;
}

void* sd_get_extra_space(struct shared_domain* sd) {
   return sd->extra_space_ptr;
}

bool sd_barrier(struct shared_domain* sd) {
   struct shared_mem_header* hp = sd->header;
   if (sd_terminating(sd)) return false;
   bool ok;
   ok = shared_mutex_lock(&hp->mutex);
   if (!ok) return false;
   if (sd_terminating(sd)) {
      shared_mutex_unlock(&hp->mutex);
      return false;
   }
   if (hp->sync_count == 0) {
      hp->sync_count = sd->nofprocesses - 1;
   } else {
      --hp->sync_count;
   }
   if (hp->sync_count == 0) {
      ok = shared_cv_notify_all(&hp->wait_for_barrier);
   } else {
      while (ok && hp->sync_count > 0) {
	 ok = shared_cv_wait(&hp->wait_for_barrier, &hp->mutex) &&
	    !sd_terminating(sd);
      }
   }
   return shared_mutex_unlock(&hp->mutex) && ok;
}

bool sd_write(struct shared_domain* sd, unsigned int recipient,
      const void* buf, size_t nbytes) {
   if (nbytes == 0) return true;
   if (recipient >= sd->nofprocesses) return false;
   if (sd_terminating(sd)) return false;
   struct shared_mem_buffer* buffer = get_buffer(sd, recipient);
   bool ok;
   ok = shared_mutex_lock(&buffer->mutex);
   if (!ok) return false;
   if (sd_terminating(sd)) {
      shared_mutex_unlock(&buffer->mutex);
      return false;
   }
   while (buffer->writing) {
      /* someone else is already writing to this buffer;
         we do not interfere here */
      ok = shared_cv_wait(&buffer->ready_for_writing_alone,
	 &buffer->mutex) && !sd_terminating(sd);
      if (!ok) goto unlock;
   }
   /* now we have exclusive write access to this buffer */
   buffer->writing = true;

   const char* src = (const char*) buf;
   size_t written = 0;
   char* shared_buf = (char*) buffer + sizeof(struct shared_mem_buffer);
   while (written < nbytes) {
      while (buffer->filled == sd->bufsize) {
	 ok = shared_cv_wait(&buffer->ready_for_writing,
	    &buffer->mutex) && !sd_terminating(sd);
	 if (!ok) goto unlock;
      }
      size_t count = nbytes - written;
      if (sd->bufsize - buffer->filled < count) {
	 count = sd->bufsize - buffer->filled;
      }
      if (sd->bufsize - buffer->write_index < count) {
	 count = sd->bufsize - buffer->write_index;
      }
      memcpy(shared_buf + buffer->write_index, src, count);
      written += count; src += count;
      buffer->write_index = (buffer->write_index + count) % sd->bufsize;
      buffer->filled += count;
      ok = shared_cv_notify_one(&buffer->ready_for_reading);
   }
   buffer->writing = false;
   ok = shared_cv_notify_one(&buffer->ready_for_writing_alone)
      && ok;

unlock:
   return shared_mutex_unlock(&buffer->mutex) && ok;
}

bool sd_read(struct shared_domain* sd, void* buf, size_t nbytes) {
   if (nbytes == 0) return true;
   if (sd_terminating(sd)) return false;
   struct shared_mem_buffer* buffer = get_buffer(sd, sd->rank);
   bool ok;
   ok = shared_mutex_lock(&buffer->mutex);
   if (!ok) return false;
   if (sd_terminating(sd)) {
      shared_mutex_unlock(&buffer->mutex);
      return false;
   }
   while (buffer->reading) {
      /* another thread of the same process is already reading from
	 this buffer; we must not interfere here until the other
	 read operation is completed */
      ok = shared_cv_wait(&buffer->ready_for_reading_alone,
	 &buffer->mutex) && !sd_terminating(sd);
      if (!ok) goto unlock;
   }
   /* now we have exclusive read access to this buffer */
   buffer->reading = true;

   char* dest = (char*) buf;
   size_t bytes_read = 0;
   char* shared_buf = (char*) buffer + sizeof(struct shared_mem_buffer);
   while (bytes_read < nbytes) {
      while (buffer->filled == 0) {
	 ok = shared_cv_wait(&buffer->ready_for_reading,
	    &buffer->mutex) && !sd_terminating(sd);
	 if (!ok) goto unlock;
      }
      size_t count = nbytes - bytes_read;
      if (count > buffer->filled) {
	 count = buffer->filled;
      }
      if (count > sd->bufsize - buffer->read_index) {
	 count = sd->bufsize - buffer->read_index;
      }
      memcpy(dest, shared_buf + buffer->read_index, count);
      bytes_read += count; dest += count;
      buffer->read_index = (buffer->read_index + count) % sd->bufsize;
      buffer->filled -= count;
      ok = shared_cv_notify_one(&buffer->ready_for_writing);
   }
   buffer->reading = false;
   ok = shared_cv_notify_one(&buffer->ready_for_reading_alone)
      && ok;

unlock:
   return shared_mutex_unlock(&buffer->mutex) && ok;
}

bool sd_shutdown(struct shared_domain* sd) {
   if (!sd->creator) return false;
   struct shared_mem_header* hp = sd->header;
   bool already_terminating;
#ifdef SD_ATOMIC
   already_terminating = atomic_exchange(&hp->terminating, true);
#else
   already_terminating = hp->terminating;
   hp->terminating = true;
#endif
   if (already_terminating) return false;
   /* notify all condition variables:
      all processes hanging in a shared_cv_wait will wake up,
      see the terminating flag and will abort the current operation;
      once this is done, we are safe to terminate these processes
      without running the risk to kill them while waiting --
      this would leave the condition variable in a possibly undefined state
   */
   shared_mutex_lock(&hp->mutex);
   bool ok = shared_cv_notify_all(&hp->wait_for_barrier);
   shared_mutex_unlock(&hp->mutex);
   for (unsigned int i = 0; i < sd->nofprocesses; ++i) {
      struct shared_mem_buffer* buffer = get_buffer(sd, i);
      shared_mutex_lock(&buffer->mutex);
      ok = shared_cv_notify_all(&buffer->ready_for_reading) && ok;
      ok = shared_cv_notify_all(&buffer->ready_for_writing) && ok;
      ok = shared_cv_notify_all(&buffer->ready_for_writing_alone) && ok;
      ok = shared_cv_notify_all(&buffer->ready_for_reading_alone) && ok;
      shared_mutex_unlock(&buffer->mutex);
   }
   return ok;
}

bool sd_terminating(struct shared_domain* sd) {
   bool terminating;
#ifdef SD_ATOMIC
   terminating = atomic_load(&sd->header->terminating);
#else
   terminating = sd->header->terminating;
#endif
   return terminating;
}
