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

# read local makeopts settings
-include makeopts

.PHONY: clean dist-clean distclean test ntest ctest gtest

# Basic set of sources and flags/libraries/includes
OBJS:=menuselect.o strcompat.o
CFLAGS:=-g -c -D_GNU_SOURCE -Wall

# Pick a curses library if available
ifneq ($(NCURSES_LIB),)
  C_OBJS += menuselect_curses.o
  C_LIBS +=$(NCURSES_LIB)
  C_INCLUDE += $(NCURSES_INCLUDE)
else
 ifneq ($(CURSES_LIB),)
    C_OBJS += menuselect_curses.o
    C_LIBS +=$(CURSES_LIB)
    C_INCLUDE += $(CURSES_INCLUDE)
  endif
endif

# Pick gtk library if available
ifneq ($(GTK2_LIB),)
  G_OBJS += menuselect_gtk.o
  G_LIBS += $(GTK2_LIB)
  G_INCLUDE += $(GTK2_INCLUDE)
endif

# Pick newt if available
ifneq ($(NEWT_LIB),)
  N_OBJS += menuselect_newt.o
  N_LIBS += $(NEWT_LIB)
  N_INCLUDE += $(NEWT_INCLUDE)
endif

ifneq ($(N_OBJS),)
  M_OBJS += $(N_OBJS)
  M_LIBS += $(N_LIBS)
  M_INCLUDE += $(N_INCLUDE)
else
  ifneq ($(C_OBJS),)
    M_OBJS += $(C_OBJS)
    M_LIBS += $(C_LIBS)
    M_INCLUDE += $(C_INCLUDE)
 else
    M_OBJS += menuselect_stub.o
 endif
endif

CFLAGS+= $(INCLUDE)
all:
	@$(MAKE) menuselect

$(OBJS) menuselect_gtk.o menuselect_curses.o menuselect_stub.o: autoconfig.h menuselect.h

makeopts autoconfig.h: autoconfig.h.in makeopts.in
	@./configure $(CONFIGURE_SILENT) CC= LD= AR= CFLAGS=

menuselect cmenuselect gmenuselect nmenuselect: mxml/libmxml.a

cmenuselect: $(OBJS) $(C_OBJS)
	$(CC) -o $@ $^ $(C_LIBS)

gmenuselect: $(OBJS) $(G_OBJS)
	$(CC) -o $@ $^ $(G_LIBS)

nmenuselect: $(OBJS) $(N_OBJS)
	$(CC) -o $@ $^ $(N_LIBS)

menuselect: $(OBJS) $(M_OBJS)
	$(CC) -o $@ $^ $(M_LIBS)

mxml/libmxml.a:
	@if test ! -f mxml/Makefile ; then cd mxml && ./configure ; fi
	@$(MAKE) -C mxml libmxml.a

test: menuselect
	(cd test; ../$< menuselect.makeopts)

ctest: cmenuselect
	(cd test; ../$< menuselect.makeopts)

gtest: gmenuselect
	(cd test; ../$< menuselect.makeopts)

ntest: nmenuselect
	(cd test; ../$< menuselect.makeopts)

clean:
	rm -f menuselect cmenuselect gmenuselect nmenuselect $(OBJS) $(M_OBJS) $(C_OBJS) $(G_OBJS) $(N_OBJS)
	@if test -f mxml/Makefile ; then $(MAKE) -C mxml clean ; fi

dist-clean: distclean

distclean: clean
	@if test -f mxml/Makefile ; then $(MAKE) -C mxml distclean ; fi
	rm -f autoconfig.h config.status config.log makeopts
	rm -rf autom4te.cache
