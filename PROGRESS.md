# Progress

## Roadmap
1. [x] Build system, Unity tests, battery pack sim model
2. [ ] HAL: voltage/current/temperature sensing and contactor/balance control
3. [ ] Firmware: protection (over/under voltage, over current, over temp) with fault state
4. [ ] Firmware: SOC estimation (coulomb counting + OCV correction)
5. [ ] Firmware: passive cell balancing
6. [ ] Harness: scenario runner wiring sim + HAL + firmware, CSV output
7. [ ] WebAssembly build (Emscripten)
8. [ ] Web dashboard (HTML/JS + Chart.js)

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
