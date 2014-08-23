# makefile variables and rules shared by both gkrellm and gkrellmd

CC ?= gcc
AR ?= ar
PKG_CONFIG ?= pkg-config
WINDRES ?= windres

PREFIX ?= /usr/local
INSTALLROOT ?= $(DESTDIR)$(PREFIX)

BINMODE ?= 755
BINEXT ?=

INSTALLDIRMODE ?= 755

INCLUDEDIR ?= $(INSTALLROOT)/include
INCLUDEMODE ?= 644
INCLUDEDIRMODE ?= 755

LIBDIR ?= $(INSTALLROOT)/lib
LIBDIRMODE ?= 755

MANMODE ?= 644
MANDIRMODE ?= 755

INSTALL ?= install
STRIP ?= -s
LINK_FLAGS ?= -Wl,-E

SHARED_PATH = ../shared
# Make GNU Make search for sources somewhere else as well
VPATH = $(SHARED_PATH)

%.o: %.c
	$(CC) -c -Wall $(FLAGS) $(CFLAGS) $(CPPFLAGS) $< -o $@
