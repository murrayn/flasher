
CFLAGS = -Wall -O

CC = gcc
DESTDIR=/usr/local

all: flasher man

flasher: flasher.c flasher.h util.c
	${CC} ${CFLAGS} -o flasher flasher.c util.c

flasher.1.gz: flasher.1
	gzip < flasher.1 > flasher.1.gz

man: flasher.1.gz

install: flasher man
	strip flasher
	install flasher $(DESTDIR)/sbin/flasher
	install flasher.1.gz $(DESTDIR)/man/man1
	
clean:
	rm -f *.o flasher flasher.1.gz

