CC = gcc
CFLAGS = -g

.PHONY: all clean

NAMES = hello iver ialloc ifree getattr balloc
TARGETS = $(NAMES:=.x)

all: $(TARGETS)

%.x: %.o
	$(CC) -o $@ $(CFLAGS) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TARGETS)

