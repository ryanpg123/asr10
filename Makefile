# asr-10 tools
# (c) by K.M.Indlekofer (version 2/11/97)
# Makefile for GNU-C-compiler
#
# Copyright (c) 2008-2011 K.M.Indlekofer
#
# m.indlekofer@gmx.de
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.



VERSION	=	1.1

#TEMPLATEPATH	=	$(PREFIX)/libdata/asr10.template
TEMPLATEPATH	=	eps.template



all:	asr10 wav2asr

install:	asr10 wav2asr asr10.template
	$(INSTALL) asr10 $(PREFIX)/bin/ ;\
	$(INSTALL) wav2asr $(PREFIX)/bin/ ;\
	$(INSTALL) asr10.template $(PREFIX)/libdata/

clean:
	rm -f asr10 asr10.exe wav2asr wav2asr.exe *.o

back:
	mkdir -p asr10-$(VERSION);\
	cp -p Makefile README.txt gpl*.txt *.template *.c *.h asr10-$(VERSION);\
	tar -clvpzf asr10-$(VERSION).tar.gz asr10-$(VERSION)



asr10:	epsfs_main.c epsfs.c epsfs_core.c epsfs.h epsfs_core.h epsfs_struct.h
	$(CC) $(CFLAGS) -o $@ epsfs_main.c epsfs.c epsfs_core.c

wav2asr:	wav2asr.c
	$(CC) $(CFLAGS) -o $@ wav2asr.c -DTEMPLATE=\"$(TEMPLATEPATH)\"



# End of Makefile
