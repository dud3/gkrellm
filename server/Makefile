PACKAGE_D ?= gkrellmd
PKG_CONFIG ?= pkg-config
BINMODE ?= 755
BINEXT ?=

INSTALLROOT ?= $(DESTDIR)$(PREFIX)
ifeq ($(INSTALLROOT),)
	INSTALLROOT = /usr/local
endif

SINSTALLDIR ?= $(INSTALLROOT)/bin
INSTALLDIRMODE ?= 755

INCLUDEDIR ?= $(INSTALLROOT)/include
INCLUDEMODE ?= 644
INCLUDEDIRMODE ?= 755

LIBDIR ?= $(INSTALLROOT)/lib
LIBDIRMODE ?= 755

SMANDIR ?= $(INSTALLROOT)/share/man/man1
MANMODE ?= 644
MANDIRMODE ?= 755
INSTALL ?= install
LINK_FLAGS ?= -Wl,-E
EXTRAOBJS =


ifeq ($(without-libsensors),yes)
	CONFIGURE_ARGS += --without-libsensors
endif
ifeq ($(without-libsensors),1)
	CONFIGURE_ARGS += --without-libsensors
endif

DUMMY_VAR := $(shell ./configure $(CONFIGURE_ARGS))

HAVE_LIBSENSORS = $(shell grep -c HAVE_LIBSENSORS configure.h)
ifeq ($(HAVE_LIBSENSORS),1)
    SENSORS_LIBS ?= -lsensors
endif


CC ?= gcc
STRIP ?= -s

GKRELLMD_INCLUDES = gkrellmd.h

PKG_INCLUDE = `$(PKG_CONFIG) --cflags glib-2.0 gthread-2.0`
PKG_LIB = `$(PKG_CONFIG) --libs glib-2.0 gmodule-2.0 gthread-2.0`

GLIB12_INCLUDE = `glib-config --cflags`
GLIB12_LIB = `glib-config --libs glib gmodule`

FLAGS = -O2 -I.. $(PKG_INCLUDE) $(GTOP_INCLUDE)
ifeq ($(glib12),1)
FLAGS = -O2 -I.. $(GLIB12_INCLUDE) $(GTOP_INCLUDE)
endif
ifeq ($(glib12),yes)
FLAGS = -O2 -I.. $(GLIB12_INCLUDE) $(GTOP_INCLUDE)
endif

FLAGS+= $(PTHREAD_INC)

LIBS = $(PKG_LIB) $(GTOP_LIBS_D) $(SYS_LIBS) $(SENSORS_LIBS)

ifeq ($(glib12),1)
LIBS = $(GLIB12_LIB) $(GTOP_LIBS_D) $(SYS_LIBS) $(SENSORS_LIBS)
endif
ifeq ($(glib12),yes)
LIBS = $(GLIB12_LIB) $(GTOP_LIBS_D) $(SYS_LIBS) $(SENSORS_LIBS)
endif

ifeq ($(debug),1)
    FLAGS += -g
endif
ifeq ($(debug),yes)
    FLAGS += -g
endif

ifeq ($(profile),1)
    FLAGS += -g -pg
endif
ifeq ($(profile),yes)
    FLAGS += -g -pg
endif

ifeq ($(enable_nls),1)
    FLAGS += -DENABLE_NLS -DLOCALEDIR=\"$(LOCALEDIR)\"
endif
ifeq ($(enable_nls),yes)
    FLAGS += -DENABLE_NLS -DLOCALEDIR=\"$(LOCALEDIR)\"
endif

ifneq ($(PACKAGE_D),gkrellmd)
	FLAGS += -DPACKAGE_D=\"$(PACKAGE_D)\"
endif

ifeq ($(HAVE_GETADDRINFO),1)
    FLAGS += -DHAVE_GETADDRINFO
endif


override CC += -Wall $(FLAGS)

OS_NAME=$(shell uname -s)
OS_RELEASE=$(shell uname -r)

OBJS =	main.o monitor.o mail.o plugins.o glib.o utils.o sysdeps-unix.o

EXTRAOBJS =

all:	gkrellmd

gkrellmd: $(OBJS) $(EXTRAOBJS)
	$(CC) $(OBJS) $(EXTRAOBJS) -o gkrellmd $(LIBS) $(LINK_FLAGS)

static: $(OBJS) $(EXTRAOBJS)
	$(CC) $(OBJS) $(EXTRAOBJS) -o gkrellmd.static -static \
		$(LIBS) $(LINK_FLAGS)

freebsd2:
	$(MAKE) GTK_CONFIG=gtk12-config \
		EXTRAOBJS= SYS_LIBS="-lkvm -lmd" gkrellmd

freebsd3 freebsd:
	$(MAKE) GTK_CONFIG=gtk12-config \
		EXTRAOBJS= SYS_LIBS="-lkvm -ldevstat -lmd" gkrellmd

