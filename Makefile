CFLAGS := -std=c99 \
	-Wall -Wextra -pedantic -O2 \
	${CFLAGS}

LDLIBS = -lalpm

all: alpm-dump
alpm-dump: alpm-dump.o termio.c

clean:
	${RM} alpm-dump *.o

.PHONY: clean install uninstall
