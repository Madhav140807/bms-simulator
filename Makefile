CC      ?= gcc
WARN    := -std=c11 -Wall -Wextra -Werror
CFLAGS  := $(WARN) -O2 -Isim -Ihal -Ifirmware -Iharness
LDLIBS  := -lm
BUILD   := build

SIM_SRC := $(wildcard sim/*.c)
HAL_SRC := $(wildcard hal/*.c)
FW_SRC  := $(wildcard firmware/*.c)
LIB_SRC := $(SIM_SRC) $(HAL_SRC) $(FW_SRC)

UNITY_DIR := tests/unity
TEST_SRC  := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))

EMCC     ?= emcc
WEB      := web
API_SRC  := harness/api.c
API_FNS  := reset set_load set_ambient set_cell_soc step clear_faults time_s \
            load_a current_a pack_v cell_v cell_soc cell_temp_c contactor \
            faults num_cells soc_est_pct
comma    := ,
empty    :=
space    := $(empty) $(empty)
EXPORTS  := $(subst $(space),$(comma),$(patsubst %,'_api_%',$(API_FNS)))
EMFLAGS  := -sMODULARIZE=1 -sEXPORT_NAME=createBms -sENVIRONMENT=web,node \
            -sEXPORTED_FUNCTIONS=[$(EXPORTS)] \
            -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap]

.PHONY: all test run clean wasm serve

all: $(BUILD)/bms

$(BUILD):
	mkdir -p $@

$(BUILD)/bms: harness/main.c $(LIB_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/test_%: tests/test_%.c $(LIB_SRC) $(UNITY_DIR)/unity.c | $(BUILD)
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -I$(UNITY_DIR) $^ -o $@ $(LDLIBS)

# The API test also needs the harness API source.
$(BUILD)/test_api: tests/test_api.c $(API_SRC) $(LIB_SRC) $(UNITY_DIR)/unity.c | $(BUILD)
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -I$(UNITY_DIR) $^ -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; ./$$t; done

run: $(BUILD)/bms
	./$(BUILD)/bms

wasm: $(WEB)/bms.js

$(WEB)/bms.js: $(API_SRC) $(LIB_SRC)
	$(EMCC) $(CFLAGS) $^ -o $@ $(EMFLAGS)

serve: wasm
	cd $(WEB) && python3 -m http.server 8000

clean:
	rm -rf $(BUILD) $(WEB)/bms.js $(WEB)/bms.wasm
