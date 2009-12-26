# To make GKrellM for different systems, you can simply:
# For Linux:
#	make
# For FreeBSD 2.X:
#	make freebsd2
# For FreeBSD 3.X or later:
#	make freebsd
# For NetBSD 1.5 - 1.6.X
#	make netbsd1
# For NetBSD 2.X
#	make netbsd2
# For OpenBSD
#	make openbsd
# For Darwin / Mac OS X
#   make darwin
# For Solaris 2.x (8 tested so far):
#	make solaris
# For libgtop if you have version 1.1.x installed:
#	make gtop
# For libgtop if you have version 1.0.x installed in /usr/include & /usr/lib,
# uncomment GTOP lines below:
#	make gtop1.0
# or, eg. if libgtop 1.0 is installed in /opt/gnome/include & /opt/gnome/lib
#	make gtop1.0 GTOP_PREFIX=/opt/gnome
#
# Then:
#	make install
# To override default install locations /usr/local/bin and /usr/local/include
# to, for example, /usr/bin and /usr/include:
#    make install INSTALLDIR=/usr/bin INCLUDEDIR=/usr/include
#
# ------------------------------------------------------------------
#  If you want to override the default behaviour for the above simple "make"
#  steps, then uncomment and edit the appropriate lines below.
#
# Default
#--------
EXTRAOBJS = md5c.o
BINMODE = 755

# FreeBSD 2.X
#------------
#SYS_LIBS = -lkvm
#EXTRAOBJS =
#BINMODE = 4111


# FreeBSD 3.X or later
#---------------------
#SYS_LIBS = -lkvm -ldevstat
#EXTRAOBJS =
#BINMODE = 4111

# NetBSD 1.5 - 1.6.X
#------------------
#SYS_LIBS=-lkvm
#EXTRAOBJS =
#MANDIR = $(INSTALLROOT)/man/man1

# NetBSD
#------------------
#SYS_LIBS=-lkvm -lpthread
#EXTRAOBJS =
#MANDIR = $(INSTALLROOT)/man/man1

# OpenBSD
#------------------
#SYS_LIBS=-lkvm -lpthread
#EXTRAOBJS =
#BINMODE=2755

# Solaris 2.x
#------------
#SYS_LIBS = -lkstat -lkvm -ldevinfo
#EXTRAOBJS = md5c.o
#BINMODE=2755
#LOCALEDIR = /usr/local/share/locale

# "make gtop1.0" defaults.  If you don't have gnome or libgtop 1.1.x
# installed, uncomment and edit these if necessary for a libgtop install.
# Or, see below about specifying them on the command line.  These are not
# used if you "make gnome-gtop" or "make gtop".
#-----------------------------------
#GTOP_PREFIX = /usr
#GTOP_INCLUDE = -I$(GTOP_PREFIX)/include
#GTOP_LIBS = -L$(GTOP_PREFIX)/lib -lgtop -lgtop_common -lgtop_sysdeps -lXau
#GTOP_LIBS_D = -L$(GTOP_PREFIX)/lib -lgtop -lgtop_common -lgtop_sysdeps
#export GTOP_INCLUDE GTOP_LIBS GTOP_LIBS_D

VERSION = 2.3.3

INSTALLROOT ?= $(DESTDIR)$(PREFIX)

ifeq ($(INSTALLROOT),)
	INSTALLROOT = /usr/local
endif

INSTALLDIR = $(INSTALLROOT)/bin
SINSTALLDIR ?= $(INSTALLDIR)
MANDIR ?= $(INSTALLROOT)/share/man/man1
SMANDIR ?= $(MANDIR)
MANMODE = 644
MANDIRMODE = 755
INCLUDEDIR = $(INSTALLROOT)/include
INCLUDEMODE = 644
INCLUDEDIRMODE = 755
INSTALL ?= $(shell which install)
PKGCONFIGDIR ?= $(INSTALLROOT)/lib/pkgconfig
LOCALEDIR ?= $(INSTALLROOT)/share/locale

