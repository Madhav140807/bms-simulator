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
9. [x] Sim realism: cell manufacturing mismatch and sensor noise
10. [x] Firmware: Kalman filter SOC estimation, dashboard chart
11. [x] Fault injection (overheat, internal short, sensor failure) + sensor plausibility
12. [x] Firmware: CAN messages, dashboard CAN log panel

Items 9 to 12 were requested later (the request called them milestones 4 to
6, which here were already SOC, balancing and the scenario runner).

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
- `make test-browser` (tests/test_browser.cjs, Playwright): serves `web/` on
  a free local port, loads the dashboard in headless Chromium (light and
  dark) and drives protection, balancing and scenarios through the UI;
  fails on any page error. 26 checks, all passing (~36 s). Screenshots in
  `build/screenshots/`. Needs `npm install && npx playwright install
  chromium` once. Verified it fails (exit 1) on an injected JS error and a
  wrong fault label.

## CI and GitHub Pages
- `.github/workflows/ci.yml` (every push and PR): job `unit` runs
  `make test` and `make scenarios` (CSV uploaded as an artifact); job
  `wasm-browser` installs Emscripten 6.0.10 (cached) and Playwright, then
  runs `make test-wasm` and `make test-browser` (screenshots uploaded).
- `.github/workflows/deploy.yml`: runs when CI completes on main and only if
  CI succeeded (or manually); checks out the exact commit CI tested, builds the
  WASM and deploys `web/` to GitHub Pages with actions/deploy-pages.
- Both lint clean with actionlint 1.7.12.

## Milestone 9: Cell mismatch and sensor noise
- `sim/rng.c`: seeded xorshift32; gaussian as sum of 12 uniforms (no libm,
  so native and WASM draw identical numbers).
- `sim_pack_apply_mismatch()`: per cell capacity (sigma 1.5 %), resistance
  (8 %) and start SOC (0.5 points), clamped to 3 sigma. `sim_pack_init()`
  still makes identical cells.
- `hal_sim_set_noise()`: gaussian noise on every HAL read (2 mV, 10 mA,
  0.1 C). Off by default at the HAL level; 10 mA keeps SOC rest detection
  (|I| <= 50 mA for 5 min) working.
- `sys_init()` applies both with fixed seeds, so runs stay reproducible.
  `protection` keeps its last sample (`prot_t.last`), which the dashboard
  shows as "BMS reads" without drawing extra noise samples.
- Bugs found by the noise and fixed:
  - `soc_step()` read current and voltage as two function arguments; C
    leaves the order unspecified, so GCC and clang drew noise differently
    and WASM diverged from native. Now separate statements.
  - Balancing chattered (635 bleeder switches in `imbalance`) and stopped
    ~9.5 mV short. Balancing now filters cell voltages (EMA 1/8, fixed point)
    and adds back the bleed sag of bleeding cells (`bleed_sag_mv` = 2):
    4 switches, final spread 7.6 mV with noise, 5.9 mV without.
- Dashboard: "Sensor noise" toggle; table shows true voltage, what the BMS
  reads, capacity and resistance per cell.
- Tests: 5 RNG, 3 pack mismatch, 4 HAL noise, 1 protection, 5 balancing
  (filter, chatter, sag), 2 system; SOC tests now compare against the lowest
  true cell. 135 unit tests, 11 WASM, 30 browser checks (new: capacities
  differ, voltages differ, readings scatter with noise and match truth
  without). All passing.

## Milestone 10: Kalman filter SOC
- `firmware/ocv.c`: the OCV table now shared by both estimators (`soc.c`
  uses it unchanged), plus `ocv_mv_at()` and `ocv_slope_mv()` for the filter.
- `firmware/ekf.c`: one extended Kalman filter per cell, state = SOC.
  Predict by coulomb counting (q = 1e-8 per s); correct with the measured
  cell voltage against OCV(SOC) - I * R0 (R0 = 20 mOhm, r = 25 mV^2).
  Pack SOC = lowest cell. Single precision floats, Newton square root so
  the firmware needs no libm. Seeds each cell from its OCV.
