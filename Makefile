SHELL := /bin/sh
PREFIX := /usr/local
DESTDIR ?=

LIBS := -lc -lxcb -lxcb-randr -lasound
CFLAGS = -std=c99 -pedantic -Wall -O2 $(INCS)
LDFLAGS = -s $(LIBS)

TARGET := dwm-statusline
SRCS = $(TARGET).c
OBJS = $(subst .c,.o,$(SRCS))

.SUFFIXES: .c .o
.PHONY: all clean install uninstall

all: $(TARGET)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(RM) $(OBJS) $(TARGET)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(TARGET) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

