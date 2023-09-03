VERSION := 0.0.1

PACKAGE := elm327diag

CC=gcc
CFLAGS=-Wall
#LDLIBS=-lserialport
LDFLAGS=-L/usr/local/include

all: compile

compile: $(PACKAGE)

distclean: clean
clean:
	$(RM) *.o *.swp $(PACKAGE) *.orig *.rej map *~

elm327diag: elm327diag.c elm327.c
	gcc $(CFLAGS) $(CPPFLAGS) -funsigned-char $^ $(LDLIBS) $(LDFLAGS) -o $@


install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(PACKAGE) $(DESTDIR)$(PREFIX)/bin

dist: clean
	rm -rf $(PACKAGE)
	mkdir $(PACKAGE)


.PHONY: all clean distclean compile install dist
