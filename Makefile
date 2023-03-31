ifdef GPIO
CFLAGS=-Wall -std=c11 -DGPIO=$(GPIO)
else
CFLAGS=-Wall -std=c99
endif

BINDIR=/usr/local/bin
MANDIR=/usr/local/share/man/man1

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LINK_LIBS=-framework OpenAL
else
ifdef GPIO
LINK_LIBS=-lm
else
LINK_LIBS=-lopenal -lm
endif
endif

ifdef GPIO
mbeep : mbeep.c text.h text.c sound.h sound.c patterns.h patterns.c tiny_gpio.c tiny_gpio.h
	gcc $(CFLAGS) -o mbeep mbeep.c text.c sound.c patterns.c tiny_gpio.c $(LINK_LIBS)
else
mbeep : mbeep.c text.h text.c sound.h sound.c patterns.h patterns.c
	gcc $(CFLAGS) -o mbeep mbeep.c text.c sound.c patterns.c $(LINK_LIBS)
endif


install : mbeep
	cp mbeep $(BINDIR)/
	mkdir -p $(MANDIR)
	cp mbeep.1 $(MANDIR)/

clean :
	rm -f mbeep *.o

distclean :
	rm -f mbeep *.o $(BINDIR)/mbeep $(MANDIR)/mbeep.1
