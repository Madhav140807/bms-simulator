// End to end dashboard test (run with `make test-browser`).
// Serves web/ on a free local port, loads it in headless Chromium in light and
// dark mode, fails on any page error, and drives the protection, balancing and
// scenario features through the UI. Screenshots go to build/screenshots/.
// Needs the WASM build and Playwright: `npm install && npx playwright install chromium`.
// Set BMS_URL to test a deployed site instead (e.g. the GitHub Pages URL).
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
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm",
                ".svg": "image/svg+xml", ".png": "image/png" };
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
  check(/V$/.test(t["Pack voltage"]) && /%$/.test(t["SOC (coulomb count)"]), `${scheme}_tiles_render`);
  check(await p.evaluate(() => Chart.getChart("cVolt").data.datasets[0].data.length > 1), `${scheme}_charts_have_data`);
  await p.screenshot({ path: path.join(shotDir, `${scheme}.png`), fullPage: true });
}

// Page metadata, intro, footer, favicon, social preview image and tile tooltips.
async function checkPageInfo(p, url) {
  const meta = (sel) => p.getAttribute(sel, "content").catch(() => null);
  check(((await meta('meta[name="description"]')) || "").length > 50, "meta_description");
  const og = await meta('meta[property="og:image"]');
  check(/og\.png$/.test(og || "") && await meta('meta[name="twitter:card"]') === "summary_large_image", "og_twitter_tags");
  const fetchOk = async (rel) => (await p.request.get(new URL(rel, url).href)).ok();
  check(await fetchOk("og.png") && await fetchOk(await p.getAttribute('link[rel="icon"]', "href")), "og_image_and_favicon_served");
  check((await text(p, "#intro")).length > 100, "intro_present");
  check(/github\.com\/Madhav140807\/bms-simulator/.test(await p.getAttribute("#repoLink", "href")), "footer_repo_link");
  const tips = await p.$$eval(".tile", (ts) => ts.every((t) => t.querySelector(".info") && t.querySelector(".tip").textContent.length > 20));
  check(tips, "every_tile_has_tooltip");
  const tip = p.locator("#tipContactor");
  const hidden = !(await tip.isVisible());
  await p.hover('[aria-describedby="tipContactor"]');
  const shown = await tip.isVisible();
  await p.mouse.move(0, 0);
  check(hidden && shown && !(await tip.isVisible()), "tooltip_shows_on_hover");
  await p.click('[aria-describedby="tipProtection"]');
  const pinned = await p.locator("#tipProtection").isVisible();
  await p.click("h1");
  check(pinned && !(await p.locator("#tipProtection").isVisible()), "tooltip_click_toggles");
  const box = await tip.boundingBox().catch(() => null);
  check(box === null || box.x >= 0, "tooltip_stays_on_screen");
}