darwin: 
	$(MAKE) GTK_CONFIG=gtk-config STRIP= \
		EXTRAOBJS= SYS_LIBS="-lkvm -lmd5" \
		LINK_FLAGS="-flat_namespace -undefined warning" gkrellmd

macosx: 
	$(MAKE) STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= SYS_LIBS="-lkvm" \
		LINK_FLAGS="-flat_namespace -undefined warning" \
		gkrellmd

netbsd1:
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm" gkrellmd

netbsd2:
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm -pthread" gkrellmd

openbsd:
	$(MAKE) SYS_LIBS="-lkvm -pthread" gkrellmd

solaris:
ifeq ($(OS_RELEASE),5.8)
	$(MAKE) CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lsocket -lnsl -lintl" \
		LINK_FLAGS="" gkrellmd
else
	$(MAKE) CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lsocket -lnsl" \
		LINK_FLAGS="" gkrellmd
endif

windows: libgkrellmd.a
	$(MAKE) \
	CFLAGS="${CFLAGS} -D_WIN32_WINNT=0x0500 -DWINVER=0x0500" \
	LINK_FLAGS="${LINK_FLAGS} -mconsole" \
	EXTRAOBJS="win32-resource.o win32-plugin.o" \
	SYS_LIBS=" -llargeint -lws2_32 -lpdh -lnetapi32 -liphlpapi -lntdll -lintl" \
	gkrellmd

install: install_bin install_inc install_man

install_bin:
	$(INSTALL) -d -m $(INSTALLDIRMODE) $(SINSTALLDIR)
	$(INSTALL) -c $(STRIP) -m $(BINMODE) $(PACKAGE_D)$(BINEXT) $(SINSTALLDIR)/$(PACKAGE_D)$(BINEXT)

install_inc:
	$(INSTALL) -d -m $(INCLUDEDIRMODE) $(INCLUDEDIR)/gkrellm2
	$(INSTALL) -c -m $(INCLUDEMODE) $(GKRELLMD_INCLUDES) $(INCLUDEDIR)/gkrellm2

install_man:
	$(INSTALL) -d -m $(MANDIRMODE) $(SMANDIR)
	$(INSTALL) -c -m $(MANMODE) ../gkrellmd.1 $(SMANDIR)/$(PACKAGE_D).1

uninstall:
	rm -f $(SINSTALLDIR)/$(PACKAGE_D)
	rm -f $(SMANDIR)/$(PACKAGE_D).1

install_darwin install_macosx:
	$(MAKE) install STRIP=

install_freebsd:
	$(MAKE) install
	chgrp kmem $(SINSTALLDIR)/$(PACKAGE_D)
	chmod g+s $(SINSTALLDIR)/$(PACKAGE_D)

install_netbsd:
	$(MAKE) SMANDIR="$(INSTALLROOT)/man/man1" install

install_openbsd:
	$(MAKE) install
	chgrp kmem $(SINSTALLDIR)/$(PACKAGE_D)
	chmod g+sx $(SINSTALLDIR)/$(PACKAGE_D)

install_solaris:
	$(MAKE) install INSTALL=/usr/ucb/install
	chgrp sys $(SINSTALLDIR)/$(PACKAGE_D)
	chmod g+s $(SINSTALLDIR)/$(PACKAGE_D)  

install_windows:
	$(MAKE) BINEXT=".exe" install_bin install_inc
	$(INSTALL) -d -m $(LIBDIRMODE) $(LIBDIR)
	$(INSTALL) -c -m $(BINMODE) libgkrellmd.a $(LIBDIR)

clean:
	$(RM) *.o *~ *.bak configure.h configure.log gkrellmd gkrellmd.exe \
		libgkrellmd.a core

SYSDEPS = ../src/sysdeps/bsd-common.c ../src/sysdeps/bsd-net-open.c \
	../src/sysdeps/darwin.c \
	../src/sysdeps/freebsd.c ../src/sysdeps/gtop.c \
	../src/sysdeps/linux.c ../src/sysdeps/netbsd.c \
	../src/sysdeps/openbsd.c ../src/sysdeps/sensors-common.c \
	../src/sysdeps/solaris.c ../src/sysdeps/win32.c 

main.o:		main.c gkrellmd.h
monitor.o:	monitor.c gkrellmd.h 
mail.o:		mail.c gkrellmd.h
plugins.o:	plugins.c gkrellmd.h 
glib.o:		glib.c gkrellmd.h
utils.o:	utils.c gkrellmd.h
sysdeps-unix.o: sysdeps-unix.c gkrellmd.h ../src/gkrellm-sysdeps.h $(SYSDEPS)
win32-gui.o:	win32-gui.c
win32-plugin.o:	win32-plugin.c win32-plugin.h gkrellmd.h

win32-libgkrellmd.o: win32-libgkrellmd.c win32-plugin.h gkrellmd.h
libgkrellmd.a: win32-libgkrellmd.o
	ar -cr libgkrellmd.a win32-libgkrellmd.o

win32-resource.o: win32-resource.rc win32-resource.h
	windres -I.. -o win32-resource.o win32-resource.rc
