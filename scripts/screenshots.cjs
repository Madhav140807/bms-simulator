// Regenerates the dashboard images (run with `make screenshots`):
//   web/og.png          1200x630 social preview (status tiles and charts)
//   docs/dashboard.png  README hero image
// Runs a 1C discharge with an overheated cell so the charts have shape.
// Needs the WASM build and Playwright: `npm install && npx playwright install chromium`.
"use strict";
const fs = require("fs");
const http = require("http");
const path = require("path");
const { chromium } = require("playwright");

const root = path.join(__dirname, "..");
const webDir = path.join(root, "web");
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm", ".svg": "image/svg+xml" };

function serve() {
  const server = http.createServer((req, res) => {
    const rel = decodeURIComponent(req.url.split("?")[0]).replace(/^\/+/, "") || "index.html";
    const file = path.join(webDir, path.normalize(rel));
    if (!file.startsWith(webDir) || !fs.existsSync(file)) return res.writeHead(404).end();
    res.writeHead(200, { "Content-Type": TYPES[path.extname(file)] || "application/octet-stream" });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", () => resolve(server)));
}

// Free run at 3 A, cell 2 heated, 20 sim minutes at 600x.
async function stage(p) {
  await p.waitForSelector('body[data-ready="1"]');
  await p.selectOption("#speed", "60");
  await p.selectOption("#injCell", "1");
  await p.click("#injHeat");
  await p.waitForFunction(() => bms._api_time_s() >= 1200, null, { timeout: 60000 });
  await p.click("#run");   // pause so the image is stable
}

async function main() {
  const server = await serve();
  const url = `http://127.0.0.1:${server.address().port}/`;
  const browser = await chromium.launch();
  try {
    const og = await browser.newPage({ viewport: { width: 1200, height: 1400 }, colorScheme: "light" });
    await og.goto(url);
    await stage(og);
    const top = await og.evaluate(() => document.querySelector(".tiles").getBoundingClientRect().top + scrollY - 16);
    await og.screenshot({ path: path.join(webDir, "og.png"), clip: { x: 0, y: top, width: 1200, height: 630 } });

    const hero = await browser.newPage({ viewport: { width: 1280, height: 1420 }, colorScheme: "dark" });
    await hero.goto(url);
    await stage(hero);
    fs.mkdirSync(path.join(root, "docs"), { recursive: true });
    await hero.screenshot({ path: path.join(root, "docs", "dashboard.png") });
  } finally {
    await browser.close();
    server.close();
  }
  console.log("wrote web/og.png and docs/dashboard.png");
}

main().catch((e) => { console.error(e); process.exit(1); });
