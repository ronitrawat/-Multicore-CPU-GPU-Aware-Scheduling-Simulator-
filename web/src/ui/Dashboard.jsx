import { useEffect, useMemo, useState } from "react";
import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  Legend,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis
} from "recharts";

const algorithmNames = {
  fcfs: "FCFS",
  sjf: "SJF",
  srtf: "SRTF",
  rr: "Round Robin",
  priority: "Priority",
  "priority-np": "Priority (non-preemptive)",
  mlfq: "MLFQ"
};
const colors = ["#69e0bd", "#6fa8ff", "#b295ff", "#ffb86b", "#ff718f", "#75d9f2", "#c5e477", "#f18aca"];
const defaultConfig = {
  algorithm: "rr",
  workload: "example.csv",
  distribution: "uniform",
  processCount: 24,
  seed: 42,
  cores: 4,
  speeds: "1,1,1.5,2",
  quantum: 4,
  loadBalancer: "speedaware"
};
const format = (value, digits = 1) => Number.isFinite(Number(value)) ? Number(value).toFixed(digits) : "—";

function MetricCard({ label, value, unit, icon, tone }) {
  return (
    <article className={`metric-card ${tone}`}>
      <div className="metric-top"><span>{label}</span><span className="metric-icon">{icon}</span></div>
      <div className="metric-value">{value}<small>{unit}</small></div>
      <div className="metric-foot"><span className="live-dot" /> from latest simulation</div>
    </article>
  );
}

function GanttChart({ segments, cores, makespan }) {
  const [hover, setHover] = useState(null);
  const rows = useMemo(() => cores.map((core) => ({
    core,
    segments: segments.filter((segment) => segment.core_id === core.core_id)
  })), [cores, segments]);
  const width = Math.max(760, Math.min(1500, makespan * 11 || 760));
  const maxTime = Math.max(makespan, 1);

  return (
    <div className="timeline-scroll">
      <div className="timeline-inner" style={{ minWidth: width }}>
        <div className="timeline-axis">
          <span className="core-label">CORE</span>
          <div className="axis-track">
            {Array.from({ length: 6 }, (_, index) => {
              const tick = Math.round((maxTime * index) / 5);
              return <span key={index} style={{ left: `${(tick / maxTime) * 100}%` }}>{tick}</span>;
            })}
          </div>
        </div>
        {rows.map(({ core, segments: coreSegments }) => (
          <div className="timeline-row" key={core.core_id}>
            <div className="core-label">
              <b>Core {core.core_id}</b>
            <small>{format(core.speed, 1)}× · {format(core.utilization * 100, 0)}% busy</small>
            <span className="core-utilization"><i style={{ width: `${Math.max(0, Math.min(100, core.utilization * 100))}%` }} /></span>
            </div>
            <div className="timeline-track">
              {coreSegments.map((segment, index) => {
                const isIdle = segment.pid === 0;
                const isSwitch = segment.pid === 4294967295;
                return (
                  <button
                    type="button"
                    key={`${segment.start_time}-${index}`}
                    className={`timeline-segment ${isIdle ? "idle" : ""} ${isSwitch ? "switch" : ""}`}
                    style={{
                      left: `${(segment.start_time / maxTime) * 100}%`,
                      width: `${Math.max((segment.duration / maxTime) * 100, 0.4)}%`,
                      backgroundColor: isIdle || isSwitch ? undefined : colors[(segment.pid - 1) % colors.length]
                    }}
                    title={`${segment.name} · ${segment.start_time}–${segment.end_time} ticks`}
                    onMouseEnter={() => setHover({ ...segment, core_id: core.core_id })}
                    onMouseLeave={() => setHover(null)}
                    aria-label={`Core ${core.core_id}: ${segment.name} from tick ${segment.start_time} to ${segment.end_time}`}
                  />
                );
              })}
            </div>
          </div>
        ))}
        <div className="timeline-legend">
          <span><i className="legend-work" /> Process</span>
          <span><i className="legend-idle" /> Idle</span>
          <span><i className="legend-switch" /> Context switch</span>
          {hover && <strong>{hover.name} · Core {hover.core_id} · {hover.start_time}–{hover.end_time} ({hover.duration} ticks)</strong>}
        </div>
      </div>
    </div>
  );
}

