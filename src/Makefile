PACKAGE ?= gkrellm
PKG_CONFIG ?= pkg-config
BINMODE ?= 755
BINEXT ?=

INSTALLROOT ?= $(DESTDIR)$(PREFIX)
ifeq ($(INSTALLROOT),)
	INSTALLROOT = /usr/local
endif

INSTALLDIR ?= $(INSTALLROOT)/bin
INSTALLDIRMODE ?= 755
INCLUDEDIR ?= $(INSTALLROOT)/include
INCLUDEMODE ?= 644
INCLUDEDIRMODE ?= 755
LIBDIR ?= $(INSTALLROOT)/lib
LIBDIRMODE ?= 755
MANDIR ?= $(INSTALLROOT)/share/man/man1
MANMODE ?= 644
MANDIRMODE ?= 755
INSTALL ?= install
LINK_FLAGS ?= -Wl,-E
X11_LIBS ?= -L/usr/X11R6/lib -lX11 -lSM -lICE
MATH_LIB ?= -lm

SHARED_PATH = ../shared
# Make GNU Make search for sources somewhere else as well
VPATH = $(SHARED_PATH)


ifeq ($(without-gnutls),1)
	CONFIGURE_ARGS += --without-gnutls
endif
ifeq ($(without-gnutls),yes)
	CONFIGURE_ARGS += --without-gnutls
endif
ifeq ($(without-ssl),1)
	CONFIGURE_ARGS += --without-ssl
endif
ifeq ($(without-ssl),yes)
	CONFIGURE_ARGS += --without-ssl
endif
ifeq ($(without-libsensors),yes)
	CONFIGURE_ARGS += --without-libsensors
endif
ifeq ($(without-libsensors),1)
	CONFIGURE_ARGS += --without-libsensors
endif
ifeq ($(without-ntlm),yes)
	CONFIGURE_ARGS += --without-ntlm
endif
ifeq ($(without-ntlm),1)
	CONFIGURE_ARGS += --without-ntlm
endif
# run configure shell script
DUMMY_VAR := $(shell ./configure $(CONFIGURE_ARGS))
# pull in variables set by configure script
include configure.mk

# Only use own md5-code if neither OpenSSL nor GnuTLS are present
ifneq ($(HAVE_SSL),1)
    EXTRAOBJS ?= md5c.o
endif

CC ?= gcc
WINDRES ?= windres
STRIP ?= -s

GKRELLM_INCLUDES = gkrellm.h gkrellm-public-proto.h $(SHARED_PATH)/log.h

PKG_INCLUDE = `$(PKG_CONFIG) --cflags gtk+-2.0 gmodule-2.0 gthread-2.0`
PKG_LIB = `$(PKG_CONFIG) --libs gtk+-2.0 gmodule-2.0 gthread-2.0`

FLAGS = -O2 -I.. -I$(SHARED_PATH) $(PKG_INCLUDE) $(GTOP_INCLUDE) $(PTHREAD_INC)\
 ${SSL_INCLUDE} ${NTLM_INCLUDE} -DGKRELLM_CLIENT

LIBS = $(PKG_LIB) $(GTOP_LIBS) $(X11_LIBS) $(SYS_LIBS) $(SSL_LIBS) $(SENSORS_LIBS)\
 $(NTLM_LIBS) $(MATH_LIB)

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

ifneq ($(PACKAGE),gkrellm)
    FLAGS += -DPACKAGE=\"$(PACKAGE)\"
endif

ifeq ($(HAVE_GETADDRINFO),1)
    FLAGS += -DHAVE_GETADDRINFO
endif


OBJS =	main.o alerts.o battery.o base64.o clock.o cpu.o disk.o fs.o \
	hostname.o inet.o mail.o mem.o net.o proc.o sensors.o uptime.o \
	chart.o panel.o config.o gui.o krell.o plugins.o pixops.o \
	client.o utils.o sysdeps-unix.o deprecated.o log.o

