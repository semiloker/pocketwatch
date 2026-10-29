# xmbprobe - a research tool for M7's XMB side (docs/research/m7-xmb-context-menu.md): a VSH
# plugin that logs vsh_module's context-menu calls and dumps vsh_module and game_plugin_module.
# Not part of Pocketwatch: built by hand, never by scripts/build.sh, never in a release.
#
#   make        build xmbprobe.prx
#   make check  verify its imports
#   make clean

TARGET = xmbprobe
OBJS   = build/main.o build/stubs.o build/stb_sprintf.o exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# User mode with the SDK's small C library (strcmp, strncpy).
USE_PSPSDK_LIBC = 1

INCDIR  = ../../plugin/lib
CFLAGS  = -Os -G0 -Wall -Wextra -Werror -fno-pic -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)
LIBS    = -lgcc

VPATH = ../../plugin/lib

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

build:
	mkdir -p build

build/%.o: %.c | build
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

build/%.o: %.S | build
	$(CC) $(ASFLAGS) -c -o $@ $<

build/stb_sprintf.o: CFLAGS += -Wno-sign-compare

-include $(wildcard build/*.d)

check: $(FINAL_TARGET)
	python3 ../check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../nids/ark4_660.tsv --allowlist ../nids/allowlist.tsv \
		--max-prx-size 65536 --max-mem 65536

clean: clean-local
clean-local:
	-rm -rf build exports.c

.PHONY: check clean-local
