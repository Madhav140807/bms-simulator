# bms-simulator
Simulated 4S lithium ion pack plus BMS firmware in C, compiled to WebAssembly
with a live browser dashboard.

```sh
make test                          # run all unit tests
make run                           # native harness, CSV to stdout
make run SCENARIO=charge           # any scenario from ./build/bms --list
make scenarios                     # every scenario's CSV into build/csv/
source ~/emsdk/emsdk_env.sh        # Emscripten toolchain
make serve                         # build WASM, serve web/ on :8000
```
