import test from "node:test";
import assert from "node:assert/strict";
import { parseCsv, validateConfig } from "../server.js";

test("validates default and multicore simulation configuration", () => {
  const config = validateConfig({ cores: 2, speeds: "0.5,2", algorithm: "srtf" });
  assert.equal(config.algorithm, "srtf");
  assert.equal(config.speeds, "0.5,2");
  assert.equal(config.cores, 2);
  assert.equal(validateConfig({ workload: "generated" }).workload, "generated");
});

test("rejects unsupported executable inputs and invalid core speeds", () => {
  assert.throws(() => validateConfig({ algorithm: "custom-command" }), /supported scheduling algorithm/);
  assert.throws(() => validateConfig({ workload: "..\\outside.csv" }), /valid workload/);
  assert.throws(() => validateConfig({ cores: 2, speeds: "1" }), /exactly 2 core speeds/);
});

test("parses quoted CSV fields", () => {
  assert.deepEqual(parseCsv('pid,name,value\r\n1,"task, alpha",7\r\n'), [
    { pid: "1", name: "task, alpha", value: "7" }
  ]);
});
