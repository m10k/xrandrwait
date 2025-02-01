ifeq ($(PREFIX), )
	PREFIX = /usr
endif

ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PHONY = all clean install uninstall

all:

install:
	mkdir -p $(DESTDIR)/$(MANPREFIX)/man1
	install --owner=root --group=root --mode=644 man/xrandrwait.1 $(DESTDIR)$(MANPREFIX)/man1/xrandrwait.1

uninstall:
	rm $(DESTDIR)$(MANPREFIX)/man1/xrandrwait.1

clean:

.PHONY: $(PHONY)
