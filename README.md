# bms-simulator
Simulated 4S lithium ion pack plus BMS firmware in C, compiled to WebAssembly
with a live browser dashboard.

Emscripten is expected on PATH or in `~/emsdk`:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
```

```sh
make test                          # run all unit tests
make run                           # native harness, CSV to stdout
make run SCENARIO=charge           # any scenario from ./build/bms --list
make scenarios                     # every scenario's CSV into build/csv/
make test-wasm                     # WASM build vs native, needs Emscripten + Node
make serve                         # build WASM, serve web/ on :8000
```
