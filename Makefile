CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -Isim -Ihal -Ifirmware
LDLIBS  := -lm
BUILD   := build

SIM_SRC := $(wildcard sim/*.c)
HAL_SRC := $(wildcard hal/*.c)
FW_SRC  := $(wildcard firmware/*.c)
LIB_SRC := $(SIM_SRC) $(HAL_SRC) $(FW_SRC)

UNITY_DIR := tests/unity
TEST_SRC  := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))

.PHONY: all test run clean

all: $(BUILD)/bms

$(BUILD):
	mkdir -p $@

$(BUILD)/bms: harness/main.c $(LIB_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/test_%: tests/test_%.c $(LIB_SRC) $(UNITY_DIR)/unity.c | $(BUILD)
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -I$(UNITY_DIR) $^ -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; ./$$t; done

run: $(BUILD)/bms
	./$(BUILD)/bms

clean:
	rm -rf $(BUILD)
