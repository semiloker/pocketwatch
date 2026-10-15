# Host-side unit tests for the portable parts of the plugin.
#
#   make            build and run all tests (ASan + UBSan)
#   make run T=log  run only tests whose name contains "log"
#   make clean

CC      ?= gcc
SRC_DIR  = ../plugin/src
LIB_DIR  = ../plugin/lib
BUILD    = build

CFLAGS  = -std=gnu11 -g -O1 -Wall -Wextra -Werror -I$(SRC_DIR) -I$(LIB_DIR) -I. \
          -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
LDFLAGS = -fsanitize=address,undefined

# Portable plugin modules under test (must not include PSP headers).
PLUGIN_SRCS = $(SRC_DIR)/core/ring.c $(SRC_DIR)/core/log.c $(SRC_DIR)/core/crc32.c $(SRC_DIR)/core/stubinfo.c \
              $(SRC_DIR)/core/png.c $(SRC_DIR)/complications/states/sfo.c $(SRC_DIR)/complications/states/stream.c \
              $(SRC_DIR)/core/lz.c $(LIB_DIR)/stb_sprintf.c
TEST_SRCS   = main.c platform_host.c $(wildcard test_*.c)

OBJS = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(TEST_SRCS) $(PLUGIN_SRCS)))
vpath %.c . $(SRC_DIR)/core $(SRC_DIR)/complications/states $(LIB_DIR)

all: run

$(BUILD)/pwtest: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/stb_sprintf.o: CFLAGS += -Wno-sign-compare
$(BUILD)/lz.o: CFLAGS += -Wno-unused-function

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)/tmp

run: $(BUILD)/pwtest
	PWTEST_TMPDIR=$(BUILD)/tmp ./$(BUILD)/pwtest $(T)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
