# Host-side unit tests for the portable parts of the plugin.
#
#   make            build and run all tests (ASan + UBSan)
#   make run T=log  run only tests whose name contains "log"
#   make clean

CC      ?= gcc
SRC_DIR  = ../plugin/src
LIB_DIR  = ../plugin/lib
HOME_DIR = ../home/src
BUILD    = build

CFLAGS  = -std=gnu11 -g -O1 -Wall -Wextra -Werror -I$(SRC_DIR) -I$(LIB_DIR) -I$(HOME_DIR) -I. \
          -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
LDFLAGS = -fsanitize=address,undefined
# Rebuild objects when a header they include changes (a stale object once hid a new enum value).
CFLAGS += -MMD -MP
# Every string, the XMB plugin's and Home's too: the tests check each key in every translation.
CFLAGS += -DPW_I18N_HOME

# Portable plugin modules under test (must not include PSP headers).
PLUGIN_SRCS = $(SRC_DIR)/core/ring.c $(SRC_DIR)/core/log.c $(SRC_DIR)/core/crc32.c $(SRC_DIR)/core/stubinfo.c \
              $(SRC_DIR)/core/png.c $(SRC_DIR)/core/config.c $(SRC_DIR)/complications/states/sfo.c $(SRC_DIR)/complications/states/stream.c \
              $(SRC_DIR)/core/lz.c $(SRC_DIR)/ui/draw.c $(SRC_DIR)/ui/font8x8.c $(SRC_DIR)/ui/input.c \
              $(SRC_DIR)/ui/shapes.c $(SRC_DIR)/ui/icons.c $(SRC_DIR)/ui/views.c \
              $(SRC_DIR)/gfx/pgf.c $(SRC_DIR)/gfx/text.c $(SRC_DIR)/gfx/rlz.c $(SRC_DIR)/gfx/rco.c \
              $(SRC_DIR)/gfx/image.c $(SRC_DIR)/ui/xmbbg.c $(SRC_DIR)/complications/states/store.c $(SRC_DIR)/ui/i18n.c \
              $(SRC_DIR)/complications/registry.c $(SRC_DIR)/ui/backdrop.c \
              $(SRC_DIR)/ui/views_modern.c $(SRC_DIR)/ui/views_minimal.c $(SRC_DIR)/ui/volume.c $(SRC_DIR)/ui/langnames.c $(SRC_DIR)/core/battmodel.c $(SRC_DIR)/core/playtime.c $(SRC_DIR)/core/bootpick.c \
              $(HOME_DIR)/disc.c $(HOME_DIR)/home_view.c $(HOME_DIR)/gfx_cpu.c $(HOME_DIR)/sprites.c $(HOME_DIR)/usb_link.c $(HOME_DIR)/music_name.c $(HOME_DIR)/sound_synth.c \
              $(LIB_DIR)/stb_sprintf.c
TEST_SRCS   = main.c platform_host.c $(wildcard test_*.c)

OBJS = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(TEST_SRCS) $(PLUGIN_SRCS)))
vpath %.c . $(SRC_DIR)/core $(SRC_DIR)/complications $(SRC_DIR)/complications/states $(SRC_DIR)/ui $(SRC_DIR)/gfx $(LIB_DIR) $(HOME_DIR)

all: run

$(BUILD)/pwtest: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/stb_sprintf.o: CFLAGS += -Wno-sign-compare
$(BUILD)/lz.o: CFLAGS += -Wno-unused-function

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)/tmp

# RLZ streams from our own data (tests/fixtures/make_rlz.py), generated, not committed.
FIXTURES = $(BUILD)/fixtures/rlz_text.rlz
$(FIXTURES): fixtures/make_rlz.py | $(BUILD)
	mkdir -p $(BUILD)/fixtures
	python3 fixtures/make_rlz.py $(BUILD)/fixtures > /dev/null

run: $(BUILD)/pwtest $(FIXTURES)
	PWTEST_TMPDIR=$(BUILD)/tmp ./$(BUILD)/pwtest $(T)

# Host renders of every menu screen (tests/preview/preview_ui.c); FONT=path/to/a.pgf is optional,
# UI_LANG=fr renders a translation from plugin/lang/.
PREVIEW_OBJS = $(BUILD)/preview_ui.o $(filter-out $(BUILD)/main.o $(BUILD)/test_%.o $(BUILD)/store.o,$(OBJS))
$(BUILD)/preview_ui.o: preview/preview_ui.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

preview: $(PREVIEW_OBJS)
	$(CC) $(LDFLAGS) -o $(BUILD)/preview_ui $^
	mkdir -p $(BUILD)/preview
	ASAN_OPTIONS=detect_leaks=0 PW_PREVIEW_FONT=$(FONT) PW_PREVIEW_RCO=$(RCO) PW_PREVIEW_SKY=$(SKY) PW_PREVIEW_MONTH=$(MONTH) PW_PREVIEW_LANG=$(UI_LANG) ./$(BUILD)/preview_ui

# Frame cost by part (tests/bench/bench_ui.c), -O2 without sanitizers: only the proportions matter.
BENCH_OPT ?= -O2
BENCH_SRCS = bench/bench_ui.c $(filter-out %/store.c %/log.c %/ring.c,$(PLUGIN_SRCS))
bench: | $(BUILD)
	$(CC) -std=gnu11 $(BENCH_OPT) -I$(SRC_DIR) -I$(LIB_DIR) -I$(HOME_DIR) -I. -DPW_I18N_HOME -Wno-sign-compare -o $(BUILD)/bench_ui $(BENCH_SRCS) platform_host.c
	PW_BENCH_FONT=$(FONT) ./$(BUILD)/bench_ui

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d) $(BUILD)/preview_ui.d

.PHONY: all run clean preview bench
