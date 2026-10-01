// Smoke test for the WebAssembly build (run with `make test-wasm`).
// Loads web/bms.js in Node, exercises the API, and checks that every
// scenario ends in the same state as the native build/bms CSV runner.
"use strict";
const { execFileSync } = require("child_process");
const path = require("path");
const root = path.join(__dirname, "..");
const createBms = require(path.join(root, "web", "bms.js"));

let failures = 0, count = 0;
function check(ok, name) {
  count++;
  if (!ok) failures++;
  console.log(`${name}:${ok ? "PASS" : "FAIL"}`);
}

function nativeFinalRow(name) {
  const csv = execFileSync(path.join(root, "build", "bms"), [name, "--every", "1000000"], { encoding: "utf8" });
  const lines = csv.trim().split("\n");
  const head = lines[0].split(",");
  const row = lines[lines.length - 1].split(",");
  return Object.fromEntries(head.map((h, i) => [h, row[i]]));
}

createBms().then((m) => {
  m._api_reset(0.5);
  check(m._api_num_cells() === 4, "four_cells");
  const minSoc = Math.min(...[0, 1, 2, 3].map((i) => m._api_cell_soc(i))) * 100;
  check(Math.abs(m._api_soc_est_pct() - minSoc) < 1.5, "soc_estimate_seeded");
  const caps = new Set([0, 1, 2, 3].map((i) => m._api_cell_capacity_ah(i)));
  check(caps.size === 4, "cells_are_mismatched");
  m._api_set_load(3);
  m._api_step(60);
  check(m._api_time_s() === 60 && m._api_cell_soc(0) < 0.5, "discharge_steps");
  m._api_set_load(15);
  m._api_step(5);
  check((m._api_faults() & 0x04) && m._api_contactor() === 0, "overcurrent_trips");

  for (let i = 0; i < m._api_scenario_count(); i++) {
    const name = m.UTF8ToString(m._api_scenario_name(i));
    m._api_start_scenario(i);
    m._api_step(1e6);
    const want = nativeFinalRow(name);
    const sameFaults = m._api_faults() === parseInt(want.faults, 16);
    const sameTime = m._api_time_s() === Number(want.time_s);
    const sameV = Math.abs(m._api_pack_v() - Number(want.pack_v)) < 1e-3;
    const sameSoc = Math.abs(m._api_soc_est_pct() - Number(want.soc_est_pct)) < 0.01;
    check(m._api_scenario_done() && sameFaults && sameTime && sameV && sameSoc,
          `scenario_${name}_matches_native`);
  }
  console.log(`\n${count} Tests ${failures} Failures`);
  process.exit(failures ? 1 : 0);
});
