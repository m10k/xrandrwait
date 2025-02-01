DEPS = src
OUTPUT = xrandrwait
PHONY = $(DEPS) clean install uninstall

all: $(OUTPUT)

$(OUTPUT): $(DEPS)

$(DEPS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

install: $(DEPS)

uninstall: $(DEPS)

clean: $(DEPS)

.PHONY: $(PHONY)
