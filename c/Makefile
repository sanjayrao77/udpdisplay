CFLAGS=-g -Wall -O2
all: udpdisplay
udpdisplay: udpdisplay.o
	gcc -o udpdisplay $<
table:
	scp Makefile *.sh udpdisplay.c table:src/udpdisplay/
clean:
	rm -f *.o udpdisplay
backup:
	tar -jcf - . | jbackup src.udpdisplay.tar.bz2
.PHONY: clean backup table
