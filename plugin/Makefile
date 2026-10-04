# Pocketwatch - kernel-mode PRX plugin (GAME runlevel)
#
#   make                 build pocketwatch.prx
#   make VERSION=v0.1.0  override the embedded version string
#   make clean

TARGET = pocketwatch
OBJS   = src/core/main.o src/core/log.o src/core/ring.o src/core/platform_psp.o \
         lib/stb_sprintf.o exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# Kernel plugin: link only against kernel libraries (no newlib).
USE_KERNEL_LIBS = 1

INCDIR  = src lib
CFLAGS  = -Os -G0 -Wall -Wextra -fno-pic -fno-builtin-printf -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS   = -lgcc

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

# Third-party code: keep upstream sources untouched, silence their known warnings.
lib/stb_sprintf.o: CFLAGS += -Wno-sign-compare

# Rebuild main.o whenever the version string changes.
src/core/main.o: .version
.version: FORCE
	@echo '$(VERSION)' | cmp -s - $@ || echo '$(VERSION)' > $@

clean: clean-local
clean-local:
	-rm -f exports.c .version

.PHONY: FORCE clean-local
