#
# rdesktop: A Remote Desktop Protocol client
# Makefile
# Copyright (C) Matthew Chapman 1999-2001
#

# Configuration defaults

CC       = cc
CFLAGS   = -O2 -DKEYMAP_PATH=\"$(KEYMAP_PATH)\"
INCLUDES = -I/usr/X11R6/include
LDLIBS   = -L/usr/X11R6/lib -lX11 -lXext

PREFIX   = /usr/local
EPREFIX  = $(PREFIX)
BINDIR   = $(EPREFIX)/bin
# MANDIR   = $(PREFIX)/man
SHAREDIR = $(PREFIX)/share/rdesktop

KEYMAP_PATH = $(SHAREDIR)/keymaps/

RDPOBJ   = rdesktop.o tcp.o iso.o mcs.o secure.o licence.o rdp.o orders.o bitmap.o cache.o xwin.o xkeymap.o
CRYPTOBJ = crypto/rc4_enc.o crypto/rc4_skey.o crypto/md5_dgst.o crypto/sha1dgst.o crypto/bn_exp.o crypto/bn_mul.o crypto/bn_div.o crypto/bn_sqr.o crypto/bn_add.o crypto/bn_shift.o crypto/bn_asm.o crypto/bn_ctx.o crypto/bn_lib.o

include Makeconf  # local configuration


rdesktop: $(RDPOBJ) $(CRYPTOBJ)
	$(CC) $(CFLAGS) -o rdesktop $(RDPOBJ) $(CRYPTOBJ) $(LDDIRS) $(LDLIBS)

Makeconf:
	./configure

install: installbin installkeymaps

installbin: rdesktop
	mkdir -p $(BINDIR)
	cp rdesktop $(BINDIR)
	strip $(BINDIR)/rdesktop
	chmod 755 $(BINDIR)/rdesktop

# installman: rdesktop.1
#	mkdir -p $(MANDIR)/man1
#	cp rdesktop.1 $(MANDIR)/man1
#	chmod 755 $(MANDIR)/man1/rdesktop.1

installkeymaps:
	mkdir -p $(KEYMAP_PATH)
# Prevent copying the CVS directory
	cp keymaps/?? $(KEYMAP_PATH)
	cp keymaps/common $(KEYMAP_PATH)
	cp keymaps/modifiers $(KEYMAP_PATH)
	cp keymaps/README $(KEYMAP_PATH)
	chmod 644 $(KEYMAP_PATH)/*

proto:
	cproto -DMAKE_PROTO -o proto.h *.c

clean:
	rm -f *.o crypto/*.o *~ rdesktop

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

