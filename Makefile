CFLAGS := -std=c99 \
	-Wall -Wextra -pedantic \
	${CFLAGS}

LDLIBS = -lalpm

all: alpm-dump
alpm-dump: alpm-dump.o

clean:
	${RM} alpm-dump *.o

.PHONY: clean install uninstall