// No horizontal page scroll at tablet and phone widths, in both themes.
async function checkNarrowLayout(browser, url) {
  for (const [name, width] of [["tablet", 820], ["phone", 390]]) {
    for (const scheme of ["light", "dark"]) {
      const p = await browser.newPage({ viewport: { width, height: 900 }, colorScheme: scheme });
      await p.goto(url, { waitUntil: "networkidle" });
      await p.waitForSelector('body[data-ready="1"]', { timeout: 10000 });
      await p.hover('[aria-describedby="tipProtection"]');
      const fits = await p.evaluate(() => {
        const vw = document.documentElement.clientWidth;
        const tip = document.querySelector("#tipProtection").getBoundingClientRect();
        return document.documentElement.scrollWidth <= vw && tip.left >= 0 && tip.right <= vw;
      });
      check(fits, `${name}_${scheme}_no_horizontal_overflow`);
      await p.screenshot({ path: path.join(shotDir, `${name}-${scheme}.png`), fullPage: true });
      await p.close();
    }
  }
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

// Kalman estimate error and coulomb counting error vs the true lowest cell SOC.
function estimatorErrors(p) {
  return p.evaluate(() => {
    const truth = Math.min(...[0, 1, 2, 3].map((i) => bms._api_cell_soc(i))) * 100;
    return { ekf: Math.abs(bms._api_ekf_pct() - truth), coulomb: Math.abs(bms._api_soc_est_pct() - truth) };
  });
}

async function checkKalman(p) {
  check(await p.isVisible("#cKalman"), "kalman_chart_visible");
  const chart = await p.evaluate(() => {
    const c = Chart.getChart("cKalman");
    return { sets: c.data.datasets.length, points: c.data.datasets[4].data.length,
             legend: c.legend.legendItems.map((l) => l.text) };
  });
  check(chart.sets === 5 && chart.points > 1, "kalman_chart_has_data");
  check(chart.legend.join("|") === "True SOC (lowest cell)|Coulomb counting|Kalman filter", "kalman_legend_hides_band");
  check(/%$/.test(await text(p, "#tEkf")) && /1σ/.test(await text(p, "#tEkfSigma")), "kalman_tile_shows_estimate");
  check((await estimatorErrors(p)).ekf < 3, "kalman_tracks_truth");
  // From a full pack, forcing the estimates to 50 % is a ~50 % error.
  await p.click("#reset");
  await p.selectOption("#speed", "1");
  await waitText(p, "#tTime", /^0:00:(0[5-9]|[1-5]\d)/);
  await p.click("#corrupt");
  const recovered = await p.waitForFunction(() => {
    const truth = Math.min(...[0, 1, 2, 3].map((i) => bms._api_cell_soc(i))) * 100;
    return Math.abs(bms._api_ekf_pct() - truth) < 2;
  }, null, { timeout: WAIT_MS }).then(() => true, () => false);
  const err = await estimatorErrors(p);
  check(recovered && err.ekf < 2, "kalman_recovers_from_corruption");
  check(err.coulomb > 20, "coulomb_count_stays_wrong");
  await p.screenshot({ path: path.join(shotDir, "kalman.png"), fullPage: true });
  await p.selectOption("#speed", "60");
}

// Table row values for one cell (0 based).
function cellRow(p, i) {
  return p.$eval(`#cells tr:nth-child(${i + 1})`, (r) => ({
    meas: parseFloat(r.cells[2].textContent), soc: parseFloat(r.cells[3].textContent),
    temp: parseFloat(r.cells[4].textContent), injected: r.cells[8].textContent,
  }));
}

async function injectOn(p, cell, button) {
  await p.click("#reset");
  await p.selectOption("#speed", "60");
  await setRange(p, "load", 1);
  await p.selectOption("#injCell", String(cell));
  await p.click(button);
}

async function checkFaultInjection(p) {
  await injectOn(p, 1, "#injHeat");
  check(/Cell 2: heater/.test(await text(p, "#injNote")), "overheat_button_injects");
  check(await waitText(p, "#tFaults", /Over temperature/), "overheat_trips_ot");
  const hot = await cellRow(p, 1), cool = await cellRow(p, 0);
  check(hot.temp > 60 && hot.temp > cool.temp + 20 && hot.injected === "Heater", "overheat_only_that_cell");
  await p.screenshot({ path: path.join(shotDir, "inject-overheat.png"), fullPage: true });

  await injectOn(p, 2, "#injShort");
  check(await waitText(p, "#tFaults", /Over temperature/), "short_trips_ot");
  await p.waitForTimeout(1500);
  const shorted = await cellRow(p, 2), healthy = await cellRow(p, 0);
  check(shorted.soc < healthy.soc - 5 && shorted.injected === "Internal short", "short_drains_that_cell");
  check(/Open/.test(await text(p, "#tCont")), "short_opens_contactor");

  await injectOn(p, 3, "#injSensor");
  check(await waitText(p, "#tFaults", /Sensor fault/), "sensor_failure_trips_sensor_fault");
  check((await cellRow(p, 3)).meas === 0, "sensor_failure_reads_zero");
  check(!/Under voltage/.test(await text(p, "#tFaults")), "sensor_failure_not_reported_as_uv");
  await p.screenshot({ path: path.join(shotDir, "inject-sensor.png"), fullPage: true });
  await p.click("#clear");
  check(/Fault/.test(await text(p, "#tProt")), "clear_blocked_while_sensor_failed");
  await p.click("#injClear");
  check(/No faults injected/.test(await text(p, "#injNote")), "remove_injected_faults");
  await p.waitForTimeout(300);
  await p.click("#clear");
  check(await waitText(p, "#tProt", /OK/), "clear_works_after_removal");
  await p.click("#reset");
}

function canRows(p) {
  return p.$$eval("#canLog .can-row", (rows) => rows.map((r) => ({
    id: Number(r.dataset.id), fault: r.classList.contains("fault"), text: r.textContent,
  })));
}

async function checkCanLog(p) {
  await p.click("#reset");
  await p.selectOption("#speed", "1");
  await setRange(p, "load", 3);
  check(await p.isVisible("#canLog"), "can_panel_visible");
  await waitText(p, "#tTime", /^0:00:(0[5-9]|[1-5]\d)/);
  const rows = await canRows(p);
  const ids = new Set(rows.map((r) => r.id));
  check([0x100, 0x101, 0x102, 0x103].every((id) => ids.has(id)), "can_periodic_ids_present");
  const count1 = parseInt(await text(p, "#canStats"), 10);
  await p.waitForTimeout(800);
  check(parseInt(await text(p, "#canStats"), 10) > count1, "can_frame_count_grows");

  // Freeze so the log and the table show the same sim step, then compare.
  await p.click("#run");
  await p.waitForTimeout(300);
  const snap = await canRows(p);
  const packV = parseFloat(snap.find((r) => r.id === 0x100).text.match(/pack ([\d.]+) V/)[1]);
  check(Math.abs(packV - parseFloat((await tiles(p))["Pack voltage"])) < 0.05, "can_pack_voltage_matches_tile");
  const canMv = snap.find((r) => r.id === 0x101).text.match(/cells ([\d ]+) mV/)[1].trim().split(" ").map(Number);
  const tableMv = (await cellRows(p)).map((r) => r.measMv);
  check(JSON.stringify(canMv) === JSON.stringify(tableMv), "can_cell_voltages_match_bms_readings");
  await p.click("#run");

  await setRange(p, "load", 15);
  check(await waitText(p, "#canLog", /FAULT latched: Over current \(discharge\)/), "can_fault_event_on_trip");
  check((await canRows(p)).some((r) => r.id === 0x080 && r.fault), "can_fault_event_highlighted");
  await setRange(p, "load", 3);
  await p.click("#clear");

  await p.selectOption("#canFilter", "258");
  await p.waitForTimeout(300);
  const filtered = await canRows(p);
  check(filtered.length > 0 && filtered.every((r) => r.id === 0x102), "can_filter_by_id");
  await p.selectOption("#canFilter", "all");
  await p.check("#canFreeze");
  const frozen = await text(p, "#canLog");
  await p.waitForTimeout(600);
  check((await text(p, "#canLog")) === frozen, "can_freeze_holds_log");
  await p.uncheck("#canFreeze");
  await p.screenshot({ path: path.join(shotDir, "can-log.png"), fullPage: true });
  await p.selectOption("#speed", "60");
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
  check(names.length === 10, "picker_lists_nine_scenarios");
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
  const remote = process.env.BMS_URL;
  if (!remote && !fs.existsSync(path.join(webDir, "bms.wasm"))) {
    console.error("web/bms.wasm missing. Run `make wasm` first.");
    process.exit(2);
  }
  fs.mkdirSync(shotDir, { recursive: true });
  const server = remote ? null : await serve();
  const url = remote || `http://127.0.0.1:${server.address().port}/`;
  console.log(`testing ${url}`);
  const browser = await chromium.launch();
  try {
    for (const scheme of ["light", "dark"]) {
      const errors = [];
      const p = await openPage(browser, url, scheme, errors);
      await checkLoads(p, scheme);
      if (scheme === "light") {
        await checkPageInfo(p, url);
        await checkMismatchAndNoise(p);
        await checkKalman(p);
        await checkFaultInjection(p);
        await checkCanLog(p);
        await checkProtection(p);
        await checkBalancing(p);
        await checkScenarios(p);
      }
      check(errors.length === 0, `${scheme}_no_page_errors`);
      errors.forEach((e) => console.log("  page error: " + e));
      await p.close();
    }
    await checkNarrowLayout(browser, url);
  } finally {
    await browser.close();
    if (server) server.close();
  }
  console.log(`\n${count} Tests ${failures} Failures`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
