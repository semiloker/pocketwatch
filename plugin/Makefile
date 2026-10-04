# Pocketwatch - kernel-mode PRX plugin (GAME runlevel)
#
#   make                 build pocketwatch.prx
#   make check           verify import NIDs against ARK-4 6.61 and the size budgets
#   make VERSION=v0.1.0  override the embedded version string
#   make clean

TARGET = pocketwatch
OBJS   = src/core/main.o src/core/log.o src/core/ring.o src/core/platform_psp.o \
         src/core/selfcheck.o \
         lib/stb_sprintf.o exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# Budgets enforced by `make check`; raise them deliberately, not by accident.
MAX_PRX_SIZE = 65536
MAX_MEM      = 131072

# Kernel plugin: link only against kernel libraries (no newlib).
USE_KERNEL_LIBS = 1

INCDIR  = src lib
CFLAGS  = -Os -G0 -Wall -Wextra -Werror -fno-pic -fno-builtin-printf -DPW_VERSION=\"$(VERSION)\"
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

check: $(FINAL_TARGET)
	python3 ../tools/check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../tools/nids/ark4_660.tsv --allowlist ../tools/nids/allowlist.tsv \
		--max-prx-size $(MAX_PRX_SIZE) --max-mem $(MAX_MEM)

clean: clean-local
clean-local:
	-rm -f exports.c .version

.PHONY: FORCE check clean-local
