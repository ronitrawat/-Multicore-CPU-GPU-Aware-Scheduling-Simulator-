import { chromium } from "playwright";
import { mkdir, readdir, rm } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const webRoot = path.resolve(here, "..");
const outputDir = path.join(webRoot, "demo");
const baseUrl = process.env.DASHBOARD_URL ?? "http://127.0.0.1:4174";

await mkdir(outputDir, { recursive: true });

const browser = await chromium.launch({
  headless: true,
  channel: process.env.PLAYWRIGHT_BROWSER_CHANNEL || "msedge"
});
const context = await browser.newContext({
  viewport: { width: 1440, height: 1050 },
  deviceScaleFactor: 1,
  recordVideo: { dir: outputDir, size: { width: 1440, height: 1050 } }
});
const page = await context.newPage();
const recording = page.video();

try {
  const health = await fetch(`${baseUrl}/api/health`);
  if (!health.ok) throw new Error(`Dashboard API is not available at ${baseUrl}. Start it with npm start first.`);

  await page.goto(baseUrl, { waitUntil: "networkidle" });
  await page.getByLabel("Workload source").selectOption("generated");
  await page.getByRole("button", { name: /Run simulation/ }).click();
  await page.getByText("RESULTS READY").waitFor({ timeout: 60000 });
  await page.mouse.move(1410, 20);
  await page.waitForTimeout(500);

  await page.screenshot({
    path: path.join(outputDir, "dashboard-overview.png"),
    fullPage: true,
    animations: "disabled"
  });

  await page.locator(".lower-grid").screenshot({
    path: path.join(outputDir, "policy-comparison.png"),
    animations: "disabled"
  });
  await page.locator(".timeline-panel").screenshot({
    path: path.join(outputDir, "execution-timeline.png"),
    animations: "disabled"
  });
  await page.locator(".process-panel").screenshot({
    path: path.join(outputDir, "process-metrics.png"),
    animations: "disabled"
  });

  await page.waitForTimeout(1500);
  console.log(`Screenshots and video saved to ${outputDir}`);
} finally {
  await context.close();
  if (recording) {
    const recordedPath = await recording.path();
    const videoPath = path.join(outputDir, "scheduler-dashboard.webm");
    await recording.saveAs(videoPath);
    if (path.resolve(recordedPath) !== path.resolve(videoPath)) {
      await rm(recordedPath, { force: true });
    }
    const staleRecordings = (await readdir(outputDir)).filter((name) => /^page@.*\.webm$/.test(name));
    await Promise.all(staleRecordings.map((name) => rm(path.join(outputDir, name), { force: true })));
  }
  await browser.close();
}
