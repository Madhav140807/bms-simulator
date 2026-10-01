# bms-simulator
Simulated 4S lithium ion pack plus BMS firmware in C, compiled to WebAssembly
with a live browser dashboard.

Live dashboard: https://madhav140807.github.io/bms-simulator/ (deployed by
`.github/workflows/deploy.yml` after CI passes on main).

Emscripten is expected on PATH or in `~/emsdk`:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
```

The browser test needs Playwright once: `npm install && npx playwright install chromium`.

```sh
make test                          # run all unit tests
make run                           # native harness, CSV to stdout
make run SCENARIO=charge           # any scenario from ./build/bms --list
make scenarios                     # every scenario's CSV into build/csv/
./build/bms overcurrent --can      # CAN frames in candump log format
make test-wasm                     # WASM build vs native, needs Emscripten + Node
make test-browser                  # dashboard end to end in headless Chromium
BMS_URL=https://... node tests/test_browser.cjs   # same checks against a deployed site
make serve                         # build WASM, serve web/ on :8000
```
