System :=	$(shell uname)

ifeq ($(System), SunOS)
CC :=		gcc
CPPFLAGS :=	-I. -D_XOPEN_SOURCE=600 -D__EXTENSIONS__
CFLAGS :=	-std=gnu11 -Wall -Wno-parentheses -g -O3
LDLIBS :=	-lowfat -lsocket -lnsl -lpcre
SharedLib :=	libafb.so.0
CFLAGS_SHARED := -fPIC
LDFLAGS_SHARED := -shared $(CFLAGS_SHARED) -Wl,-soname=$(SharedLib)
endif

ifeq ($(System), Linux)
CC :=		gcc
CPPFLAGS :=	-I.
CFLAGS :=	-pthread -std=gnu11 -Wall -Wno-parentheses -g -O3
LDLIBS :=	-lowfat -lpcre
SharedLib :=	libafb.so.0
CFLAGS_SHARED := -fPIC
LDFLAGS_SHARED := -shared $(CFLAGS_SHARED) -Wl,-soname=$(SharedLib)
endif

ifeq ($(System), Darwin)
# directory where the shared library is to be found:
#  - this gets hard-wired into the binary
#  - by default the current directory is chosen
LIBDIR :=	$(shell pwd)
# configuration for brew:
LIBOWFAT_ROOT := $(shell brew --prefix libowfat)
LIBOWFAT_INCL := $(LIBOWFAT_ROOT)/include/libowfat
LIBOWFAT_LIBD := $(LIBOWFAT_ROOT)/lib
LIBPCRE_ROOT :=	$(shell brew --prefix pcre)
LIBPCRE_INCL :=	$(LIBPCRE_ROOT)/include
LIBPCRE_LIBD := $(LIBPCRE_ROOT)/lib
#
CC :=		gcc
CPPFLAGS :=	-I. -I$(LIBOWFAT_INCL) -I$(LIBPCRE_INCL)
CFLAGS :=	-std=gnu11 -Wall -Wno-parentheses -g -O3
LDLIBS :=	-L$(LIBOWFAT_LIBD) -L$(LIBPCRE_LIBD) -lowfat -lpcre
SharedLib :=	libafb.dylib
CFLAGS_SHARED := -fPIC -fno-common
LDFLAGS_SHARED := -dynamiclib $(CFLAGS_SHARED)
endif

MAKEDEPEND :=	perl gcc-makedepend/gcc-makedepend.pl

Target :=	libafb.a
Sources :=	$(wildcard *.[ch])
CFiles :=	$(wildcard *.c)
Headers :=	$(wildcard *.h)
StaticObjects := $(patsubst %.c,static/%.o,$(CFiles))
SharedObjects := $(patsubst %.c,shared/%.o,$(CFiles))
Objects :=	$(StaticObjects) $(SharedObjects)
ArchivedObjects := $(patsubst static/%,$(Target)(%),$(StaticObjects))
WikiDir :=	../afblib.wiki
ManPages :=	$(patsubst %.c,$(WikiDir)/%.md,$(CFiles))

.PHONY:		all objdirs depend clean realclean manpages
all:		Makefile objdirs $(Target) $(SharedLib)
objdirs: ;	@mkdir -p static shared
depend: ;	$(MAKEDEPEND) -p shared/ -p static/ $(CPPFLAGS) $(CFiles)
clean: ;	rm -f $(Objects) *.tmp
realclean:	clean
		rm -f $(Target) $(SharedLib)

$(StaticObjects): static/%.o: %.c
		$(CC) -o $@ -c $(CPPFLAGS) $(CFLAGS) $<
$(SharedObjects): shared/%.o: %.c
		$(CC) -o $@ -c $(CPPFLAGS) $(CFLAGS) $(CFLAGS_SHARED) $<
$(ArchivedObjects): $(Target)(%.o): static/%.o
		$(AR) $(ARFLAGS) $(Target) $<
$(Target):	$(ArchivedObjects)

$(SharedLib):	$(SharedObjects)
		$(CC) -o $@ $(LDFLAGS_SHARED) $(LDFLAGS) $(SharedObjects) $(LDLIBS)
ifeq ($(System), Darwin)
		install_name_tool -id $(LIBDIR)/libafb.dylib libafb.dylib
endif

manpages:	$(ManPages)
$(ManPages): $(WikiDir)/%.md: %.c
		pod2markdown --utf8 $< $@

Makefile:	$(Headers)
		@$(MAKEDEPEND) -p shared/ -p static/ $(CPPFLAGS) $(CFiles)
# DO NOT DELETE
shared/concurrency.o: concurrency.c afblib/concurrency.h
static/concurrency.o: concurrency.c afblib/concurrency.h
shared/hostport.o: hostport.c afblib/hostport.h afblib/outbuf.h \
 afblib/outbuf_printf.h
