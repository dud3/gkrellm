# Makefile for locale directory

PACKAGE ?= gkrellm
CWD = po
MSGFMT = msgfmt
SUB_DIRS = 
FILES_PO:=$(wildcard *.po)
FILES_MO:=$(FILES_PO:.po=.mo)

LOCALEDIR ?= /usr/share/locale
MSGFMT_OPT ?= -f -v -o

ifeq ($(enable_nls),yes)
do_nls:=1
endif
ifeq ($(enable_nls),1)
do_nls:=1
endif

ifeq ($(do_nls),1)
all: mo-files

mo-files: $(FILES_MO)


messages:
	xgettext -k_ -kN_ ../src/*.c ../src/sysdeps/*.c -o gkrellm.pot

merge:
	for f in $(FILES_PO) ; do \
		msgmerge --no-fuzzy-matching $$f gkrellm.pot > $$f.new; \
		mv $$f.new $$f; done

install: 
	$(MAKE) all
	for f in $(FILES_MO) ; do mkdir -p \
		$(INSTALL_PREFIX)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES ; \
		cp $$f $(INSTALL_PREFIX)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES/$(PACKAGE).mo ; done

uninstall:
	for f in $(FILES_MO) ; do \
		rm -f $(INSTALL_PREFIX)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES/$(PACKAGE).mo ; done

%.mo: %.po
	$(MSGFMT) $(MSGFMT_OPT) $@ $<

else

all:

messages:

merge:

install:

uninstall:

endif


clean:
	$(RM) $(FILES_MO)

.PHONY: update install uninstall