- Runs every second next to the coulomb counter; CSV gains `ekf_soc_pct`.
  WASM and native give identical results (checked by `make test-wasm`).
- Accuracy with noise and mismatch: within 3 % of every cell through a
  full 1C discharge (typically under 1 %). After both estimators are forced
  to 50 % under load, the filter is back within 2 % in about 30 s; coulomb
  counting stays wrong until a 5 min rest.
- Dashboard: full width chart "Kalman filter vs coulomb counting" (true
  lowest cell SOC, coulomb count, Kalman estimate with a +-2 sigma band),
  "SOC (Kalman)" tile with sigma, "Corrupt SOC estimates" button.
- Tests: 12 EKF/OCV, 1 system (tracks lowest cell through discharge),
  2 API (recovers from corruption, per cell tracking). 150 unit tests,
  11 WASM, 37 browser checks (new: chart visible with data, legend, tile,
  tracks truth, recovers from corruption while coulomb counting stays
  wrong). All passing.

## Milestone 11: Fault injection and sensor plausibility
- Sim: per cell external heater (`sim_pack_set_heater`) and soft internal
  short (`sim_pack_set_short`): the short drains the cell through R and
  dissipates V^2/R inside it, and keeps going with the contactor open.
  HAL sim: `hal_sim_set_sense_open()` makes a cell read 0 mV.
- System: `sys_inject()` with SYS_INJ_HEATER (3 W), SYS_INJ_SHORT (2 ohm,
  ~1.9 A, ~7 W), SYS_INJ_SENSOR; `sys_init()` clears them. Scenario event
  EV_INJECT and three scenarios: `cell_overheat` (OT at ~14 min),
  `internal_short` (OT at ~5 min, cell keeps draining), `sensor_failure`
  (SENSOR fault, not UV).
- Protection: new PROT_FAULT_SENSOR (0x20). Cell readings outside
  1000..5000 mV raise SENSOR instead of OV/UV; temperatures below -40.0 C
  (open thermistor) raise SENSOR. Hot readings are never dismissed as
  sensor faults: a real 128 C cell (seen in `internal_short`) must trip OT.
  SENSOR latches and cannot be cleared while the reading is implausible.
- SOC estimators skip implausible readings (`cell_mv_plausible()` in
  `ocv.h`): coulomb counting's rest correction ignores the cell, the Kalman
  filter predicts without correcting.
- Dashboard: Fault injection row (cell picker, Overheat cell, Short cell,
  Sensor failure, Remove injected faults), status note, Injected column,
  "Sensor fault" label; scenario picker lists the 3 new scenarios.
- Tests: 4 pack, 1 HAL, 6 protection, 1 SOC, 1 EKF, 4 scenario/system,
  1 API. 168 unit tests, 14 WASM (9 scenarios match native), 49 browser
  checks (new: each injection button trips the right fault on the right
  cell, sensor failure reads 0 and is not UV, clear is blocked until the
  injection is removed). All passing.

## Milestone 12: CAN messages and CAN log panel
- HAL: `hal_can_send()` with `hal_can_frame_t` (11-bit ID, DLC, 8 bytes).
  Sim backend stores frames with a timestamp in a 1024 frame ring
  (`hal_sim_can_total/get/clear/set_time_ms`); rejects DLC > 8 and IDs
  above 0x7FF.
- `firmware/can_tx.c`: little endian frames, layout documented in
  `can_tx.h`: 0x100 pack (V, I, Kalman SOC, contactor, faults), 0x101 cell
  mV, 0x102 cell 0.1 C, 0x103 status (coulomb SOC, balance mask, faults,
  alive counter) every second; 0x080 FAULT_EVENT once when new fault bits
  latch (again after a clear). Values come from protection's last sample,
  so the bus carries exactly what the BMS measured.
