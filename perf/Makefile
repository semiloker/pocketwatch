# Pocketwatch's performance overlay in games - a kernel-mode PRX of its own (ADR-043), loaded by
# ARK-4 from PLUGINS.TXT only while Home's Settings -> Overlay in Games is on
#
#   make        build pocketwatch_perf.prx
#   make check  verify its imports and size budgets
#   make clean

TARGET = pocketwatch_perf
# The 8x8 font is the game plugin's (MIT, plugin/src/ui/font8x8.LICENSE), built again here.
OBJS   = build/main.o build/overlay.o build/font8x8.o build/stubs.o exports.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

# Kernel module: kernel libraries only (no newlib), as the game plugin.
USE_KERNEL_LIBS = 1

INCDIR  = src ../plugin/src
CFLAGS  = -Os -G0 -Wall -Wextra -Werror -fno-pic -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)
LIBS    = -lgcc

VPATH = src ../plugin/src/ui

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

build:
	mkdir -p build

build/%.o: %.c | build
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

build/%.o: %.S | build
	$(CC) $(ASFLAGS) -c -o $@ $<

-include $(wildcard build/*.d)

# Budgets for `make check`: it shares P1 with the game plugin and the game's drivers only while it
# is turned on (RISK-005): its loaded sections, the two lines' masks included, stay small.
MAX_PRX_SIZE = 16384
MAX_MEM      = 16384

check: $(FINAL_TARGET)
	python3 ../tools/check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../tools/nids/ark4_660.tsv --allowlist ../tools/nids/allowlist.tsv \
		--max-prx-size $(MAX_PRX_SIZE) --max-mem $(MAX_MEM)

clean: clean-local
clean-local:
	-rm -rf build exports.c

.PHONY: check clean-local
