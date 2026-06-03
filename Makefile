.PHONY: all clean run

CFLAGS  := -std=c23 -Wall -Wextra -pedantic -O2 $(shell pkg-config --cflags raylib)
LDFLAGS := $(shell pkg-config --libs raylib) -lm

all: cropper

cropper: src/cropper.c
	gcc $(CFLAGS) $(CFLAGS_EXTRA) -include font_path.h -o $@ $< $(LDFLAGS)

clean:
	rm -f cropper font_path.h

run: cropper
	./cropper
