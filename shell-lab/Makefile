CC = gcc
CFLAGS = -Wall -pedantic -O2 -g -std=c11

all: gshell

gshell: main.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o gshell

test: gshell
	python3 shell_test.py

.PHONY: clean all test
