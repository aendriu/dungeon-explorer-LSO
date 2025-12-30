CC=gcc
CFLAGS=-Wall -Wextra -O2 -g

SRCDIR=server
BINARY=$(SRCDIR)/server
SRC=$(SRCDIR)/main.c

all: $(BINARY)

$(BINARY): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINARY)

.PHONY: all clean