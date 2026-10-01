# Progress

## Roadmap
1. [x] Build system, Unity tests, battery pack sim model
2. [x] HAL: voltage/current/temperature sensing and contactor/balance control
3. [x] Firmware: protection (over/under voltage, over current, over temp) with fault state
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
