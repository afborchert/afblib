System :=	$(shell uname)

ifeq ($(System), SunOS)
CC :=		gcc
CPPFLAGS :=	-I. -D_XOPEN_SOURCE=600
CFLAGS :=	-std=gnu11 -Wall -Wno-parentheses -g
LDLIBS :=	-lowfat -lsocket -lnsl -lpcre
SharedLib :=	libafb.so.0
CFLAGS_SHARED := -fPIC
LDFLAGS_SHARED := -shared $(CFLAGS_SHARED) -Wl,-soname=$(SharedLib)
endif

ifeq ($(System), Linux)
CC :=		gcc
CPPFLAGS :=	-I.
CFLAGS :=	-pthread -std=gnu11 -Wall -Wno-parentheses -g
LDLIBS :=	-lowfat -lpcre
SharedLib :=	libafb.so.0
CFLAGS_SHARED := -fPIC
LDFLAGS_SHARED := -shared $(CFLAGS_SHARED) -Wl,-soname=$(SharedLib)
endif

ifeq ($(System), Darwin)
CC :=		gcc
CPPFLAGS :=	-I.
CFLAGS :=	-std=gnu11 -Wall -Wno-parentheses -g
LDLIBS :=	-lowfat -lpcre
SharedLib :=	libafb.dylib
CFLAGS_SHARED := -fPIC -fno-common
LDFLAGS_SHARED := -dynamiclib $(CFLAGS_SHARED)
endif

Target :=	libafb.a
Sources :=	$(wildcard *.[ch])
CFiles :=	$(wildcard *.c)
Headers :=	$(wildcard *.h)
StaticObjects := $(patsubst %.c,static/%.o,$(CFiles))
SharedObjects := $(patsubst %.c,shared/%.o,$(CFiles))
Objects :=	$(StaticObjects) $(SharedObjects)
ArchivedObjects := $(patsubst static/%,$(Target)(%),$(StaticObjects))
WebDir :=	./www
ManPages :=	$(patsubst %.c,$(WebDir)/%.html,$(CFiles))
WebRoot :=	http://www.mathematik.uni-ulm.de/numerik/soft2/ss19/afblib

.PHONY:		all objdirs depend clean realclean manpages
all:		Makefile objdirs $(Target) $(SharedLib)
objdirs: ;	@mkdir -p static shared
depend: ;	gcc-makedepend -p shared/ -p static/ $(CPPFLAGS) $(CFiles)
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

manpages:	$(ManPages)
$(ManPages): $(WebDir)/%.html: %.c
		pod2html \
		   --htmlroot=$(WebRoot) \
		   --infile=$< \
		   --outfile=$@ \
		   --title="$*(3)" \
		   --noindex

Makefile:	$(Headers)
		@gcc-makedepend -p shared/ -p static/ $(CPPFLAGS) $(CFiles)
# DO NOT DELETE
shared/tokenizer.o: tokenizer.c afblib/strlist.h afblib/tokenizer.h
static/tokenizer.o: tokenizer.c afblib/strlist.h afblib/tokenizer.h
shared/ssystem.o: ssystem.c afblib/ssystem.h
static/ssystem.o: ssystem.c afblib/ssystem.h
shared/preforked_service.o: preforked_service.c afblib/preforked_service.h \
 afblib/hostport.h
static/preforked_service.o: preforked_service.c afblib/preforked_service.h \
 afblib/hostport.h
shared/outbuf.o: outbuf.c afblib/outbuf.h
static/outbuf.o: outbuf.c afblib/outbuf.h
shared/transmit_fd.o: transmit_fd.c afblib/transmit_fd.h
static/transmit_fd.o: transmit_fd.c afblib/transmit_fd.h
shared/strhash.o: strhash.c afblib/strhash.h
static/strhash.o: strhash.c afblib/strhash.h
shared/sliding_buffer.o: sliding_buffer.c afblib/sliding_buffer.h
static/sliding_buffer.o: sliding_buffer.c afblib/sliding_buffer.h
shared/mpx_session.o: mpx_session.c afblib/mpx_session.h afblib/hostport.h \
 afblib/multiplexor.h afblib/sliding_buffer.h
static/mpx_session.o: mpx_session.c afblib/mpx_session.h afblib/hostport.h \
 afblib/multiplexor.h afblib/sliding_buffer.h
shared/inbuf_scan.o: inbuf_scan.c afblib/inbuf_scan.h afblib/inbuf.h
static/inbuf_scan.o: inbuf_scan.c afblib/inbuf_scan.h afblib/inbuf.h
shared/inbuf_sareadline.o: inbuf_sareadline.c afblib/inbuf_sareadline.h \
 afblib/inbuf.h
static/inbuf_sareadline.o: inbuf_sareadline.c afblib/inbuf_sareadline.h \
 afblib/inbuf.h
shared/hostport.o: hostport.c afblib/hostport.h
static/hostport.o: hostport.c afblib/hostport.h
shared/strlist.o: strlist.c afblib/strlist.h
static/strlist.o: strlist.c afblib/strlist.h
shared/outbuf_printf.o: outbuf_printf.c afblib/outbuf_printf.h afblib/outbuf.h
static/outbuf_printf.o: outbuf_printf.c afblib/outbuf_printf.h afblib/outbuf.h
shared/inbuf.o: inbuf.c afblib/inbuf.h
static/inbuf.o: inbuf.c afblib/inbuf.h
shared/multiplexor.o: multiplexor.c afblib/multiplexor.h
static/multiplexor.o: multiplexor.c afblib/multiplexor.h
shared/inbuf_readline.o: inbuf_readline.c afblib/inbuf_readline.h afblib/inbuf.h
static/inbuf_readline.o: inbuf_readline.c afblib/inbuf_readline.h afblib/inbuf.h
shared/udp_session.o: udp_session.c afblib/udp_session.h afblib/hostport.h
static/udp_session.o: udp_session.c afblib/udp_session.h afblib/hostport.h
shared/pconnect.o: pconnect.c afblib/pconnect.h
static/pconnect.o: pconnect.c afblib/pconnect.h
shared/service.o: service.c afblib/service.h afblib/hostport.h
static/service.o: service.c afblib/service.h afblib/hostport.h
