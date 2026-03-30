CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

.PHONY: all clean

all: ar

ar: main.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f ar