function Dashboard() {
  const [config, setConfig] = useState(defaultConfig);
  const [workloads, setWorkloads] = useState([]);
  const [result, setResult] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [lastRun, setLastRun] = useState("");
  const generated = config.workload === "generated";

  useEffect(() => {
    fetch("/api/workloads")
      .then(async (response) => {
        if (!response.ok) throw new Error("Could not load sample workloads.");
        return response.json();
      })
      .then((data) => setWorkloads(data.workloads))
      .catch((reason) => setError(reason.message));
  }, []);

  const selected = result?.details;
  const summary = selected?.summary;
  const setField = (field, value) => setConfig((current) => ({ ...current, [field]: value }));

  async function runSimulation(event) {
    event.preventDefault();
    setLoading(true);
    setError("");
    try {
      const response = await fetch("/api/run", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(config)
      });
      const data = await response.json();
      if (!response.ok) throw new Error(data.error ?? "Simulation failed.");
      setResult(data);
      setLastRun(new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }));
    } catch (reason) {
      setError(reason.message);
    } finally {
      setLoading(false);
    }
  }

  const comparisonRows = result?.comparison ?? [];
  const latencyRows = summary ? [
    { name: "Waiting", average: summary.avg_waiting_time, p95: summary.p95_waiting_time },
    { name: "Turnaround", average: summary.avg_turnaround_time, p95: summary.p95_turnaround_time },
    { name: "Response", average: summary.avg_response_time, p95: summary.p95_response_time }
  ] : [];

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <a className="brand" href="#top" aria-label="Scheduler Lab home">
          <span className="brand-mark"><span /><span /><span /><span /></span>
          <span>scheduler<span className="brand-light">lab</span><small>SIMULATION WORKBENCH</small></span>
        </a>
        <div className="sidebar-label">WORKSPACE</div>
        <a className="nav-link active" href="#overview"><span className="nav-icon">◫</span> Overview <span className="nav-active" /></a>
        <a className="nav-link" href="#comparison"><span className="nav-icon">▥</span> Policy comparison</a>
        <a className="nav-link" href="#timeline"><span className="nav-icon">⌁</span> Execution timeline</a>
        <a className="nav-link" href="#processes"><span className="nav-icon">▤</span> Processes</a>
        <div className="sidebar-spacer" />
        <div className="sidebar-note">
          <span className="note-icon">✳</span>
          <b>Simulation, made visible.</b>
          <p>Explore how scheduling choices shape system performance.</p>
        </div>
        <div className="sidebar-version"><span className="live-dot" /> LOCAL SIMULATOR <span>v1.0</span></div>
      </aside>

      <main className="main-content" id="top">
        <header className="topbar">
          <div className="breadcrumb">Workspace <span>/</span> <b>Overview</b></div>
          <div className="topbar-right"><span className="connection"><i /> ENGINE CONNECTED</span><span className="avatar">SL</span></div>
        </header>

        <div className="dashboard-content" id="overview">
          <section className="page-heading">
            <div>
              <div className="eyebrow"><span className="eyebrow-line" /> CPU SCHEDULING SIMULATOR</div>
              <h1>Understand the <em>flow.</em></h1>
              <p>Configure a workload, run scheduling policies, and see how your system responds.</p>
            </div>
            {lastRun && <div className="last-run"><span className="live-dot" /> LAST RUN <b>{lastRun}</b></div>}
          </section>

          <section className="workspace-grid">
            <form className="config-panel panel" onSubmit={runSimulation}>
              <div className="panel-heading">
                <div><span className="section-index">01</span><h2>Simulation setup</h2></div>
                <span className="panel-tag">CONFIGURE</span>
              </div>
              <label className="field-label" htmlFor="workload">Workload source</label>
              <select id="workload" value={config.workload} onChange={(event) => setField("workload", event.target.value)}>
                {workloads.map((workload) => <option key={workload.id} value={workload.id}>{workload.label}</option>)}
                <option value="generated">Generate random workload</option>
              </select>
              {generated && (
                <div className="field-pair">
                  <div><label className="field-label" htmlFor="process-count">Process count</label><input id="process-count" type="number" min="1" max="150" value={config.processCount} onChange={(event) => setField("processCount", Number(event.target.value))} /></div>
                  <div><label className="field-label" htmlFor="distribution">Distribution</label><select id="distribution" value={config.distribution} onChange={(event) => setField("distribution", event.target.value)}><option value="uniform">Uniform</option><option value="exponential">Exponential</option><option value="bimodal">Bimodal</option></select></div>
                  <div className="field-full"><label className="field-label" htmlFor="seed">Random seed</label><input id="seed" type="number" min="0" value={config.seed} onChange={(event) => setField("seed", Number(event.target.value))} /></div>
                </div>
              )}
              <label className="field-label" htmlFor="algorithm">Scheduling policy</label>
              <select id="algorithm" value={config.algorithm} onChange={(event) => setField("algorithm", event.target.value)}>
                {Object.entries(algorithmNames).map(([value, label]) => <option key={value} value={value}>{label}</option>)}
              </select>
              {config.algorithm === "rr" && <><label className="field-label" htmlFor="quantum">Time quantum <small>TICKS</small></label><input id="quantum" type="number" min="1" max="1000" value={config.quantum} onChange={(event) => setField("quantum", Number(event.target.value))} /></>}
              <div className="field-pair">
                <div><label className="field-label" htmlFor="cores">CPU cores</label><select id="cores" value={config.cores} onChange={(event) => {
                  const count = Number(event.target.value);
                  const currentSpeeds = config.speeds.split(",").map((speed) => speed.trim()).filter(Boolean);
                  const fallback = currentSpeeds.at(-1) ?? "1";
                  const speeds = Array.from({ length: count }, (_, index) => currentSpeeds[index] ?? fallback).join(",");
                  setConfig((current) => ({ ...current, cores: count, speeds }));
                }}>{[1, 2, 4, 8, 16].map((count) => <option key={count} value={count}>{count} cores</option>)}</select></div>
                <div><label className="field-label" htmlFor="balancer">Load balancer</label><select id="balancer" value={config.loadBalancer} onChange={(event) => setField("loadBalancer", event.target.value)}><option value="global">Global queue</option><option value="percore">Per-core</option><option value="speedaware">Speed-aware</option></select></div>
                <div className="field-full"><label className="field-label" htmlFor="speeds">Core speeds <small>COMMA SEPARATED · ×</small></label><input id="speeds" value={config.speeds} onChange={(event) => setField("speeds", event.target.value)} placeholder="1,1,1.5,2" /></div>
              </div>
              <div className="config-hint"><span>ⓘ</span> Enter one speed multiplier for each core.</div>
              <button className="run-button" type="submit" disabled={loading}>
                {loading ? <><span className="spinner" /> Running simulation…</> : <>Run simulation <span>↗</span></>}
              </button>
              {error && <div className="error-message" role="alert">{error}</div>}
            </form>

            <div className="results-column">
              <div className="results-header">
                <div><span className="section-index">02</span><h2>Performance snapshot</h2></div>
                <span className={`status-pill ${selected ? "ready" : ""}`}><i /> {loading ? "RUNNING" : selected ? "RESULTS READY" : "AWAITING RUN"}</span>
              </div>
              <div className="metrics-grid">
                <MetricCard label="Makespan" value={summary ? format(summary.makespan, 0) : "—"} unit="ticks" icon="◷" tone="mint" />
                <MetricCard label="Avg. waiting" value={summary ? format(summary.avg_waiting_time) : "—"} unit="ticks" icon="⌛" tone="blue" />
                <MetricCard label="CPU utilization" value={summary ? `${format(summary.cpu_utilization * 100, 1)}%` : "—"} unit="" icon="◉" tone="violet" />
                <MetricCard label="Fairness index" value={summary ? format(summary.jains_fairness_index, 3) : "—"} unit="/ 1.0" icon="◎" tone="amber" />
              </div>
              <div className="panel insight-panel">
                <div className="panel-heading compact"><div><span className="section-index">03</span><h2>Run insights</h2></div><span className="panel-tag">LIVE DATA</span></div>
                {summary ? (
                  <div className="insight-content">
                    <div className="insight-stat"><span>PROCESSES COMPLETED</span><b>{summary.total_processes}<small> tasks</small></b></div>
                    <div className="insight-stat"><span>THROUGHPUT</span><b>{format(summary.throughput, 3)}<small> / tick</small></b></div>
                    <div className="insight-stat"><span>CONTEXT SWITCHES</span><b>{summary.total_context_switches}<small> switches</small></b></div>
                    <div className="insight-stat"><span>STARVED PROCESSES</span><b>{summary.starved_process_count}<small> tasks</small></b></div>
                  </div>
                ) : (
                  <div className="empty-insight"><span className="empty-pulse">↗</span><div><b>Your results will appear here</b><p>Run a simulation to reveal process metrics and system behavior.</p></div></div>
                )}
              </div>
              <div className="environment-strip">
                <span><i /> DISCRETE-EVENT ENGINE</span><span>BIG.LITTLE READY</span><span>POLICY: {selected ? algorithmNames[selected.algorithm] ?? selected.algorithm : "—"}</span>
              </div>
            </div>
          </section>

          <section className="lower-grid" id="comparison">
            <article className="panel chart-panel">
              <div className="panel-heading"><div><span className="section-index">04</span><div><h2>Policy comparison</h2><p>Same workload, different scheduling decisions</p></div></div><span className="panel-tag">ALL POLICIES</span></div>
              {comparisonRows.length ? (
                <ResponsiveContainer width="100%" height={300}>
                  <BarChart data={comparisonRows} margin={{ top: 14, right: 8, left: -12, bottom: 2 }}>
                    <CartesianGrid stroke="#242a35" vertical={false} />
                    <XAxis dataKey="name" tick={{ fill: "#8993a5", fontSize: 11 }} tickLine={false} axisLine={false} />
                    <YAxis tick={{ fill: "#758093", fontSize: 11 }} tickLine={false} axisLine={false} />
                    <Tooltip contentStyle={{ background: "#151a23", border: "1px solid #303846", borderRadius: 8, color: "#e7ecf4" }} />
                    <Legend wrapperStyle={{ color: "#8993a5", fontSize: 11, paddingTop: 12 }} />
                    <Bar dataKey="avg_waiting_time" name="Avg waiting" fill="#69e0bd" radius={[4, 4, 0, 0]} />
                    <Bar dataKey="avg_response_time" name="Avg response" fill="#6fa8ff" radius={[4, 4, 0, 0]} />
                    <Bar dataKey="avg_turnaround_time" name="Avg turnaround" fill="#b295ff" radius={[4, 4, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              ) : <div className="chart-empty"><span>▥</span><p>Run a simulation to compare all seven scheduling policies.</p></div>}
            </article>
            <article className="panel chart-panel">
              <div className="panel-heading"><div><span className="section-index">05</span><div><h2>Latency profile</h2><p>Average versus 95th percentile · selected policy</p></div></div><span className="panel-tag">TICKS</span></div>
              {latencyRows.length ? (
                <ResponsiveContainer width="100%" height={300}>
                  <BarChart data={latencyRows} margin={{ top: 14, right: 8, left: -12, bottom: 2 }}>
                    <CartesianGrid stroke="#242a35" vertical={false} />
                    <XAxis dataKey="name" tick={{ fill: "#8993a5", fontSize: 11 }} tickLine={false} axisLine={false} />
                    <YAxis tick={{ fill: "#758093", fontSize: 11 }} tickLine={false} axisLine={false} />
                    <Tooltip contentStyle={{ background: "#151a23", border: "1px solid #303846", borderRadius: 8, color: "#e7ecf4" }} />
                    <Legend wrapperStyle={{ color: "#8993a5", fontSize: 11, paddingTop: 12 }} />
                    <Bar dataKey="average" name="Average" fill="#69e0bd" radius={[4, 4, 0, 0]} />
                    <Bar dataKey="p95" name="P95" fill="#ffb86b" radius={[4, 4, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              ) : <div className="chart-empty"><span>⌁</span><p>Latency percentiles are shown after a run.</p></div>}
            </article>
          </section>

          <section className="panel timeline-panel" id="timeline">
            <div className="panel-heading"><div><span className="section-index">06</span><div><h2>Multicore execution timeline</h2><p>Every segment comes directly from the simulator’s Gantt export</p></div></div><span className="panel-tag">{selected ? `${selected.gantt.length} SEGMENTS` : "PER-CORE VIEW"}</span></div>
            {selected ? <GanttChart segments={selected.gantt} cores={selected.cores} makespan={summary.makespan} /> : <div className="timeline-empty"><span>⌁</span><b>Timeline ready when you are</b><p>Run the simulator to inspect process execution, idle periods, and core activity.</p></div>}
          </section>

          <section className="panel process-panel" id="processes">
            <div className="panel-heading"><div><span className="section-index">07</span><div><h2>Process metrics</h2><p>Per-task scheduling outcomes for the selected policy</p></div></div><span className="panel-tag">{selected ? `${selected.processes.length} PROCESSES` : "PROCESS TABLE"}</span></div>
            {selected ? (
              <div className="table-scroll"><table>
                <thead><tr><th>PROCESS</th><th>ARRIVAL</th><th>BURST</th><th>COMPLETION</th><th>TURNAROUND</th><th>WAITING</th><th>RESPONSE</th><th>STATUS</th></tr></thead>
                <tbody>{selected.processes.map((process) => <tr key={process.pid}>
                  <td><span className="process-dot" style={{ background: colors[(process.pid - 1) % colors.length] }} />{process.name}<small>PID {process.pid}</small></td>
                  <td>{process.arrival_time}</td><td>{process.burst_time}</td><td>{process.completion_time}</td>
                  <td>{process.turnaround_time}<span className="unit"> ticks</span></td><td>{process.waiting_time}<span className="unit"> ticks</span></td>
                  <td>{process.response_time}<span className="unit"> ticks</span></td>
                  <td><span className={`process-status ${process.starved ? "starved" : ""}`}><i />{process.starved ? "STARVED" : "COMPLETE"}</span></td>
                </tr>)}</tbody>
              </table></div>
            ) : <div className="table-empty">Run a simulation to populate per-process results.</div>}
          </section>

          <footer className="page-footer"><span>Built on the C++17 discrete-event simulation engine</span><span>LOCAL WORKSPACE <i /></span></footer>
        </div>
      </main>
    </div>
  );
}

export default Dashboard;
