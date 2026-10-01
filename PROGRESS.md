# Progress

## Roadmap
1. [x] Build system, Unity tests, battery pack sim model
2. [x] HAL: voltage/current/temperature sensing and contactor/balance control
3. [x] Firmware: protection (over/under voltage, over current, over temp) with fault state
4. [x] Firmware: SOC estimation (coulomb counting + OCV correction)
5. [x] Firmware: passive cell balancing
6. [x] Harness: scenario runner wiring sim + HAL + firmware, CSV output
7. [x] WebAssembly build (Emscripten)
8. [x] Web dashboard (HTML/JS + Chart.js)

## Milestone 1: Build system and sim model
- Makefile: `make` builds `build/bms`, `make test` runs all `tests/test_*.c`,
  `make run` prints CSV. Flags: C11, `-Wall -Wextra -Werror`.
- Unity v2.6.0 vendored in `tests/unity/` (built with `UNITY_INCLUDE_DOUBLE`).
- `sim/cell.c`: NMC cell model. OCV from SOC lookup table (3.00 V to 4.20 V,
  linear interpolation), coulomb counted SOC, I*R voltage sag, I^2*R heating
  with cooling toward ambient. Positive current = discharge.
- `sim/pack.c`: 4S series pack sharing one current; pack voltage = sum of cells.
- `harness/main.c`: 1C discharge for one hour, CSV every 60 s.
- Tests: 11 cell tests, 5 pack tests, all passing.

## Milestone 2: HAL
- `hal/hal.h`: firmware facing interface, fixed point units only. Cell voltage
  (`uint16_t` mV), pack current (`int32_t` mA, positive = discharge), cell
  temperature (`int16_t` 0.1 C), contactor set/get, per cell balance set/get.
  Includes nothing from `sim/`.
- `hal/hal_sim.c` + `hal_sim.h`: sim backend. `hal_sim_attach(&pack)` binds it;
  readings are rounded and clamped; bad cell index or no attached pack reads 0
  and ignores writes. Only harness/tests include `hal_sim.h`.
- Sim additions: main contactor (open = no load current, closed by default) and
  a 33 ohm bleed resistor per cell switched by `sim_pack_set_balance`.
  `sim_pack_current()` returns the current actually flowing.
- Tests: 11 cell, 8 pack, 9 HAL, all passing.

## Milestone 3: Protection
- `firmware/protection.c` + `.h`: includes only `hal.h`. Faults are bit flags:
  OV (> 4250 mV), UV (< 3000 mV), OC discharge (> 10 A), OC charge (> 3 A),
  OT (> 60.0 C). Limits are configurable via `prot_limits_t`.
- `protection_check()` is pure threshold logic on a `prot_sample_t`;
  `protection_step()` reads the HAL, debounces (3 consecutive steps by
  default, any clean step resets the counter), latches faults and opens the
  contactor. `protection_clear()` only clears and recloses the contactor when
  no condition is still present.
- Harness runs `protection_step()` every second and adds `contactor,faults`
  CSV columns; the 1C discharge now ends with a UV trip.
- Tests: 11 cell, 8 pack, 9 HAL, 17 protection, all passing.

## Milestone 4: SOC estimation
- `firmware/soc.c` + `.h`: includes only `hal.h`. SOC in 0.01 % units
  (`uint16_t`, 0..10000); remaining charge held as `int64_t` mA*ms.
- `soc_init()` seeds the estimate from the OCV of the lowest cell (series pack
  is limited by its weakest cell). `soc_update()` is pure: coulomb counts the
  current, clamps to 0..capacity, and once |I| <= `rest_ma` (50 mA) has held
  for `rest_ms` (5 min) it snaps SOC to the OCV lookup. Any load resets the
  rest timer. `soc_step()` reads the HAL. Config via `soc_config_t`
  (default 3000 mAh).
- Firmware has its own mV OCV table (same curve as the sim, no sim include).
- Harness runs 4200 s (discharge, UV trip, rest) and adds `soc_est_pct`.
- Tests: 11 cell, 8 pack, 9 HAL, 17 protection, 17 SOC, all passing.
- Dashboard (after milestones 7/8 landed): `api_soc_est_pct()` runs
  `soc_step()` each sim second; the dashboard shows an "SOC estimate
  (firmware)" tile and plots the estimate as a dashed line against the true
  cell SOCs. 3 more API tests (seeding, tracking a discharge, OCV correction
  after 5 min rest). Verified in headless Chromium.

## Milestone 5: Passive cell balancing
- `firmware/balance.c` + `.h`: includes only `hal.h`. Bleeds every cell that
  is more than `start_mv` (15 mV) above the lowest cell, and keeps bleeding
  until it is within `stop_mv` (5 mV) (hysteresis, state in `bal_t.mask`).
- Inhibited (all bleeders off) while discharging above 500 mA, when the
  lowest cell is below 3400 mV, when any cell is above 50.0 C, or when the
  caller passes `allowed = false`. Charging and rest are allowed.
