# Pocketwatch - kernel-mode PRX plugin (GAME runlevel)
#
#   make                 build pocketwatch.prx
#   make VERSION=v0.1.0  override the embedded version string
#   make clean

TARGET = pocketwatch
OBJS   = src/main.o src/log.o exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# Kernel plugin: link only against kernel libraries (no newlib).
USE_KERNEL_LIBS = 1

INCDIR  = src
CFLAGS  = -Os -G0 -Wall -Wextra -fno-pic -fno-builtin-printf -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS   =

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

# Rebuild main.o whenever the version string changes.
src/main.o: .version
.version: FORCE
	@echo '$(VERSION)' | cmp -s - $@ || echo '$(VERSION)' > $@

clean: clean-local
clean-local:
	-rm -f exports.c .version

.PHONY: FORCE clean-local