OS_NAME=$(shell uname -s)
OS_RELEASE=$(shell uname -r)

export SYS_LIBS EXTRAOBJS BINMODE
export INSTALLDIR SINSTALLDIR INCLUDEDIR INCLUDEMODE INCLUDEDIRMODE LOCALEDIR
export MANDIR SMANDIR MANDIRMODE MANMODE
export OS_NAME OS_RELEASE

enable_nls=1
debug=0
export enable_nls
export debug

all gkrellm: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} gkrellm)
	(cd server && ${MAKE} gkrellmd)

# win32 needs a Libs: line and ${prefix} for paths so we install a different
# pkg-config file than what gets used on unix
# TODO: move to src/Makefile and install a gkrellmd.pc from server/Makefile
gkrellm.pc_win: Makefile
	echo "prefix=$(INSTALLROOT)" > gkrellm.pc
	echo "Name: GKrellM" >> gkrellm.pc
	echo "Description: Extensible GTK system monitoring application" >> gkrellm.pc
	echo "Version: $(VERSION)" >> gkrellm.pc
	echo "Requires: gtk+-2.0 >= 2.4.0" >> gkrellm.pc
	echo 'Cflags: -I$${prefix}/include' >> gkrellm.pc
	echo 'Libs: -L$${prefix}/lib -lgkrellm' >> gkrellm.pc

gkrellm.pc: Makefile
	echo "prefix=$(INSTALLROOT)" > gkrellm.pc
	echo "Name: GKrellM" >> gkrellm.pc
	echo "Description: Extensible GTK system monitoring application" >> gkrellm.pc
	echo "Version: $(VERSION)" >> gkrellm.pc
	echo "Requires: gtk+-2.0 >= 2.4.0" >> gkrellm.pc
	echo "Cflags: -I$(INCLUDEDIR)" >> gkrellm.pc

install: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install)
	(cd server && ${MAKE} install)

uninstall:
	(cd po && ${MAKE} uninstall)
	(cd src && ${MAKE} uninstall)
	(cd server && ${MAKE} uninstall)
	rm -f $(PKGCONFIGDIR)/gkrellm.pc

install_gkrellm.pc:
	$(INSTALL) -d $(PKGCONFIGDIR)
	$(INSTALL) -m $(INCLUDEMODE) -c gkrellm.pc $(PKGCONFIGDIR)

install_darwin install_darwin9 install_macosx: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install STRIP="")
	(cd server && ${MAKE} install STRIP="")

install_freebsd: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install_freebsd)
	(cd server && ${MAKE} install_freebsd)

install_netbsd: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install_netbsd)
	(cd server && ${MAKE} install_netbsd)

install_openbsd: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install_openbsd)
	(cd server && ${MAKE} install_openbsd)

install_solaris: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install_solaris)
	(cd server && ${MAKE} install_solaris)

install_windows: install_gkrellm.pc
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install_windows)
	(cd server && ${MAKE} install_windows)

clean:
	(cd po && ${MAKE} clean)
	(cd src && ${MAKE} clean)
	(cd server && ${MAKE} clean)
	rm -f gkrellm.pc

freebsd2:	gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} freebsd2)
	(cd server && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm -lmd" gkrellmd )

freebsd3 freebsd4 freebsd5 freebsd: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} freebsd)
	(cd server && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm -ldevstat -lmd" gkrellmd )

darwin: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} GTK_CONFIG=gtk-config STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= SYS_LIBS="-lkvm -framework IOKit" \
		LINK_FLAGS="-prebind -Wl,-bind_at_load -framework CoreFoundation -lX11" \
		gkrellm )
	(cd server && ${MAKE} GTK_CONFIG=gtk-config STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= SYS_LIBS="-lkvm -framework IOKit" \
		LINK_FLAGS="-prebind -Wl,-bind_at_load -framework CoreFoundation" \
		gkrellmd )

