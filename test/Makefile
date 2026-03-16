CC = gcc
CFLAGS = -g

.PHONY: all clean

TESTS = test-hello test-ialloc test-balloc

all: $(TESTS)

test-%: test-%.o
	$(CC) -o $@ $(CFLAGS) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TESTS)

