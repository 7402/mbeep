CFLAGS=-Wall -std=c99
BINDIR=/usr/local/bin
MANDIR=/usr/local/share/man/man1

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LINK_LIBS=-framework OpenAL
else
LINK_LIBS=-lopenal -lm
endif

mbeep : mbeep.c text.h text.c sound.h sound.c patterns.h patterns.c
	gcc $(CFLAGS) -o mbeep mbeep.c text.c sound.c patterns.c $(LINK_LIBS)

install : mbeep
	cp mbeep $(BINDIR)/
	mkdir -p $(MANDIR)
	cp mbeep.1 $(MANDIR)/

clean :
	rm -f mbeep *.o

distclean :
	rm -f mbeep *.o $(BINDIR)/mbeep $(MANDIR)/mbeep.1
