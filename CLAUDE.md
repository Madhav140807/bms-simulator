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
  attribution
