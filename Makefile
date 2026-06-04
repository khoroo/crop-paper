PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

CC       ?= gcc
CFLAGS   ?= -std=c23 -Wall -Wextra -pedantic -O2
CFLAGS   += $(shell pkg-config --cflags raylib) $(shell pkg-config --cflags fontconfig)
LDFLAGS  ?= $(shell pkg-config --libs raylib) -lm $(shell pkg-config --libs fontconfig)

SRC = src/crop-paper.c
OBJ = src/crop-paper.o

.PHONY: all clean install uninstall

all: crop-paper

crop-paper: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/crop-paper.o: src/crop-paper.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f crop-paper src/crop-paper.o

install: crop-paper
	mkdir -p $(DESTDIR)$(BINDIR)
	cp crop-paper $(DESTDIR)$(BINDIR)/crop-paper

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/crop-paper