UNIXOBJS = winops-x11.o

all:	gkrellm

gkrellm: check_env $(OBJS) $(UNIXOBJS) $(EXTRAOBJS)
	$(CC) $(OBJS) $(UNIXOBJS) $(EXTRAOBJS) -o gkrellm \
		$(LDFLAGS) $(LIBS) $(LINK_FLAGS)

static: check_env $(OBJS) $(UNIXOBJS) $(EXTRAOBJS)
	$(CC) $(OBJS) $(UNIXOBJS) $(EXTRAOBJS) -o gkrellm.static -static \
		$(LDFLAGS) $(LIBS) $(LINK_FLAGS)

freebsd2:
ifeq ($(HAVE_SSL),1)
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm" gkrellm
else
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm -lmd" gkrellm
endif

freebsd3 freebsd:
ifeq ($(HAVE_SSL),1)
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm -ldevstat" gkrellm
else
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm -ldevstat -lmd" gkrellm
endif

# Darwin target for systems that still have libkvm
darwin: 
ifeq ($(HAVE_SSL),1)
	$(MAKE) STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= \
		SYS_LIBS="-framework CoreFoundation -framework IOKit -lkvm" \
		LINK_FLAGS="-Wl,-bind_at_load" \
		gkrellm
else
	$(MAKE) STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= \
		SYS_LIBS="-framework CoreFoundation -framework IOKit -lkvm -lmd5" \
		LINK_FLAGS="-Wl,-bind_at_load" \
		gkrellm
endif

# Mac OS X target with X11-based Gtk+
darwin9: 
	$(MAKE) STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS= \
		SYS_LIBS="-framework CoreFoundation -framework IOKit" \
		LINK_FLAGS="-Wl,-bind_at_load" \
		gkrellm

# Mac OS X target with Quartz-based Gtk+ (experimental)
macosx: 
	$(MAKE) STRIP= HAVE_GETADDRINFO=1 \
		EXTRAOBJS="winops-gtk-mac.o" \
		SYS_LIBS="-framework CoreFoundation -framework IOKit" \
		LINK_FLAGS="-Wl,-bind_at_load" \
		X11_LIBS="" \
		UNIXOBJS="" \
		gkrellm

netbsd1:
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm" \
		X11_LIBS="-L/usr/X11R6/lib -lX11 -lSM -lICE -Wl,-R/usr/X11R6/lib" \
		gkrellm
netbsd2:
	$(MAKE) EXTRAOBJS= SYS_LIBS="-lkvm -pthread" \
		X11_LIBS="-L/usr/X11R6/lib -lX11 -lSM -lICE -R/usr/X11R6/lib" \
		gkrellm

openbsd:
	$(MAKE) GTOP_LIBS= SYS_LIBS="-lkvm -pthread" gkrellm

solaris:
	$(MAKE) CFLAGS="-Wno-implicit-int" \
		SYS_LIBS="-lkstat -lkvm -ldevinfo" gkrellm

windows: libgkrellm.a
	$(MAKE) \
		CFLAGS="${CFLAGS} -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN" \
		LINK_FLAGS="${LINK_FLAGS} -mwindows" \
		EXTRAOBJS="${EXTRAOBJS} winops-win32.o win32-plugin.o win32-resource.o" \
		SYS_LIBS="-lws2_32 -lwtsapi32 -lpdh -lnetapi32 -liphlpapi -lintl" \
		X11_LIBS="" \
		UNIXOBJS="" \
		gkrellm

install: install_bin install_inc install_man

install_bin:
	$(INSTALL) -d -m $(INSTALLDIRMODE) $(INSTALLDIR)
	$(INSTALL) -c $(STRIP) -m $(BINMODE) $(PACKAGE)$(BINEXT) $(INSTALLDIR)/$(PACKAGE)$(BINEXT)

