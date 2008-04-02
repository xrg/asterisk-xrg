#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for Menuselect
#
# Copyright (C) 2005-2006, Digium, Inc.
#
# Russell Bryant <russell@digium.com>
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

.PHONY: clean dist-clean distclean

MENUSELECT_OBJS:=menuselect.o menuselect_curses.o strcompat.o
MENUSELECT_CFLAGS:=-g -c -D_GNU_SOURCE
MENUSELECT_LIBS:=mxml/libmxml.a

-include makeopts

ifneq ($(NCURSES_LIB),)
  MENUSELECT_LIBS+=$(NCURSES_LIB)
  MENUSELECT_INCLUDE:=$(NCURSES_INCLUDE)
else
 ifneq ($(CURSES_LIB),)
  MENUSELECT_LIBS+=$(CURSES_LIB)
  MENUSELECT_INCLUDE:=$(CURSES_INCLUDE)
 else
  MENUSELECT_OBJS:=$(filter-out menuselect_curses.o,$(MENUSELECT_OBJS)) menuselect_stub.o
 endif
endif

all: autoconfig.h
	@$(MAKE) menuselect

autoconfig.h:
	@./configure $(CONFIGURE_SILENT)

menuselect: mxml/libmxml.a $(MENUSELECT_OBJS)
	$(CC) -g -Wall -o $@ $(MENUSELECT_OBJS) $(MENUSELECT_LIBS)

menuselect.o: menuselect.c menuselect.h
	$(CC) -Wall $(CFLAGS) -o $@ $(MENUSELECT_CFLAGS) $<

menuselect_curses.o: menuselect_curses.c menuselect.h
	$(CC) -Wall $(CFLAGS) -o $@ $(MENUSELECT_CFLAGS) $(MENUSELECT_INCLUDE) $<

menuselect_stub.o: menuselect_stub.c menuselect.h
	$(CC) -Wall $(CFLAGS) -o $@ $(MENUSELECT_CFLAGS) $(MENUSELECT_INCLUDE) $<

strcompat.o: strcompat.c
	$(CC) -Wall $(CFLAGS) -o $@ $(MENUSELECT_CFLAGS) $<

mxml/libmxml.a:
	@$(MAKE) -C mxml libmxml.a

clean:
	rm -f menuselect *.o
	@if test -f mxml/Makefile ; then $(MAKE) -C mxml clean ; fi

dist-clean: distclean

distclean: clean
	@if test -f mxml/Makefile ; then $(MAKE) -C mxml clean ; fi
	rm -f autoconfig.h config.status config.log makeopts
	rm -rf autom4te.cache
