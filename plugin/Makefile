# Pocketwatch - kernel-mode PRX plugin (UMD runlevel: retail games)
#
#   make                 build pocketwatch.prx (DEBUG=0 for the release build)
#   make check           verify import NIDs against ARK-4 6.61 and the size budgets
#   make VERSION=v0.1.0  override the embedded version string
#   make clean

TARGET = pocketwatch
OBJS   = src/core/main.o src/core/log.o src/core/ring.o src/core/platform_psp.o \
         src/core/selfcheck.o src/core/stubinfo.o src/core/freeze.o src/core/mem.o \
         src/core/crc32.o src/core/png.o src/core/flash.o src/core/stubs/SystemCtrlForKernel.o \
         src/core/stubs/scePower_driver.o src/core/stubs/sceSyscon_driver.o \
         src/complications/states/sfo.o src/complications/states/savedata_hash.o \
         src/complications/states/stream.o src/complications/states/engine.o \
         src/complications/states/handoff.o \
         src/core/lz.o src/ui/draw.o src/ui/font8x8.o src/ui/input.o src/ui/fb.o \
         src/ui/menu.o lib/stb_sprintf.o exports.o

# DEBUG=1 (default while developing) adds the hardware probe and its button triggers.
DEBUG ?= 1
ifeq ($(DEBUG),1)
OBJS   += src/debug/probe.o src/debug/bench.o
endif

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# Budgets enforced by `make check`; raise them deliberately, not by accident.
# 112 KB: LZ4 at -O2 (~29 KB of code), the in-game menu, the debug probe and benchmark (2026-10-16).
MAX_PRX_SIZE = 114688
MAX_MEM      = 131072

# Kernel plugin: link only against kernel libraries (no newlib).
USE_KERNEL_LIBS = 1

INCDIR  = src lib
CFLAGS  = -Os -G0 -Wall -Wextra -Werror -fno-pic -fno-builtin-printf -DPW_VERSION=\"$(VERSION)\"
ifeq ($(DEBUG),1)
CFLAGS += -DPW_DEBUG
endif
ASFLAGS = $(CFLAGS)

LIBDIR =
# Order matters: sceRtcGetCurrentTick resolves from the user library sceRtc (NIDs not randomized,
# as PSPFatSave does on 6.61); sceRtcSetAlarmTick exists only in sceRtc_driver (ARK-mapped).
# scePower_driver comes from src/core/stubs/scePower_driver.S, not -lpsppower_driver.
LIBS   = -lgcc -lpspge_driver -lpsprtc -lpsprtc_driver

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

# Third-party code: keep upstream sources untouched, silence their known warnings.
lib/stb_sprintf.o: CFLAGS += -Wno-sign-compare
# Hot loops at -O2 (the last -O wins): LZ4 (vendored, unused static entry points dropped), the
# checksums and the benchmark. The rest stays -Os.
src/core/lz.o: CFLAGS += -O2 -Wno-unused-function
src/core/crc32.o: CFLAGS += -O2
src/debug/bench.o: CFLAGS += -O2

# Rebuild main.o whenever the version string changes, and everything when the profile or libs change.
src/core/main.o: .version
.version: FORCE
	@echo '$(VERSION)' | cmp -s - $@ || echo '$(VERSION)' > $@
$(OBJS): .buildflags
.buildflags: FORCE
	@echo 'DEBUG=$(DEBUG) LIBS=$(LIBS)' | cmp -s - $@ || echo 'DEBUG=$(DEBUG) LIBS=$(LIBS)' > $@

check: $(FINAL_TARGET)
	python3 ../tools/check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../tools/nids/ark4_660.tsv --allowlist ../tools/nids/allowlist.tsv \
		--max-prx-size $(MAX_PRX_SIZE) --max-mem $(MAX_MEM)

clean: clean-local
clean-local:
	-rm -f exports.c .version .buildflags src/debug/*.o

.PHONY: FORCE check clean-local
