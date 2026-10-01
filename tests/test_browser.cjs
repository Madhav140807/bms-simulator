// End to end dashboard test (run with `make test-browser`).
// Serves web/ on a free local port, loads it in headless Chromium in light and
// dark mode, fails on any page error, and drives the protection, balancing and
// scenario features through the UI. Screenshots go to build/screenshots/.
// Needs the WASM build and Playwright: `npm install && npx playwright install chromium`.
"use strict";
const fs = require("fs");
const http = require("http");
const path = require("path");

let chromium;
try {
  ({ chromium } = require("playwright"));
} catch {
  console.error("Playwright not found. Run: npm install && npx playwright install chromium");
  process.exit(2);
}

const root = path.join(__dirname, "..");
const webDir = path.join(root, "web");
const shotDir = path.join(root, "build", "screenshots");
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm" };
const WAIT_MS = 60000;

let failures = 0, count = 0;
function check(ok, name) {
  count++;
  if (!ok) failures++;
  console.log(`${name}:${ok ? "PASS" : "FAIL"}`);
}

function serve() {
  const server = http.createServer((req, res) => {
    const rel = decodeURIComponent(req.url.split("?")[0]).replace(/^\/+/, "") || "index.html";
    const file = path.join(webDir, path.normalize(rel));
    if (!file.startsWith(webDir) || !fs.existsSync(file)) {
      res.writeHead(404).end();
      return;
    }
    res.writeHead(200, { "Content-Type": TYPES[path.extname(file)] || "application/octet-stream" });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", () => resolve(server)));
}

// ---- page helpers ----

const text = (p, sel) => p.textContent(sel);

function tiles(p) {
  return p.evaluate(() => Object.fromEntries([...document.querySelectorAll(".tile")].map((t) => {
    const chips = t.querySelector(".chips");
    return [t.querySelector(".k").textContent, t.querySelector(".v").textContent + (chips ? chips.textContent : "")];
  })));
}

async function setRange(p, id, value) {
  await p.fill(`#${id}`, String(value));
  await p.dispatchEvent(`#${id}`, "input");
}

function waitText(p, sel, re) {
  return p.waitForFunction(([s, src]) => new RegExp(src).test(document.querySelector(s).textContent),
    [sel, re.source], { timeout: WAIT_MS }).then(() => true, () => false);
}

async function openPage(browser, url, scheme, errors) {
  const p = await browser.newPage({ viewport: { width: 1280, height: 1600 }, colorScheme: scheme });
  p.on("console", (m) => { if (m.type() === "error") errors.push(m.text()); });
  p.on("pageerror", (e) => errors.push(String(e)));
  await p.goto(url, { waitUntil: "networkidle" });
  await p.waitForSelector('body[data-ready="1"]', { timeout: 10000 });
  await p.selectOption("#speed", "60");
  return p;
}

// ---- checks ----

async function checkLoads(p, scheme) {
  check(await waitText(p, "#tTime", /^0:(0[1-9]|[1-5]\d):/), `${scheme}_sim_runs`);
  const t = await tiles(p);
  check(/V$/.test(t["Pack voltage"]) && /%$/.test(t["SOC estimate (firmware)"]), `${scheme}_tiles_render`);
  check(await p.evaluate(() => Chart.getChart("cVolt").data.datasets[0].data.length > 1), `${scheme}_charts_have_data`);
  await p.screenshot({ path: path.join(shotDir, `${scheme}.png`), fullPage: true });
}

// Per cell rows of the table: true voltage (mV), BMS reading (mV), capacity (Ah).
function cellRows(p) {
  return p.$$eval("#cells tr", (rows) => rows.map((r) => ({
    trueMv: parseFloat(r.cells[1].textContent) * 1000,
    measMv: parseFloat(r.cells[2].textContent),
    capAh: parseFloat(r.cells[5].textContent),
  })));
}

// Largest |BMS reading - true voltage| seen over several refreshes.
async function maxReadingError(p, samples) {
  let max = 0;
  for (let i = 0; i < samples; i++) {
    await p.waitForTimeout(150);
    for (const r of await cellRows(p)) max = Math.max(max, Math.abs(r.measMv - r.trueMv));
  }
  return max;
}

async function checkMismatchAndNoise(p) {
  const rows = await cellRows(p);
  check(new Set(rows.map((r) => r.capAh)).size === 4, "cell_capacities_differ");
  check(new Set(rows.map((r) => r.trueMv.toFixed(1))).size > 1, "cell_voltages_differ");
  check(await maxReadingError(p, 10) >= 2, "noise_on_readings_scatter");
  await p.uncheck("#noise");
  check(await maxReadingError(p, 10) <= 1.5, "noise_off_readings_match_truth");
  await p.check("#noise");
}

async function checkProtection(p) {
  await setRange(p, "load", 15);
  check(await waitText(p, "#tFaults", /Over current/), "overcurrent_trips");
  check(/Open/.test(await text(p, "#tCont")), "overcurrent_opens_contactor");
  await setRange(p, "load", 3);
  await p.click("#clear");
  check(await waitText(p, "#tProt", /OK/), "clear_faults_recovers");
}

async function checkBalancing(p) {
  await p.click("#imbalance");
  check(await waitText(p, "#tBal", /Bleeding cell/), "imbalance_starts_bleeding");
  check((await text(p, "#cells")).includes("Bleeding"), "table_shows_bleeding");
  check(await waitText(p, "#tBal", /idle/), "balancing_finishes");
  check(parseInt((await tiles(p))["Cell spread"], 10) < 10, "spread_below_10_mv");
  await p.screenshot({ path: path.join(shotDir, "balancing.png"), fullPage: true });
  await p.click("#imbalance");
  await p.uncheck("#balancing");
  check(await waitText(p, "#tBal", /disabled/), "toggle_disables_balancing");
  await p.check("#balancing");
}

async function runScenario(p, name) {
  await p.selectOption("#scenario", { label: name });
  await p.click("#start");
  return waitText(p, "#scnNote", /Finished/);
}

async function checkScenarios(p) {
  const names = await p.$$eval("#scenario option", (os) => os.map((o) => o.textContent));
  check(names.length === 7, "picker_lists_six_scenarios");
  check(await runScenario(p, "charge"), "charge_finishes");
  check(/Over voltage/.test(await text(p, "#tFaults")), "charge_ends_in_ov");
  check((await text(p, "#run")) === "Run", "scenario_auto_pauses");
  check((await text(p, "#loadVal")) === "-2.9 A", "load_label_follows_scenario");
  await p.screenshot({ path: path.join(shotDir, "scenario-charge.png"), fullPage: true });
  check(await runScenario(p, "overcurrent"), "overcurrent_finishes");
  const t = await tiles(p);
  check(/OK/.test(t["Protection"]) && /Closed/.test(t["Contactor"]), "overcurrent_scenario_recovers");
  check(await runScenario(p, "weak_cell"), "weak_cell_finishes");
  check(/Under voltage/.test(await text(p, "#tFaults")), "weak_cell_ends_in_uv");
  await p.click("#reset");
  check((await p.$eval("#scenario", (s) => s.value)) === "-1" && (await text(p, "#run")) === "Pause",
        "reset_returns_to_free_run");
}

async function main() {
  if (!fs.existsSync(path.join(webDir, "bms.wasm"))) {
    console.error("web/bms.wasm missing. Run `make wasm` first.");
    process.exit(2);
  }
  fs.mkdirSync(shotDir, { recursive: true });
  const server = await serve();
  const url = `http://127.0.0.1:${server.address().port}/`;
  const browser = await chromium.launch();
  try {
    for (const scheme of ["light", "dark"]) {
      const errors = [];
      const p = await openPage(browser, url, scheme, errors);
      await checkLoads(p, scheme);
      if (scheme === "light") {
        await checkMismatchAndNoise(p);
        await checkProtection(p);
        await checkBalancing(p);
        await checkScenarios(p);
      }
      check(errors.length === 0, `${scheme}_no_page_errors`);
      errors.forEach((e) => console.log("  page error: " + e));
      await p.close();
    }
  } finally {
    await browser.close();
    server.close();
  }
  console.log(`\n${count} Tests ${failures} Failures`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
