# Pocketwatch Home - a home screen of our own (homebrew application, docs/subsystems/home.md)
#
#   make        build EBOOT.PBP (install as ms0:/PSP/GAME/PocketwatchHome/EBOOT.PBP)
#   make clean

TARGET = pocketwatch_home

# Our drawing, fonts, texts and models are the plugin's own portable code, built again here
# (into build/, so the plugin's objects next to the sources stay apart).
OBJS = build/main.o build/hw.o build/library.o build/launch.o build/texts.o build/home_view.o \
       build/disc.o build/hm_image.o build/gfx_gu.o build/sprites.o build/usb.o build/usb_link.o \
       build/hsettings.o build/dialogs.o build/states.o build/platform_home.o build/music.o \
       build/music_name.o build/sound.o build/sound_synth.o build/stubs.o \
       build/draw.o build/shapes.o build/icons.o build/font8x8.o build/input.o build/i18n.o \
       build/text.o build/pgf.o build/sfo.o build/playtime.o build/crc32.o build/stb_sprintf.o \
       build/config.o build/langnames.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

INCDIR  = src lib ../plugin/src ../plugin/lib
CFLAGS  = -O2 -G0 -Wall -Wextra -Werror -DPW_I18N_HOME -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)
LIBS    = -lpspgu -lpspusb -lpspusbstor -lpspwlan -lpspmp3 -lpspaudio -lpsppower -lpsprtc -lpsputility

BUILD_PRX      = 1
PSP_FW_VERSION = 660
EXTRA_TARGETS  = EBOOT.PBP
PSP_EBOOT_TITLE = Pocketwatch Home
PSP_EBOOT_ICON  = res/ICON0.PNG
PSP_EBOOT_PIC1  = res/PIC1.PNG

VPATH = src ../plugin/src/ui ../plugin/src/gfx ../plugin/src/core \
        ../plugin/src/complications/states ../plugin/lib

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

build:
	mkdir -p build

build/%.o: %.c | build
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

build/%.o: %.S | build
	$(CC) $(ASFLAGS) -c -o $@ $<

# Third-party code keeps its own style.
build/hm_image.o: CFLAGS += -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter \
                            -Wno-implicit-fallthrough -Wno-unused-but-set-variable
build/stb_sprintf.o: CFLAGS += -Wno-sign-compare

# `make check`: imports (user libraries and ARK-4's, ../tools/check_prx.py) and the budgets. Home is
# a user program: its RAM is the 24 MB user partition, not kernel RAM; 2 MB loaded (the GE's
# display list alone is 512 KB) keeps it lean beside its textures and the firmware's dialogs.
MAX_PRX_SIZE = 524288
MAX_MEM      = 2097152

check: $(EXTRA_TARGETS)
	python3 ../tools/check_prx.py --elf $(TARGET).elf --prx $(TARGET).prx \
		--nid-table ../tools/nids/ark4_660.tsv --allowlist ../tools/nids/allowlist.tsv \
		--max-prx-size $(MAX_PRX_SIZE) --max-mem $(MAX_MEM)

.PHONY: check

-include $(wildcard build/*.d)

clean: clean-local
clean-local:
	-rm -rf build

.PHONY: clean-local