install_inc:
	$(INSTALL) -d -m $(INCLUDEDIRMODE) $(INCLUDEDIR)/gkrellm2
	$(INSTALL) -c -m $(INCLUDEMODE) $(GKRELLM_INCLUDES) $(INCLUDEDIR)/gkrellm2

install_man:
	$(INSTALL) -d -m $(MANDIRMODE) $(MANDIR)
	$(INSTALL) -c -m $(MANMODE) ../gkrellm.1 $(MANDIR)/$(PACKAGE).1

uninstall:
	$(RM) $(INSTALLDIR)/$(PACKAGE)
	$(RM) -r $(INCLUDEDIR)/gkrellm2
	$(RM) $(MANDIR)/$(PACKAGE).1
	$(RM) $(LIBDIR)/libgkrellm.a

install_darwin install_darwin9 install_macosx:
	$(MAKE) install STRIP=

install_freebsd:
	$(MAKE) install
	chgrp kmem $(INSTALLDIR)/$(PACKAGE)
	chmod g+s $(INSTALLDIR)/$(PACKAGE)

install_netbsd:
	$(MAKE) MANDIR="$(INSTALLROOT)/man/man1" install

install_openbsd:
	$(MAKE) install
	chgrp kmem $(INSTALLDIR)/$(PACKAGE)
	chmod g+sx $(INSTALLDIR)/$(PACKAGE)

install_solaris:
	$(MAKE) install INSTALL=/usr/ucb/install
	chgrp sys $(INSTALLDIR)/$(PACKAGE)
	chmod g+s $(INSTALLDIR)/$(PACKAGE)  

install_windows: windows
	$(MAKE) BINEXT=".exe" install_bin install_inc
	$(INSTALL) -d -m $(LIBDIRMODE) $(LIBDIR)
	$(INSTALL) -c -m $(BINMODE) libgkrellm.a $(LIBDIR)