darwin9: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} GTK_CONFIG=gtk-config STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= SYS_LIBS="-framework IOKit" \
		LINK_FLAGS="-prebind -Wl,-bind_at_load -framework CoreFoundation -lX11" \
		gkrellm )
	(cd server && ${MAKE} GTK_CONFIG=gtk-config STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= SYS_LIBS="-framework IOKit" \
		LINK_FLAGS="-prebind -Wl,-bind_at_load -framework CoreFoundation" \
		gkrellmd )

macosx: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} macosx)
	(cd server && ${MAKE} macosx)

netbsd1: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm" \
		SMC_LIBS="-L/usr/X11R6/lib -lSM -lICE -Wl,-R/usr/X11R6/lib" \
		gkrellm )
	(cd server && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm" gkrellmd )

netbsd2: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm -pthread" \
		SMC_LIBS="-L/usr/X11R6/lib -lSM -lICE -R/usr/X11R6/lib" \
		gkrellm )
	(cd server && ${MAKE} \
		EXTRAOBJS= SYS_LIBS="-lkvm -pthread" gkrellmd )

openbsd: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} \
		PTHREAD_INC=-I${PREFIX}/include EXTRAOBJS= \
		SYS_LIBS="-lkvm -pthread" gkrellm )
	(cd server && ${MAKE} \
		PTHREAD_INC=-I${PREFIX}/include EXTRAOBJS= \
		SYS_LIBS="-lkvm -pthread" gkrellmd )

solaris: gkrellm.pc
	(cd po && ${MAKE} MSGFMT_OPT="-v -o" \
		LOCALEDIR=/usr/local/share/locale all)
ifeq ($(OS_RELEASE),5.8)
	(cd src && ${MAKE} CC=gcc \
		CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lresolv -lsocket -lX11 -lintl" \
		LINK_FLAGS="" gkrellm )
	(cd server && ${MAKE} CC=gcc \
		CFLAGS="-Wno-implicit-int -DSOLARIS_8" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lsocket -lnsl -lintl" \
		LINK_FLAGS="" gkrellmd )
else
	(cd src && ${MAKE} CC=gcc \
		CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lresolv -lsocket -lX11" LINK_FLAGS="" gkrellm )
	(cd server && ${MAKE} CC=gcc \
		CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo -lsocket -lnsl" LINK_FLAGS="" gkrellmd )
endif

gnome-gtop: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} GTOP_PREFIX="\`gnome-config --prefix libgtop\`" \
		GTOP_INCLUDE="\`gnome-config --cflags libgtop\`" \
		GTOP_LIBS="\`gnome-config --libs libgtop\`" \
		SYS_LIBS= gkrellm )
	(cd server && ${MAKE} GTOP_PREFIX="\`gnome-config --prefix libgtop\`" \
		GTOP_INCLUDE="\`gnome-config --cflags libgtop\`" \
		GTOP_LIBS_D="\`gnome-config --libs libgtop\`" \
		SYS_LIBS= gkrellmd )
 
gtop: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} GTOP_INCLUDE="\`libgtop-config --cflags\`" \
		GTOP_LIBS="\`libgtop-config --libs\`" \
		SYS_LIBS="-lXau" gkrellm )
	(cd server && ${MAKE} GTOP_INCLUDE="\`libgtop-config --cflags\`" \
		GTOP_LIBS_D="\`libgtop-config --libs\`" \
		SYS_LIBS= gkrellmd )

gtop1.0: gkrellm.pc
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} gkrellm )
	(cd server && ${MAKE} gkrellmd )

windows: gkrellm.pc_win
	(cd po && ${MAKE} LOCALEDIR="share/locale" all)
	(cd src && ${MAKE} LOCALEDIR="share/locale" windows )
	(cd server && ${MAKE} LOCALEDIR="share/locale" windows)

msgmerge:
	(cd po && ${MAKE} messages)
	(cd po && ${MAKE} merge)
