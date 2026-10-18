# Pocketwatch Home - a home screen of our own (homebrew application, docs/subsystems/home.md)
#
#   make        build EBOOT.PBP (install as ms0:/PSP/GAME/PocketwatchHome/EBOOT.PBP)
#   make clean

TARGET = pocketwatch_home

# Our drawing, fonts, texts and models are the plugin's own portable code, built again here
# (into build/, so the plugin's objects next to the sources stay apart).
OBJS = build/main.o build/hw.o build/library.o build/launch.o build/texts.o build/home_view.o \
       build/disc.o build/hm_image.o build/gfx_gu.o build/sprites.o build/stubs.o \
       build/draw.o build/shapes.o build/icons.o build/font8x8.o build/input.o build/i18n.o \
       build/text.o build/pgf.o build/sfo.o build/playtime.o build/crc32.o build/stb_sprintf.o

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

INCDIR  = src lib ../plugin/src ../plugin/lib
CFLAGS  = -O2 -G0 -Wall -Wextra -Werror -DPW_I18N_HOME -DPW_VERSION=\"$(VERSION)\"
ASFLAGS = $(CFLAGS)
LIBS    = -lpspgu -lpsppower -lpsprtc -lpsputility

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

-include $(wildcard build/*.d)

clean: clean-local
clean-local:
	-rm -rf build

.PHONY: clean-local