clean:
	$(RM) *.o *~ *.bak configure.h configure.mk configure.log gkrellm gkrellm.exe \
		libgkrellm.a sysdeps/*.o core

IMAGES = \
	../pixmaps/frame_top.xpm \
	../pixmaps/frame_bottom.xpm \
	../pixmaps/frame_left.xpm \
	../pixmaps/frame_right.xpm \
	\
	../pixmaps/button_panel_out.xpm \
	../pixmaps/button_panel_in.xpm \
	../pixmaps/button_meter_out.xpm \
	../pixmaps/button_meter_in.xpm \
	\
	../pixmaps/bg_chart.xpm \
	../pixmaps/bg_grid.xpm  \
	../pixmaps/bg_panel.xpm \
	../pixmaps/bg_meter.xpm \
	\
	../pixmaps/data_in.xpm \
	../pixmaps/data_in_grid.xpm \
	../pixmaps/data_out.xpm \
	../pixmaps/data_out_grid.xpm \
	\
	../pixmaps/net/decal_net_leds.xpm \
	../pixmaps/decal_misc.xpm \
	../pixmaps/decal_alarm.xpm \
	../pixmaps/decal_warn.xpm \
	\
	../pixmaps/krell_panel.xpm \
	../pixmaps/krell_meter.xpm \
	../pixmaps/krell_slider.xpm \
	../pixmaps/krell_mini.xpm \
	../pixmaps/fs/bg_panel.xpm \
	../pixmaps/host/bg_panel.xpm \
	../pixmaps/mail/decal_mail.xpm \
	../pixmaps/mail/krell_mail.xpm \
	../pixmaps/mail/krell_mail_daemon.xpm \
	../pixmaps/timer/bg_panel.xpm \
	../pixmaps/timer/bg_timer.xpm \
	../pixmaps/timer/decal_timer_button.xpm \
	../pixmaps/uptime/bg_panel.xpm \
	\
	../pixmaps/gkrellmms/bg_panel.xpm \
	../pixmaps/gkrellmms/spacer_top.xpm \
	../pixmaps/gkrellmms/spacer_bottom.xpm \
	../pixmaps/gkrellmms/bg_scroll.xpm \
	../pixmaps/pmu/bg_panel.xpm \
	../pixmaps/pmu/spacer_top.xpm \
	../pixmaps/pmu/spacer_bottom.xpm \
	../pixmaps/volume/bg_panel.xpm \
	../pixmaps/volume/spacer_top.xpm \
	../pixmaps/volume/spacer_bottom.xpm \
	../pixmaps/bg_separator.xpm

SYSDEPS_SRC = sysdeps/bsd-common.c sysdeps/bsd-net-open.c sysdeps/freebsd.c \
	sysdeps/gtop.c sysdeps/linux.c sysdeps/netbsd.c sysdeps/openbsd.c \
	sysdeps/solaris.c sysdeps/darwin.c sysdeps/sensors-common.c \
	sysdeps/dragonfly.c sysdeps/win32.c

GKRELLM_H = gkrellm.h gkrellm-private.h
GKRELLM_H_SYS = gkrellm.h gkrellm-public-proto.h gkrellm-private.h \
	gkrellm-sysdeps.h

main.o:      main.c $(GKRELLM_H)
alerts.o:    alerts.c $(GKRELLM_H)
battery.o:   battery.c $(GKRELLM_H_SYS)
base64.o:    base64.c
clock.o:     clock.c  $(GKRELLM_H_SYS)
cpu.o:	     cpu.c  $(GKRELLM_H_SYS)
disk.o:      disk.c $(GKRELLM_H_SYS)
fs.o:        fs.c $(GKRELLM_H_SYS)
hostname.o:  hostname.c $(GKRELLM_H_SYS)
inet.o:      inet.c $(GKRELLM_H_SYS)
mail.o:      mail.c md5.h $(GKRELLM_H_SYS)
md5c.o:      md5.h
mem.o:       mem.c  $(GKRELLM_H_SYS)
net.o:	     net.c  $(GKRELLM_H_SYS)
proc.o:      proc.c  $(GKRELLM_H_SYS)
sensors.o:   sensors.c $(GKRELLM_H_SYS) ../pixmaps/sensors/bg_volt.xpm
uptime.o:    uptime.c $(GKRELLM_H_SYS)
chart.o:     chart.c  $(GKRELLM_H)
panel.o:     panel.c  $(GKRELLM_H)
config.o:    config.c  $(GKRELLM_H) $(IMAGES)
krell.o:     krell.c  $(GKRELLM_H)
gui.o:       gui.c  $(GKRELLM_H)
plugins.o:   plugins.c $(GKRELLM_H)
pixops.o:    pixops.c $(GKRELLM_H)
client.o:    client.c $(GKRELLM_H)
utils.o:     utils.c $(GKRELLM_H)
sysdeps-unix.o: sysdeps-unix.c $(GKRELLM_H_SYS) $(SYSDEPS_SRC)
log.o: $(SHARED_PATH)/log.c $(SHARED_PATH)/log.h $(GKRELLM_H)
deprecated.o: deprecated.c $(GKRELLM_H)

winops-x11.o: winops-x11.c $(GKRELLM_H)
winops-gtk-mac.o: winops-gtk-mac.c $(GKRELLM_H)
winops-win32.o: winops-win32.c $(GKRELLM_H)

win32-plugin.o: win32-plugin.c win32-plugin.h
win32-libgkrellm.o: win32-libgkrellm.c win32-plugin.h
win32-resource.o: win32-resource.rc win32-resource.h
	$(WINDRES) $< -o $@

libgkrellm.a: win32-libgkrellm.o
	ar -cr libgkrellm.a win32-libgkrellm.o

# Checks if the build environment is ok
check_env:
	$(PKG_CONFIG) --atleast-version=2.4 gtk+-2.0

%.o: %.c
	$(CC) -c -Wall $(FLAGS) $(CFLAGS) $(CPPFLAGS) $< -o $@
