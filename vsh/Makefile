# Pocketwatch XMB plugin - user-mode PRX for the VSH runlevel (the "Pocketwatch" entry under
# Settings, docs/subsystems/vsh-integration.md)
#
#   make        build pocketwatch_xmb.prx
#   make check  verify its imports and size budgets
#   make clean

TARGET = pocketwatch_xmb

# The settings model and the texts are the game side's own portable code, built again here for
# user mode (into build/, so the kernel build's objects next to the sources stay apart).
OBJS = build/main.o build/xmb.o build/xsettings.o build/xlog.o build/xbattery.o build/xboot.o \
       build/platform_vsh.o build/stubs.o build/config.o build/i18n.o build/langnames.o \
       build/battmodel.o build/playtime.o build/bootpick.o build/crc32.o build/stb_sprintf.o \
       exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

INCDIR  = src ../plugin/src ../plugin/lib
# No newlib: src/platform_vsh.c has the few C functions used, so the compiler must not turn its
# loops back into calls to them.
CFLAGS  = -Os -G0 -Wall -Wextra -Werror -fno-pic -fno-builtin -fno-tree-loop-distribute-patterns \
          -DPW_VERSION=\"$(VERSION)\" -DPW_I18N_XMB
ASFLAGS = $(CFLAGS)
LDFLAGS = -nostdlib
LIBS    = -lpspuser -lpsputility -lpsppower -lpspctrl -lpsprtc -lpspmodinfo -lgcc

VPATH = src ../plugin/src/core ../plugin/src/ui ../plugin/lib

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

build:
	mkdir -p build

build/%.o: %.c | build
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

build/%.o: %.S | build
	$(CC) $(ASFLAGS) -c -o $@ $<

build/stb_sprintf.o: CFLAGS += -Wno-sign-compare

# main.o again whenever the version string changes (xmb.log's first line names the build).
build/main.o: build/.version
build/.version: FORCE | build
	@echo '$(VERSION)' | cmp -s - $@ || echo '$(VERSION)' > $@

-include $(wildcard build/*.d)

# Budgets for `make check`: the XMB's user memory is shared with the other VSH plugins. 64 KB
# since the Battery and Play Time pages (2026-10-17): the battery history (3 KB), the translation
# (8 KB buffer + 4 KB pool) and the shared model code.
MAX_PRX_SIZE = 57344
MAX_MEM      = 65536

check: $(FINAL_TARGET)
	python3 ../tools/check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../tools/nids/ark4_660.tsv --allowlist ../tools/nids/allowlist.tsv \
		--max-prx-size $(MAX_PRX_SIZE) --max-mem $(MAX_MEM)

clean: clean-local
clean-local:
	-rm -rf build exports.c

.PHONY: FORCE check clean-local
