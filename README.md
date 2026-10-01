# bms-simulator
Simulated 4S lithium ion pack plus BMS firmware in C, compiled to WebAssembly
with a live browser dashboard.

```sh
make test                          # run all unit tests
make run                           # native harness, CSV to stdout
source ~/emsdk/emsdk_env.sh        # Emscripten toolchain
make serve                         # build WASM, serve web/ on :8000
```
