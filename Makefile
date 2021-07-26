System :=	$(shell uname)

ifeq ($(System), SunOS)
CC :=		gcc
CPPFLAGS :=	-I. -D_XOPEN_SOURCE=600 -D__EXTENSIONS__
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
# directory where the shared library is to be found:
#  - this gets hard-wired into the binary
#  - by default the current directory is chosen
LIBDIR :=	$(shell pwd)
# configuration for brew:
ifneq ($(wildcard /opt/homebrew/.*),)
BREW_ROOT :=	/opt/homebrew
else
BREW_ROOT :=	/usr/local/opt
endif
ifneq ($(wildcard $(BREW_ROOT)/libowfat/.*),)
LIBOWFAT_ROOT := $(BREW_ROOT)/libowfat
else
LIBOWFAT_ROOT := $(BREW_ROOT)
endif
LIBOWFAT_INCL := $(LIBOWFAT_ROOT)/include/libowfat
LIBOWFAT_LIBD := $(LIBOWFAT_ROOT)/lib
ifneq ($(wildcard $(BREW_ROOT)/pcre/.*),)
LIBPCRE_ROOT :=	$(BREW_ROOT)/pcre
else
LIBPCRE_ROOT :=	$(BREW_ROOT)
endif
LIBPCRE_INCL :=	$(LIBPCRE_ROOT)/include
LIBPCRE_LIBD := $(LIBPCRE_ROOT)/lib
#
CC :=		gcc
CPPFLAGS :=	-I. -I$(LIBOWFAT_INCL) -I$(LIBPCRE_INCL)
CFLAGS :=	-std=gnu11 -Wall -Wno-parentheses -g
LDLIBS :=	-L$(LIBOWFAT_LIBD) -L$(LIBPCRE_LIBD) -lowfat -lpcre
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
WebRoot :=	http://www.mathematik.uni-ulm.de/numerik/soft2/ss21/afblib

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
ifeq ($(System), Darwin)
		install_name_tool -id $(LIBDIR)/libafb.dylib libafb.dylib
endif

manpages:	$(ManPages)
$(ManPages): $(WebDir)/%.html: %.c
		pod2html \
		   --htmlroot=$(WebRoot) \
		   --podroot=$(WebDir) \
		   --podpath=. \
		   --infile=$< \
		   --outfile=$@ \
		   --title="$*(3)" \
		   --quiet \
		   --noindex

Makefile:	$(Headers)
		@gcc-makedepend -p shared/ -p static/ $(CPPFLAGS) $(CFiles)
# DO NOT DELETE
