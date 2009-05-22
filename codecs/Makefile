#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for codec modules
#
# Copyright (C) 1999-2006, Digium, Inc.
#
# Mark Spencer <markster@digium.com>
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

-include $(ASTTOPDIR)/menuselect.makeopts $(ASTTOPDIR)/menuselect.makedeps

MODULE_PREFIX=codec
MENUSELECT_CATEGORY=CODECS
MENUSELECT_DESCRIPTION=Codec Translators

LIBILBC=-lilbc
LIBLPC10=-llpc10

all: _all

include $(ASTTOPDIR)/Makefile.moddir_rules

ifneq ($(GSM_INTERNAL),no)
GSM_INCLUDE:=-Igsm/inc
$(if $(filter codec_gsm,$(EMBEDDED_MODS)),modules.link,codec_gsm.so): gsm/lib/libgsm.a
endif

clean::
	$(MAKE) -C gsm clean
	rm -f g722/*.[oa]

gsm/lib/libgsm.a:
	@mkdir -p gsm/lib
	@$(MAKE) -C gsm lib/libgsm.a

$(if $(filter codec_lpc10,$(EMBEDDED_MODS)),modules.link,codec_lpc10.so): $(LIBLPC10)

$(if $(filter codec_ilbc,$(EMBEDDED_MODS)),modules.link,codec_ilbc.so): $(LIBILBC)

$(if $(filter codec_g722,$(EMBEDDED_MODS)),modules.link,codec_g722.so): g722/g722_encode.o g722/g722_decode.o
