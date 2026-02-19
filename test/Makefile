CC = gcc
CFLAGS = -g

.PHONY: all clean

all: test

test: test.o
	${CC} -o test ${CFLAGS} test.o

test.o: test.c
	${CC} -c ${CFLAGS} test.c

clean:
	rm -rf test test.o