- `build/bms <scenario> --can` prints every frame in candump log format,
  e.g. `(000062.000000) can0 080#0404`.
- Sim timing bug found and fixed: a cell's I*R drop only updated on the
  next sim step, so right after a load change the BMS saw the new current
  with the old voltage. The Kalman filter jumped ~8 % for a few seconds.
  Switch and load changes now update cell currents immediately. This also
  exposed that scenarios seeded the SOC estimators under load; they now
  seed at rest, as at power up.
- Dashboard: CAN bus log panel (newest first: time, ID, raw bytes, decoded
  meaning; fault events highlighted with an icon), ID filter, freeze.
- `tests/test_browser.cjs` can target a deployed site with `BMS_URL=...`.
- Tests: 14 CAN (encoders byte for byte, fault event rules, alive wrap,
  bus ring and rejects, 4 frames per sim second, trip on the bus, candump
  format), 1 pack and 1 scenario timing regression. 184 unit tests,
  15 WASM (new: CAN frames identical to native), 58 browser checks (new:
  panel visible and filling, all periodic IDs, decoded pack voltage
  matches the tile, decoded cell mV equal the BMS readings, fault event on
  a trip and highlighted, filter, freeze). All passing.

## Dashboard visual pass
- `web/index.html`: restyled only (no element IDs, classes read by JS/tests,
  or script logic changed). Header got a brand mark and a live-sim badge;
  cards got real elevation (shadow + radius tokens) instead of a flat
  border; buttons/inputs got hover, active and focus-visible states;
  status chips and the CAN fault rows now use a tinted background instead
  of just a border; table rows and CAN log rows highlight on hover; the
  page fades in using the `data-ready` flag the script already sets.
  Categorical chart colors unchanged and re-validated (light and dark both
  pass the colorblind-safety/contrast checks).
- Checked by rendering the page in headless Chromium (light, dark, and a
  420px mobile width) against a stubbed WASM API, since Emscripten isn't
  available in this environment to run the real `make test-browser`
  suite. No selectors `tests/test_browser.cjs` depends on were touched.

## Final polish
- CLAUDE.md restored (the committed copy was cut off): full no attribution
  rule and the 8 original milestones.
- Re-verified the dashboard visual pass against the real WASM build (it had
  only been checked against a stubbed API). Found two problems: the page
  stayed invisible (opacity 0) when WASM or Chart.js failed to load, hiding
  the error message, and `.tile { overflow: hidden }` would clip the new
  tooltips. Both fixed; unit, WASM and browser suites pass.
- Dashboard: favicon, meta description, Open Graph and Twitter tags with
  `web/og.png` (1200x630), short intro, an info tooltip on every status tile
  (hover, focus or tap; flips left near the right edge), footer with the
  repo link. Layout checked at 1440, 820 and 390 px in light and dark:
  tiles are now 4 columns on desktop/tablet and 2 on phones (8 in one row
  had wrapped labels and "Closed" overflowing its tile), range sliders no
  longer overflow by 4 px, Actions spans two columns on wide screens, cell
  names no longer wrap on phones. Overheat cell is a fault injection button.
- `make screenshots` (scripts/screenshots.cjs) regenerates `web/og.png` and
  `docs/dashboard.png` from a real run.
- README rewritten (demo link, badges, hero image, features, Mermaid
  diagram, how each part works, CAN table, testing, structure, limitations,
  future work). `docs/ARCHITECTURE.md` covers the design decisions,
  especially the HAL boundary.
- MIT LICENSE; removed `firmware/.gitkeep`. Header comment on every C
  source; no dead code found. Zero warnings with gcc 13, clang 18 and emcc
  under `-std=c11 -Wall -Wextra -Werror`.
- Tests: 184 unit, 15 WASM, 71 browser checks (new: meta and social tags,
  og.png and favicon served, intro, footer link, a tooltip on every tile,
  hover and click behaviour, no horizontal overflow at tablet and phone
  widths in both themes). Verified the overflow check fails when the
  tooltip flip is broken. All passing.
