OUTPUT = xrandrwait
OBJECTS = xrandrwait.o
CFLAGS = -std=c99 -pedantic -Wall -O2
LDFLAGS = -lX11 -lXrandr
PHONY = clean install uninstall

ifeq ($(PREFIX), )
	PREFIX = /usr
endif
ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -g
	LDFLAGS += -g
endif

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install --owner=root --group=root --mode=755 $(OUTPUT) $(DESTDIR)$(PREFIX)/bin/.

uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(OUTPUT)

clean:
	rm -rf $(OUTPUT) $(OBJECTS)

.PHONY: $(PHONY)
