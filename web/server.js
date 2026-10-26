import { execFile } from "node:child_process";
import { createServer } from "node:http";
import { mkdtemp, readFile, readdir, rm, stat } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";

const execFileAsync = promisify(execFile);
const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "..");
const staticRoot = path.join(here, "dist");
const allowedAlgorithms = new Set(["fcfs", "sjf", "srtf", "rr", "priority", "priority-np", "mlfq"]);
const allowedWorkloads = new Set(["example.csv", "srtf_demo.csv", "textbook_sjf.csv"]);
const allowedDistributions = new Set(["uniform", "exponential", "bimodal"]);
const allowedBalancers = new Set(["global", "percore", "speedaware"]);
const mimeTypes = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".ico": "image/x-icon",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png"
};

export function validateConfig(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Request body must be a JSON object.");
  }

  const config = {
    algorithm: value.algorithm ?? "rr",
    workload: value.workload ?? "example.csv",
    distribution: value.distribution ?? "uniform",
    processCount: Number(value.processCount ?? 24),
    seed: Number(value.seed ?? 42),
    cores: Number(value.cores ?? 4),
    speeds: value.speeds ?? "1,1,1.5,2",
    quantum: Number(value.quantum ?? 4),
    loadBalancer: value.loadBalancer ?? "speedaware"
  };

  if (!allowedAlgorithms.has(config.algorithm)) throw new Error("Choose a supported scheduling algorithm.");
  if (typeof config.workload !== "string" ||
      (config.workload !== "generated" && !/^[\w.-]+\.csv$/i.test(config.workload))) {
    throw new Error("Choose a valid workload.");
  }
  if (config.workload !== "generated" && !allowedWorkloads.has(config.workload)) {
    throw new Error("That workload is not available.");
  }
  if (!Number.isInteger(config.processCount) || config.processCount < 1 || config.processCount > 150) {
    throw new Error("Generated workloads must contain between 1 and 150 processes.");
  }
  if (!Number.isInteger(config.seed) || config.seed < 0 || config.seed > 2147483647) {
    throw new Error("Seed must be an integer between 0 and 2147483647.");
  }
  if (!Number.isInteger(config.cores) || config.cores < 1 || config.cores > 16) {
    throw new Error("Choose between 1 and 16 CPU cores.");
  }
  if (!Number.isInteger(config.quantum) || config.quantum < 1 || config.quantum > 1000) {
    throw new Error("Time quantum must be between 1 and 1000 ticks.");
  }
  if (!allowedBalancers.has(config.loadBalancer)) throw new Error("Choose a supported load balancer.");
  if (!allowedDistributions.has(config.distribution)) throw new Error("Choose a supported workload distribution.");

  if (typeof config.speeds !== "string") throw new Error("Core speeds must be comma-separated numbers.");
  const speeds = config.speeds.split(",").map((part) => Number(part.trim()));
  if (speeds.length !== config.cores || speeds.some((speed) => !Number.isFinite(speed) || speed <= 0 || speed > 16)) {
    throw new Error(`Enter exactly ${config.cores} core speeds, each greater than 0 and at most 16.`);
  }
  config.speeds = speeds.join(",");
  return config;
}

export function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length < 2) return [];
  const headers = splitCsvLine(lines[0]).map((header) => header.trim());
  return lines.slice(1).filter(Boolean).map((line) => {
    const values = splitCsvLine(line);
    return Object.fromEntries(headers.map((header, index) => [header, values[index] ?? ""]));
  });
}

function splitCsvLine(line) {
  const values = [];
  let value = "";
  let quoted = false;
  for (let index = 0; index < line.length; index += 1) {
    const character = line[index];
    if (character === '"' && line[index + 1] === '"' && quoted) {
      value += '"';
      index += 1;
    } else if (character === '"') {
      quoted = !quoted;
    } else if (character === "," && !quoted) {
      values.push(value);
      value = "";
    } else {
      value += character;
    }
  }
  values.push(value);
  return values;
}

async function getSimulatorPath() {
  const candidates = process.env.SCHEDULER_BINARY
    ? [path.resolve(process.env.SCHEDULER_BINARY)]
    : [
        path.join(root, "build", process.platform === "win32" ? "scheduler.exe" : "scheduler"),
        path.join(root, "build", "scheduler.exe"),
        path.join(root, "build", "scheduler")
      ];
  for (const candidate of candidates) {
    try {
      if ((await stat(candidate)).isFile()) return candidate;
    } catch {
      // Try the next supported build path.
    }
  }
  throw new Error("Simulator executable not found. Build the C++ project first, or set SCHEDULER_BINARY.");
}

async function runSimulator(executable, args) {
  try {
    await execFileAsync(executable, args, {
      cwd: root,
      windowsHide: true,
      timeout: 30000,
      maxBuffer: 4 * 1024 * 1024
    });
  } catch (error) {
    const details = error.stderr?.trim() || error.message;
    throw new Error(`Simulator run failed: ${details}`);
  }
}

