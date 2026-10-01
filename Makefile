CC      ?= gcc
WARN    := -std=c11 -Wall -Wextra -Werror
CFLAGS  := $(WARN) -O2 -Isim -Ihal -Ifirmware -Iharness
LDLIBS  := -lm
BUILD   := build

SIM_SRC := $(wildcard sim/*.c)
HAL_SRC := $(wildcard hal/*.c)
FW_SRC  := $(wildcard firmware/*.c)
LIB_SRC := $(SIM_SRC) $(HAL_SRC) $(FW_SRC)
# Harness library: everything in harness/ except the CLI entry point.
SYS_SRC := $(filter-out harness/main.c,$(wildcard harness/*.c))
HDRS    := $(wildcard sim/*.h hal/*.h firmware/*.h harness/*.h)
SRCS     = $(filter %.c,$^)

UNITY_DIR := tests/unity
TEST_SRC  := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))

# Emscripten: emcc from PATH, else the emsdk checkout in ~/emsdk.
EMSDK    ?= $(HOME)/emsdk
EMCC     ?= $(or $(shell command -v emcc 2>/dev/null),$(EMSDK)/upstream/emscripten/emcc)
WEB      := web
WASM_SRC := harness/api.c harness/system.c harness/scenario.c $(LIB_SRC)
API_FNS  := reset set_load set_ambient set_cell_soc step clear_faults \
            set_balancing balancing balance_mask time_s load_a ambient_c \
            current_a pack_v cell_v cell_soc cell_temp_c contactor faults \
            num_cells soc_est_pct scenario_count scenario_name scenario_desc \
            start_scenario scenario_active scenario_done scenario_duration_s
comma    := ,
empty    :=
space    := $(empty) $(empty)
EXPORTS  := $(subst $(space),$(comma),$(patsubst %,'_api_%',$(API_FNS)))
EMFLAGS  := -sMODULARIZE=1 -sEXPORT_NAME=createBms -sENVIRONMENT=web,node \
            -sEXPORTED_FUNCTIONS=[$(EXPORTS)] \
            -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,UTF8ToString]

.PHONY: all test test-wasm test-browser run scenarios clean wasm serve

SCENARIO ?= discharge_1c
CSV_DIR  := $(BUILD)/csv

all: $(BUILD)/bms

$(BUILD):
	mkdir -p $@

$(BUILD)/bms: harness/main.c $(SYS_SRC) $(LIB_SRC) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDLIBS)

$(BUILD)/test_%: tests/test_%.c $(SYS_SRC) $(LIB_SRC) $(UNITY_DIR)/unity.c $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -I$(UNITY_DIR) $(SRCS) -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; ./$$t; done

run: $(BUILD)/bms
	./$(BUILD)/bms $(SCENARIO)

# Writes one CSV per scenario into build/csv/.
scenarios: $(BUILD)/bms
	mkdir -p $(CSV_DIR)
	@set -e; for s in $$(./$(BUILD)/bms --list | cut -d' ' -f1); do \
		./$(BUILD)/bms $$s > $(CSV_DIR)/$$s.csv; echo "$(CSV_DIR)/$$s.csv"; done

wasm: $(WEB)/bms.js

$(WEB)/bms.js: $(WASM_SRC) $(HDRS)
	$(EMCC) $(CFLAGS) $(SRCS) -o $@ $(EMFLAGS)

# Checks the WASM build from Node against the native CSV runner.
test-wasm: wasm $(BUILD)/bms
	node tests/test_wasm.cjs

# Drives the dashboard in headless Chromium (needs `npm install` and
# `npx playwright install chromium` once).
test-browser: wasm
	node tests/test_browser.cjs

serve: wasm
	cd $(WEB) && python3 -m http.server 8000

clean:
	rm -rf $(BUILD) $(WEB)/bms.js $(WEB)/bms.wasm