- `balance_select()` is pure; `balance_step()` reads the HAL and drives
  `hal_set_balance()`. Config via `bal_config_t`.
- Harness: balancing runs every second, allowed only with no latched
  protection fault; CSV gains `balance_mask`. The 1C discharge never
  balances (load is above the inhibit limit).
- Dashboard: "Passive balancing enabled" toggle, "Imbalanced pack" scenario
  (cells 80/72/66/75 %, load 0 A), cell spread tile with bleeding cells,
  Balancing column in the cell table. Charts now keep the last 600 samples
  (one minute of wall time at any speed) instead of a fixed sim hour.
- Verified in headless Chromium: imbalance starts bleeding cells 1, 2, 4;
  spread drops from 107 mV to 8 mV in about 3.3 sim hours; toggle disables.
- Tests: 17 balance (selection, hysteresis, each inhibit, HAL driving, sim
  convergence at rest, no balancing under load) and 3 more API tests
  (16 total). All suites pass.

## Milestone 6: Scenario runner
- `harness/system.c` + `.h`: `bms_sys_t` bundles the pack and all firmware
  (protection, SOC, balancing). `sys_step()` = firmware then 1 s of sim.
  Used by both the CSV runner and the WASM API, so they cannot drift apart.
- `harness/scenario.c` + `.h`: scenarios are data (start SOC, duration, time
  ordered events: load, ambient, cell SOC, clear faults, balancing on/off).
  Six built in: `discharge_1c` (UV at ~1:00:00), `charge` (2.9 A, OV at
  ~0:50:00), `overcurrent` (12 A pulse trips, scripted clear recloses),
  `over_temp` (65 C ambient, OT at ~0:25:00), `imbalance` (80/72/66/75 %
  at rest, balanced in ~3.3 h), `weak_cell` (one cell at 30 %, UV at 0:18:00).
- `harness/csv.c`: header, rows and `csv_run()`; final state is always
  written. `harness/main.c` is now a CLI:
  `build/bms [scenario] [--every SECONDS]`, `build/bms --list`.
  `make run SCENARIO=charge`, `make scenarios` writes `build/csv/*.csv`.
  Default output (`discharge_1c`, every 60 s) keeps the old columns.
- Makefile: tests and the native binary link every harness module except
  `main.c`; headers are now prerequisites so header edits trigger rebuilds.
- Dashboard: scenario picker (free run + the six) running the same C table
  through the API; progress note, sliders and balancing toggle follow
  scenario events, run auto-pauses at the end; Reset returns to free run.
  Each control now pushes only its own value to the sim.
- Verified in headless Chromium: charge ends in OV and auto-pauses with the
  load label at -2.9 A, overcurrent recovers, weak_cell ends in UV, reset
  returns to free run; balancing and protection checks still pass.
- Tests: 17 scenario (system, table sanity, event timing, every scenario's
  outcome, CSV row/column counts) and 3 more API tests (19 total). 115 tests
  across 8 suites, all passing.

## Milestones 7 and 8: WebAssembly build and web dashboard
(First built ahead of 5 and 6; SOC, balancing and scenarios were wired into
the API and dashboard by the later milestones, see their sections.)
- `harness/api.c` + `.h`: flat C API over one `bms_sys_t` (reset, set
  load/ambient/cell SOC, step N seconds, clear faults, balancing toggle,
  scenarios, getters).
- `make wasm` builds `web/bms.js` + `web/bms.wasm` with emcc (modularized as
  `createBms()`, same `-Wall -Wextra -Werror` flags). Uses `emcc` from PATH,
  else `~/emsdk/upstream/emscripten/emcc` (override with `EMSDK=` or
  `EMCC=`), so no `emsdk_env.sh` is needed. Build outputs are gitignored.
- `make test-wasm` (tests/test_wasm.cjs, Node): API smoke tests plus every
  scenario run in WASM must end in exactly the native CSV runner's final
  state (time, faults, pack voltage, SOC estimate). 10 checks, all passing.
  Kept out of `make test` so the Unity suites need no Emscripten.
- `make serve` builds then serves `web/` on http://localhost:8000.
- `web/index.html`: load and ambient sliders, run/pause, speed (10x to 600x),
  reset, clear faults, "weak cell 3" UV scenario; status tiles for pack
  voltage, current, contactor and latched faults; Chart.js line charts of
  cell voltage, SOC, temperature and pack current; per cell table. Light and
  dark themes.
- Verified in headless Chromium: page loads with no console errors, sim
  runs, 15 A trips OC discharge and opens the contactor, clear recloses it.
- Tests: 10 API tests at first; 19 now (Unity). All pass.
- Final check (milestone 7 pass): fresh clone of origin/main, clean env with
  no emsdk sourcing: 115 unit tests, 10 WASM tests and 16 headless Chromium
  dashboard checks (both themes, protection, balancing, scenarios) all pass.
