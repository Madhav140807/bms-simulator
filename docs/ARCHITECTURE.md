# Architecture and design decisions

This document explains why the project is shaped the way it is. For what each
part does, see the [README](../README.md).

## Layers

```
 web/index.html (JS)           harness/main.c (CLI)        tests/
        |                             |                       |
 harness/api.c (flat C API)           |                       |
        \_____________________  ______/_______________________/
                              \/
                   harness/system.c  (sim + firmware, 1 s step)
                     /                         \
          firmware/*.c                       sim/*.c
               |                                ^
          hal/hal.h  <---- implemented by ---- hal/hal_sim.c
```

Each layer only depends on the ones below it. `firmware/` and `sim/` never
include each other.

## Why the HAL boundary exists

The point of the project is BMS firmware, not a simulator. Real BMS firmware
reads an ADC, a current shunt and thermistors, and drives a contactor and
balance switches. It never has access to the "true" state of the cells. The
HAL keeps the simulated firmware honest about that:

1. **The firmware only knows what a real BMS could measure.** If firmware
   could include `sim/pack.h`, it would be easy, even by accident, to read the
   true SOC or the true temperature and make an estimator look perfect. With
   the boundary, the Kalman filter has to earn its accuracy from noisy
   millivolt readings, exactly as it would on a board.
2. **Fixed point units at the boundary.** The sim uses `double` volts, amps
   and degrees. The HAL converts to `uint16_t` mV, `int32_t` mA and `int16_t`
   tenths of a degree, rounded and clamped like an ADC. Firmware written
   against these types would run on a microcontroller without an FPU change
   in its interface.
3. **Portability.** Moving to hardware means writing one new file, a HAL
   backend for the real peripherals (or for Renode emulated ones). Nothing in
   `firmware/` has to change. The rule is that firmware includes `hal.h`,
   its own headers and the C standard library only. The build uses one
   include path list, so this is a project rule kept by review rather than
   by the compiler.
4. **Fault injection lives on the right side.** A broken sense wire is
   modelled in `hal_sim.c` (the reading is 0 mV), and an internal short or
   heater is modelled in the sim. The firmware cannot tell an injected fault
   from a real one, so the tests check the real detection logic.
5. **Testability.** Firmware logic is split into pure functions that take a
   sample (`protection_check`, `soc_update`, `balance_select`, the CAN
   encoders) and thin `*_step` functions that read the HAL. The pure parts are
   tested with hand made numbers. The step functions are tested against the
   sim through the HAL.

`hal_sim.h` is the one header that knows both sides. Only the harness and
tests include it.

## One system, three front ends

`harness/system.c` owns one pack and all the firmware state, and
`sys_step()` runs the firmware then advances the sim by one second. The CLI,
the WASM API and the unit tests all use it. This removed a class of bugs
where the dashboard and the CSV runner wired things up slightly differently.
Scenarios are data (a table of timed events) in `harness/scenario.c`, so the
same nine scenarios run in the CLI, the tests and the dashboard picker.

## Determinism

Every run is reproducible:

- Cell mismatch and sensor noise come from a seeded xorshift32 generator
  (`sim/rng.c`). The gaussian is a sum of 12 uniforms so it needs no libm,
  which could differ between the native and WASM builds.
- Call order matters. An early bug read voltage and current as two arguments
  in one call; C leaves argument evaluation order unspecified, so gcc and
  clang drew the noise in different orders and WASM diverged from native.
  Each HAL read is now its own statement.
- `make test-wasm` runs every scenario in both builds and requires the same
  final state and byte for byte identical CAN frames.

## Firmware choices

- **Integer first.** Protection, coulomb counting, balancing and CAN use
  fixed width integers only. Charge is held as `int64_t` mA·ms so it does
  not lose precision over long runs.
- **Kalman filter in single precision floats**, one scalar filter per cell.
  A scalar state keeps the maths small (no matrices) and per cell filters
  find the weakest cell directly. Square roots use a few Newton steps, so the
  firmware does not need libm.
- **Latched faults.** A trip latches and opens the contactor. Clearing is
  refused while the condition is still present, which is how real packs
  avoid cycling a contactor into a fault.
- **Plausibility before thresholds.** A cell reading of 0 mV is far more
  likely to be a broken wire than an empty cell, so it raises a sensor
  fault. A hot reading is never dismissed as a sensor fault, because missing
  a real thermal event is worse than a false trip.
- **CAN carries what the BMS measured**, taken from protection's last sample,
  not from the sim. The dashboard test checks the decoded frames against the
  BMS readings.

## Dashboard

The dashboard is one static HTML file with no build step. It loads
`bms.js` and `bms.wasm` (built by Emscripten from the same C sources) and
Chart.js from a CDN. Every 100 ms it steps the sim by N seconds (the speed
setting) and redraws. All the physics and firmware stay in C; the JavaScript
only reads getters, drives controls and decodes CAN frames for display.

## Testing strategy

- **Unit tests (Unity)** cover each module, plus scenario level checks that
  each scripted run ends in the expected fault at the expected time.
- **WASM test (Node)** proves the browser runs the same code with the same
  results as the native build.
- **Browser test (Playwright)** drives the real dashboard: it fails on any
  page error, and checks protection, balancing, estimators, fault injection,
  the CAN log, tooltips and layout at tablet and phone widths. It can target
  the deployed site with `BMS_URL`.
- CI runs all three; Pages deploys only the commit CI tested.