async function loadWorkloadList() {
  const folder = path.join(root, "workloads");
  const names = (await readdir(folder)).filter((name) => allowedWorkloads.has(name)).sort();
  return names.map((name) => ({ id: name, label: name.replace(/\.csv$/i, "").replaceAll("_", " ") }));
}

async function executeRun(rawConfig) {
  const config = validateConfig(rawConfig);
  const executable = await getSimulatorPath();
  const tempDir = await mkdtemp(path.join(os.tmpdir(), "scheduler-dashboard-"));
  const comparisonFile = path.join(tempDir, "comparison.json");
  const detailFile = path.join(tempDir, "details.json");
  const ganttFile = path.join(tempDir, "gantt.csv");

  const workloadArgs = config.workload === "generated"
    ? ["--random", String(config.processCount), "--distribution", config.distribution, "--seed", String(config.seed)]
    : ["--workload", path.join(root, "workloads", config.workload)];
  const sharedArgs = [
    ...workloadArgs,
    "--cores", String(config.cores),
    "--speeds", config.speeds,
    "--load-balancer", config.loadBalancer,
    "--quantum", String(config.quantum)
  ];

  try {
    await runSimulator(executable, ["--algo", "all", ...sharedArgs, "--output-json", comparisonFile]);
    await runSimulator(executable, [
      "--algo", config.algorithm,
      ...sharedArgs,
      "--output-json", detailFile,
      "--gantt-output", ganttFile
    ]);

    const [comparison, details, ganttText] = await Promise.all([
      readFile(comparisonFile, "utf8").then(JSON.parse),
      readFile(detailFile, "utf8").then(JSON.parse),
      readFile(ganttFile, "utf8")
    ]);
    const gantt = parseCsv(ganttText).map((segment) => ({
      core_id: Number(segment.core_id),
      pid: Number(segment.pid),
      name: segment.name,
      start_time: Number(segment.start_time),
      end_time: Number(segment.end_time),
      duration: Number(segment.duration)
    }));
    return {
      config,
      comparison: comparison.algorithms,
      details: { ...details, gantt }
    };
  } finally {
    await rm(tempDir, { recursive: true, force: true });
  }
}

async function readRequestBody(request) {
  const chunks = [];
  let bytes = 0;
  for await (const chunk of request) {
    bytes += chunk.length;
    if (bytes > 32768) throw new Error("Request body is too large.");
    chunks.push(chunk);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString("utf8"));
  } catch {
    throw new Error("Request body must contain valid JSON.");
  }
}

function sendJson(response, status, payload) {
  response.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store",
    "x-content-type-options": "nosniff"
  });
  response.end(JSON.stringify(payload));
}

async function serveStatic(request, response) {
  const relativePath = decodeURIComponent(new URL(request.url, "http://localhost").pathname).replace(/^\/+/, "");
  const requestedPath = path.resolve(staticRoot, relativePath || "index.html");
  if (requestedPath !== staticRoot && !requestedPath.startsWith(`${staticRoot}${path.sep}`)) {
    response.writeHead(403).end("Forbidden");
    return;
  }
  let target = requestedPath;
  try {
    if (!(await stat(target)).isFile()) target = path.join(staticRoot, "index.html");
  } catch {
    target = path.join(staticRoot, "index.html");
  }
  try {
    const body = await readFile(target);
    response.writeHead(200, {
      "content-type": mimeTypes[path.extname(target)] ?? "application/octet-stream",
      "x-content-type-options": "nosniff"
    });
    response.end(body);
  } catch {
    response.writeHead(404).end("Dashboard build not found. Run npm run build in web/.");
  }
}

export function createAppServer() {
  return createServer(async (request, response) => {
    try {
      const url = new URL(request.url, "http://localhost");
      if (request.method === "GET" && url.pathname === "/api/health") {
        sendJson(response, 200, { status: "ok" });
      } else if (request.method === "GET" && url.pathname === "/api/workloads") {
        sendJson(response, 200, { workloads: await loadWorkloadList() });
      } else if (request.method === "POST" && url.pathname === "/api/run") {
        sendJson(response, 200, await executeRun(await readRequestBody(request)));
      } else if (url.pathname.startsWith("/api/")) {
        sendJson(response, 404, { error: "API route not found." });
      } else if (request.method === "GET") {
        await serveStatic(request, response);
      } else {
        sendJson(response, 405, { error: "Method not allowed." });
      }
    } catch (error) {
      sendJson(response, error.message.includes("Request body") ? 400 : 422, { error: error.message });
    }
  });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const port = Number(process.env.PORT ?? 4174);
  const host = process.env.HOST ?? "127.0.0.1";
  createAppServer().listen(port, host, () => {
    console.log(`Scheduler dashboard server listening at http://${host}:${port}`);
  });
}
