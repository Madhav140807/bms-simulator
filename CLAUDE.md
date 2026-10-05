# BMS Simulator

A simulated lithium ion battery pack plus battery management system (BMS) firmware,
written in C, compiled to WebAssembly, and shown in a live browser dashboard.

## Architecture
- sim/       Battery pack model (the "fake hardware"). C.
- hal/       Hardware abstraction layer. The ONLY interface between firmware and sim.
- firmware/  BMS logic. Must never include anything from sim/ directly.
- harness/   Native terminal program wiring sim + firmware together, outputs CSV.
- tests/     Unit tests (use the Unity C test framework).
- web/       Static dashboard (plain HTML/JS + Chart.js) that loads the WASM build.

## Rules
- C11, compile with -Wall -Wextra -Werror. Use a Makefile.
- Every milestone must include tests that pass before committing.
- Keep functions small. Use fixed size types (uint16_t etc.) in firmware/.
- After finishing a milestone: run all tests, update PROGRESS.md, commit with a short
  clear message, and push.
- NEVER include "Co-Authored-By", "Generated with Claude Code", or any Claude
  attribution in commit messages, PR descriptions, code comments or docs.

## Milestones
1. Build system, Unity tests, battery pack sim model (cell OCV curve, internal
   resistance, coulomb counted SOC, simple thermal model; 4S pack).
2. HAL: voltage, current and temperature sensing plus contactor and balance
   control, with a sim backend.
3. Firmware: protection (over/under voltage, over current, over temperature)
   with a latched fault state that opens the contactor.
4. Firmware: SOC estimation (coulomb counting + OCV correction at rest).
5. Firmware: passive cell balancing.
6. Harness: scenario runner wiring sim + HAL + firmware, CSV output.
7. WebAssembly build (Emscripten).
8. Web dashboard (HTML/JS + Chart.js).

Later additions (cell mismatch and sensor noise, Kalman filter SOC, fault
injection, CAN messages) are tracked in PROGRESS.md.
