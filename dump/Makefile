CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I..
LDFLAGS = -L.. -laout

.PHONY: all clean

all: dump

dump: main.c ../libaout.a
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

clean:
	rm -f dump