static/hostport.o: hostport.c afblib/hostport.h afblib/outbuf.h \
 afblib/outbuf_printf.h
shared/inbuf.o: inbuf.c afblib/inbuf.h
static/inbuf.o: inbuf.c afblib/inbuf.h
shared/inbuf_readline.o: inbuf_readline.c afblib/inbuf_readline.h afblib/inbuf.h
static/inbuf_readline.o: inbuf_readline.c afblib/inbuf_readline.h afblib/inbuf.h
shared/inbuf_sareadline.o: inbuf_sareadline.c afblib/inbuf_sareadline.h \
 afblib/inbuf.h
static/inbuf_sareadline.o: inbuf_sareadline.c afblib/inbuf_sareadline.h \
 afblib/inbuf.h
shared/inbuf_scan.o: inbuf_scan.c afblib/inbuf_scan.h afblib/inbuf.h
static/inbuf_scan.o: inbuf_scan.c afblib/inbuf_scan.h afblib/inbuf.h
shared/mpx_session.o: mpx_session.c afblib/mpx_session.h afblib/hostport.h \
 afblib/outbuf.h afblib/multiplexor.h afblib/sliding_buffer.h
static/mpx_session.o: mpx_session.c afblib/mpx_session.h afblib/hostport.h \
 afblib/outbuf.h afblib/multiplexor.h afblib/sliding_buffer.h
shared/mt_service.o: mt_service.c afblib/mt_service.h afblib/hostport.h \
 afblib/outbuf.h
static/mt_service.o: mt_service.c afblib/mt_service.h afblib/hostport.h \
 afblib/outbuf.h
shared/multiplexor.o: multiplexor.c afblib/multiplexor.h
static/multiplexor.o: multiplexor.c afblib/multiplexor.h
shared/outbuf.o: outbuf.c afblib/outbuf.h
static/outbuf.o: outbuf.c afblib/outbuf.h
shared/outbuf_printf.o: outbuf_printf.c afblib/outbuf_printf.h afblib/outbuf.h
static/outbuf_printf.o: outbuf_printf.c afblib/outbuf_printf.h afblib/outbuf.h
shared/pconnect.o: pconnect.c afblib/pconnect.h
static/pconnect.o: pconnect.c afblib/pconnect.h
shared/preforked_service.o: preforked_service.c afblib/preforked_service.h \
 afblib/hostport.h afblib/outbuf.h
static/preforked_service.o: preforked_service.c afblib/preforked_service.h \
 afblib/hostport.h afblib/outbuf.h
shared/service.o: service.c afblib/service.h afblib/hostport.h afblib/outbuf.h
static/service.o: service.c afblib/service.h afblib/hostport.h afblib/outbuf.h
shared/shared_cv.o: shared_cv.c afblib/shared_cv.h afblib/shared_mutex.h
static/shared_cv.o: shared_cv.c afblib/shared_cv.h afblib/shared_mutex.h
shared/shared_domain.o: shared_domain.c afblib/shared_cv.h afblib/shared_mutex.h \
 afblib/shared_domain.h
static/shared_domain.o: shared_domain.c afblib/shared_cv.h afblib/shared_mutex.h \
 afblib/shared_domain.h
shared/shared_env.o: shared_env.c afblib/shared_env.h
static/shared_env.o: shared_env.c afblib/shared_env.h
shared/shared_mutex.o: shared_mutex.c afblib/shared_mutex.h
static/shared_mutex.o: shared_mutex.c afblib/shared_mutex.h
shared/shared_rts.o: shared_rts.c afblib/shared_domain.h afblib/shared_env.h \
 afblib/shared_rts.h
static/shared_rts.o: shared_rts.c afblib/shared_domain.h afblib/shared_env.h \
 afblib/shared_rts.h
shared/sliding_buffer.o: sliding_buffer.c afblib/sliding_buffer.h
static/sliding_buffer.o: sliding_buffer.c afblib/sliding_buffer.h
shared/ssystem.o: ssystem.c afblib/ssystem.h
static/ssystem.o: ssystem.c afblib/ssystem.h
shared/strhash.o: strhash.c afblib/strhash.h
static/strhash.o: strhash.c afblib/strhash.h
shared/strlist.o: strlist.c afblib/strlist.h
static/strlist.o: strlist.c afblib/strlist.h
shared/tokenizer.o: tokenizer.c afblib/strlist.h afblib/tokenizer.h
static/tokenizer.o: tokenizer.c afblib/strlist.h afblib/tokenizer.h
shared/transmit_fd.o: transmit_fd.c afblib/transmit_fd.h
static/transmit_fd.o: transmit_fd.c afblib/transmit_fd.h
shared/udp_session.o: udp_session.c afblib/udp_session.h afblib/hostport.h \
 afblib/outbuf.h
static/udp_session.o: udp_session.c afblib/udp_session.h afblib/hostport.h \
 afblib/outbuf.h
