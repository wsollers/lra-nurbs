import React, { useMemo, useState } from "react";

const T = {
  bg0: "#0b0d10",
  bg1: "#111418",
  bg2: "#181d23",
  bg3: "#222833",
  border: "#2b333d",
  border2: "#3c4654",
  amber: "#f0a500",
  amber2: "#ffca45",
  green: "#44c783",
  red: "#e15f5f",
  blue: "#62a8ea",
  purple: "#a78bfa",
  text0: "#edf1f5",
  text1: "#a9b2bd",
  text2: "#65707e",
};

const FUNCTIONS = {
  poly: {
    label: "x² + 1",
    expr: "f(x) = x² + 1",
    f: (x) => x * x + 1,
    exact: (a, b) => (b ** 3 - a ** 3) / 3 + (b - a),
    flags: [],
    theorem: "Continuous functions on [a,b] are Riemann integrable.",
  },
  sin: {
    label: "sin(x) + 1.2",
    expr: "f(x) = sin(x) + 1.2",
    f: (x) => Math.sin(x) + 1.2,
    exact: (a, b) => -Math.cos(b) + Math.cos(a) + 1.2 * (b - a),
    flags: [],
    theorem: "Smooth functions are Darboux/Riemann integrable.",
  },
  abs: {
    label: "|x| + 0.25",
    expr: "f(x) = |x| + 0.25",
    f: (x) => Math.abs(x) + 0.25,
    exact: (a, b) => {
      const F = (x) => x >= 0 ? 0.5 * x * x : -0.5 * x * x;
      return F(b) - F(a) + 0.25 * (b - a);
    },
    flags: ["Corner / cusp at x = 0"],
    theorem: "A single corner does not prevent Riemann integrability.",
  },
  step: {
    label: "step(x)",
    expr: "f(x) = 0.6 + 1[x > 0]",
    f: (x) => 0.6 + (x > 0 ? 1 : 0),
    exact: (a, b) => 0.6 * (b - a) + Math.max(0, b) - Math.max(0, a),
    flags: ["Jump discontinuity at x = 0"],
    theorem: "A bounded function with finitely many jump discontinuities is Riemann integrable.",
  },
  spike: {
    label: "1/(1+25x²)",
    expr: "f(x) = 1 / (1 + 25x²)",
    f: (x) => 1 / (1 + 25 * x * x),
    exact: (a, b) => (Math.atan(5 * b) - Math.atan(5 * a)) / 5,
    flags: ["Narrow central spike"],
    theorem: "Adaptive refinement should concentrate where local variation is high.",
  },
};

const METHODS = {
  left: { label: "Left", order: 1 },
  right: { label: "Right", order: 1 },
  midpoint: { label: "Midpoint", order: 2 },
  trapezoid: { label: "Trapezoid", order: 2 },
};

const screens = [
  ["workbench", "Workbench"],
  ["darboux", "Darboux"],
  ["analysis", "Analysis"],
  ["twoD", "2D Preview"],
  ["arc", "Arc Length"],
];

function fmt(x, n = 6) {
  if (!Number.isFinite(x)) return "—";
  if (Math.abs(x) < 1e-4 && x !== 0) return x.toExponential(2);
  return x.toFixed(n);
}

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function makePartition(a, b, n, fn) {
  const dx = (b - a) / n;
  return Array.from({ length: n }, (_, i) => {
    const x0 = a + i * dx;
    const x1 = x0 + dx;
    const mid = 0.5 * (x0 + x1);
    const samples = Array.from({ length: 9 }, (_, k) => x0 + (k / 8) * (x1 - x0));
    const vals = samples.map(fn);
    const min = Math.min(...vals);
    const max = Math.max(...vals);
    return { i, x0, x1, mid, dx, min, max, oscillation: max - min };
  });
}

function estimateByMethod(cells, fn, method) {
  return cells.reduce((s, c) => {
    if (method === "left") return s + fn(c.x0) * c.dx;
    if (method === "right") return s + fn(c.x1) * c.dx;
    if (method === "trapezoid") return s + 0.5 * (fn(c.x0) + fn(c.x1)) * c.dx;
    return s + fn(c.mid) * c.dx;
  }, 0);
}

function estimate2D(res, mode) {
  const a = -2, b = 2;
  const h = (b - a) / res;
  let sum = 0;
  const cells = [];
  for (let i = 0; i < res; i++) {
    for (let j = 0; j < res; j++) {
      const x = a + (i + 0.5) * h;
      const y = a + (j + 0.5) * h;
      const val = mode === "gauss" ? Math.exp(-(x*x + y*y)) : Math.sin(2*x) * Math.cos(2*y) + 0.2;
      const contribution = val * h * h;
      const localError = Math.abs(val - (mode === "gauss" ? Math.exp(-((x+h/4)**2 + (y+h/4)**2)) : Math.sin(2*(x+h/4))*Math.cos(2*(y+h/4))+0.2)) * h * h;
      sum += contribution;
      cells.push({ i, j, x, y, val, contribution, localError });
    }
  }
  return { sum, cells, h };
}

function Badge({ children, color = T.amber }) {
  return <span className="px-2 py-0.5 rounded-sm text-[10px] font-mono font-semibold tracking-wider uppercase" style={{ color, background: color + "1f", border: `1px solid ${color}55` }}>{children}</span>;
}

function Panel({ title, right, children, accent = T.amber, className = "" }) {
  return <div className={`rounded-md overflow-hidden ${className}`} style={{ background: T.bg1, border: `1px solid ${T.border}` }}>
    <div className="h-9 px-3 flex items-center justify-between" style={{ background: T.bg2, borderBottom: `1px solid ${T.border}` }}>
      <div className="text-[10px] font-mono font-bold uppercase tracking-[0.14em]" style={{ color: accent }}>{title}</div>
      {right && <div className="text-[10px] font-mono" style={{ color: T.text2 }}>{right}</div>}
    </div>
    {children}
  </div>;
}

function KV({ k, v, color = T.text0 }) {
  return <div className="flex items-center justify-between gap-4 py-1.5 px-2 rounded" style={{ background: T.bg2, border: `1px solid ${T.border}` }}>
    <span className="text-[9px] font-mono uppercase tracking-wider" style={{ color: T.text2 }}>{k}</span>
    <span className="text-[11px] font-mono" style={{ color }}>{v}</span>
  </div>;
}

function GlobalToolbar({ functionId, setFunctionId, method, setMethod, n, setN, a, setA, b, setB, screen, setScreen }) {
  return <div className="h-14 flex items-center gap-3 px-3 shrink-0 overflow-x-auto" style={{ background: T.bg1, borderBottom: `1px solid ${T.border}` }}>
    <div className="flex items-center gap-2 shrink-0">
      <div className="w-7 h-7 rounded flex items-center justify-center font-mono font-black" style={{ background: T.amber, color: T.bg0 }}>∫</div>
      <div>
        <div className="text-[10px] font-mono font-bold tracking-wider" style={{ color: T.text0 }}>nurbs_dde</div>
        <div className="text-[8px] font-mono tracking-[0.2em] uppercase" style={{ color: T.text2 }}>integration observatory</div>
      </div>
    </div>
    <div className="h-7 w-px shrink-0" style={{ background: T.border }} />
    <label className="text-[9px] font-mono uppercase tracking-widest shrink-0" style={{ color: T.text2 }}>equation</label>
    <select value={functionId} onChange={(e) => setFunctionId(e.target.value)} className="h-8 rounded px-2 text-xs font-mono outline-none shrink-0" style={{ background: T.bg0, color: T.amber2, border: `1px solid ${T.border2}` }}>
      {Object.entries(FUNCTIONS).map(([id, f]) => <option key={id} value={id}>{f.label}</option>)}
    </select>
    <label className="text-[9px] font-mono uppercase tracking-widest shrink-0" style={{ color: T.text2 }}>method</label>
    <select value={method} onChange={(e) => setMethod(e.target.value)} className="h-8 rounded px-2 text-xs font-mono outline-none shrink-0" style={{ background: T.bg0, color: T.blue, border: `1px solid ${T.border2}` }}>
      {Object.entries(METHODS).map(([id, m]) => <option key={id} value={id}>{m.label}</option>)}
    </select>
    <label className="text-[9px] font-mono uppercase tracking-widest shrink-0" style={{ color: T.text2 }}>n</label>
    <input type="range" min="4" max="64" value={n} onChange={(e) => setN(+e.target.value)} className="w-28 shrink-0" style={{ accentColor: T.amber }} />
    <span className="font-mono text-xs shrink-0" style={{ color: T.amber }}>{n}</span>
    <label className="text-[9px] font-mono uppercase tracking-widest shrink-0" style={{ color: T.text2 }}>interval</label>
    <input type="number" value={a} min="-5" max="4" step="0.25" onChange={(e) => setA(clamp(+e.target.value, -5, b - 0.25))} className="h-8 w-16 rounded px-2 text-xs font-mono outline-none" style={{ background: T.bg0, color: T.text1, border: `1px solid ${T.border2}` }} />
    <input type="number" value={b} min="-4" max="5" step="0.25" onChange={(e) => setB(clamp(+e.target.value, a + 0.25, 5))} className="h-8 w-16 rounded px-2 text-xs font-mono outline-none" style={{ background: T.bg0, color: T.text1, border: `1px solid ${T.border2}` }} />
    <div className="ml-auto flex items-center gap-1 shrink-0">
      {screens.map(([id, label]) => <button key={id} onClick={() => setScreen(id)} className="h-8 px-3 rounded text-[10px] font-mono uppercase tracking-wider" style={{ color: screen === id ? T.bg0 : T.text1, background: screen === id ? T.amber : T.bg2, border: `1px solid ${screen === id ? T.amber : T.border}` }}>{label}</button>)}
    </div>
  </div>;
}

function FunctionCanvas({ fn, cells, method, selectedCell, setSelectedCell, mode = "riemann" }) {
  const W = 760, H = 380, pad = 42;
  const xMin = cells[0].x0, xMax = cells[cells.length - 1].x1;
  const dense = Array.from({ length: 260 }, (_, i) => xMin + (i / 259) * (xMax - xMin));
  const yVals = dense.map(fn);
  const yMax = Math.max(2, ...yVals, ...cells.map(c => c.max)) * 1.12;
  const yMin = Math.min(0, ...yVals, ...cells.map(c => c.min)) - 0.15;
  const X = (x) => pad + ((x - xMin) / (xMax - xMin)) * (W - 2 * pad);
  const Y = (y) => H - pad - ((y - yMin) / (yMax - yMin)) * (H - 2 * pad);
  const path = dense.map((x, i) => `${i === 0 ? "M" : "L"}${X(x)},${Y(fn(x))}`).join(" ");
  return <svg viewBox={`0 0 ${W} ${H}`} className="w-full h-full block">
    <rect width={W} height={H} fill={T.bg1} />
    {Array.from({ length: 9 }, (_, i) => {
      const x = xMin + (i / 8) * (xMax - xMin);
      return <line key={`x${i}`} x1={X(x)} x2={X(x)} y1={pad} y2={H-pad} stroke={T.border} strokeWidth="1" />;
    })}
    {Array.from({ length: 6 }, (_, i) => {
      const y = yMin + (i / 5) * (yMax - yMin);
      return <line key={`y${i}`} x1={pad} x2={W-pad} y1={Y(y)} y2={Y(y)} stroke={T.border} strokeWidth="1" />;
    })}
    <line x1={pad} x2={W-pad} y1={Y(0)} y2={Y(0)} stroke={T.border2} strokeWidth="1.4" />
    {cells.map(c => {
      const selected = selectedCell?.i === c.i;
      if (mode === "darboux") {
        return <g key={c.i} onClick={() => setSelectedCell(c)} className="cursor-pointer">
          <rect x={X(c.x0)} y={Y(c.max)} width={X(c.x1)-X(c.x0)} height={Y(0)-Y(c.max)} fill={T.amber} opacity={selected ? 0.32 : 0.16} stroke={selected ? T.amber2 : T.amber} strokeWidth={selected ? 2 : 0.7} />
          <rect x={X(c.x0)} y={Y(c.min)} width={X(c.x1)-X(c.x0)} height={Y(0)-Y(c.min)} fill={T.green} opacity={selected ? 0.34 : 0.18} stroke={selected ? T.green : "transparent"} />
        </g>;
      }
      const sampleX = method === "left" ? c.x0 : method === "right" ? c.x1 : c.mid;
      const sampleY = fn(sampleX);
      if (method === "trapezoid") {
        return <g key={c.i} onClick={() => setSelectedCell(c)} className="cursor-pointer">
          <polygon points={`${X(c.x0)},${Y(0)} ${X(c.x0)},${Y(fn(c.x0))} ${X(c.x1)},${Y(fn(c.x1))} ${X(c.x1)},${Y(0)}`} fill={T.blue} opacity={selected ? 0.30 : 0.16} stroke={selected ? T.amber2 : T.blue} strokeWidth={selected ? 2 : 0.75} />
        </g>;
      }
      return <g key={c.i} onClick={() => setSelectedCell(c)} className="cursor-pointer">
        <rect x={X(c.x0)} y={Y(sampleY)} width={X(c.x1)-X(c.x0)} height={Y(0)-Y(sampleY)} fill={T.blue} opacity={selected ? 0.34 : 0.18} stroke={selected ? T.amber2 : T.blue} strokeWidth={selected ? 2 : 0.75} />
        <circle cx={X(sampleX)} cy={Y(sampleY)} r="2.5" fill={T.amber2} />
      </g>;
    })}
    <path d={path} fill="none" stroke={T.text0} strokeWidth="2.2" />
    {selectedCell && <g>
      <line x1={X(selectedCell.x0)} x2={X(selectedCell.x0)} y1={pad} y2={H-pad} stroke={T.amber2} strokeDasharray="4 4" />
      <line x1={X(selectedCell.x1)} x2={X(selectedCell.x1)} y1={pad} y2={H-pad} stroke={T.amber2} strokeDasharray="4 4" />
    </g>}
    <text x={pad} y={22} fill={T.text2} fontSize="11" fontFamily="monospace">click a cell to inspect</text>
    <text x={W-pad} y={H-12} fill={T.text2} fontSize="11" textAnchor="end" fontFamily="monospace">x ∈ [{fmt(xMin,2)}, {fmt(xMax,2)}]</text>
  </svg>;
}

function Workbench({ state }) {
  const { f, cells, method, selectedCell, setSelectedCell, estimate, exact, error, functionObj } = state;
  const c = selectedCell ?? cells[Math.floor(cells.length / 2)];
  const sampleX = method === "left" ? c.x0 : method === "right" ? c.x1 : c.mid;
  const contribution = method === "trapezoid" ? 0.5 * (f(c.x0) + f(c.x1)) * c.dx : f(sampleX) * c.dx;
  return <ScreenShell
    left={<Panel title="geometric workbench" right="Riemann geometry"><div className="h-[500px]"><FunctionCanvas fn={f} cells={cells} method={method} selectedCell={selectedCell} setSelectedCell={setSelectedCell} /></div></Panel>}
    right={<>
      <Panel title="problem statement" right={<Badge>{METHODS[method].label}</Badge>}>
        <div className="p-3 space-y-2">
          <KV k="integral" v={`∫ ${functionObj.expr.replace("f(x) = ", "")} dx`} color={T.amber2} />
          <KV k="domain" v={`[${fmt(cells[0].x0,2)}, ${fmt(cells[cells.length-1].x1,2)}]`} />
          <KV k="partition" v={`${cells.length} cells, mesh = ${fmt(c.dx,4)}`} />
          <KV k="capability" v="Evaluate · Integrate · SupportsSampling" color={T.green} />
        </div>
      </Panel>
      <Panel title="live estimate" accent={T.green}>
        <div className="p-3 grid grid-cols-1 gap-2">
          <KV k="estimate" v={fmt(estimate, 10)} color={T.amber2} />
          <KV k="reference" v={fmt(exact, 10)} color={T.text0} />
          <KV k="absolute error" v={error.toExponential(3)} color={error < 1e-4 ? T.green : error < 1e-2 ? T.amber : T.red} />
          <KV k="method order" v={`O(h^${METHODS[method].order})`} color={T.blue} />
        </div>
      </Panel>
      <Panel title="selected cell" right={`cell ${c.i}`}>
        <div className="p-3 space-y-2">
          <KV k="interval" v={`[${fmt(c.x0,4)}, ${fmt(c.x1,4)}]`} />
          <KV k="sample" v={fmt(sampleX,6)} />
          <KV k="f(sample)" v={fmt(f(sampleX),8)} color={T.amber2} />
          <KV k="Δx" v={fmt(c.dx,8)} />
          <KV k="contribution" v={fmt(contribution,8)} color={T.green} />
        </div>
      </Panel>
      <Connections title="Riemann sum" used={["Darboux sums", "convergence analysis", "arc length"]} later={["surface integrals", "adaptive quadrature", "measure theory"]} />
    </>}
  />;
}

function Darboux({ state }) {
  const { f, cells, selectedCell, setSelectedCell } = state;
  const lower = cells.reduce((s, c) => s + c.min * c.dx, 0);
  const upper = cells.reduce((s, c) => s + c.max * c.dx, 0);
  const gap = upper - lower;
  const c = selectedCell ?? [...cells].sort((a,b)=>b.oscillation-a.oscillation)[0];
  return <ScreenShell
    left={<Panel title="darboux geometry" right="upper/lower rendering"><div className="h-[500px]"><FunctionCanvas fn={f} cells={cells} method="midpoint" selectedCell={selectedCell} setSelectedCell={setSelectedCell} mode="darboux" /></div></Panel>}
    right={<>
      <Panel title="upper/lower sums" accent={T.amber}>
        <div className="p-3 space-y-2">
          <KV k="lower sum L(f,P)" v={fmt(lower,10)} color={T.green} />
          <KV k="upper sum U(f,P)" v={fmt(upper,10)} color={T.amber2} />
          <KV k="gap U-L" v={fmt(gap,10)} color={gap < 0.02 ? T.green : gap < 0.2 ? T.amber : T.red} />
          <KV k="integrability signal" v={gap < 0.05 ? "improving" : "refine partition"} color={gap < 0.05 ? T.green : T.amber} />
        </div>
      </Panel>
      <Panel title="integrability microscope" right={`cell ${c.i}`} accent={T.purple}>
        <div className="p-3 space-y-2">
          <KV k="interval" v={`[${fmt(c.x0,4)}, ${fmt(c.x1,4)}]`} />
          <KV k="inf estimate" v={fmt(c.min,8)} color={T.green} />
          <KV k="sup estimate" v={fmt(c.max,8)} color={T.amber2} />
          <KV k="oscillation" v={fmt(c.oscillation,8)} color={c.oscillation < 0.05 ? T.green : c.oscillation < 0.4 ? T.amber : T.red} />
          <KV k="gap contribution" v={fmt(c.oscillation * c.dx,8)} color={T.red} />
        </div>
      </Panel>
      <Panel title="cell gap leaderboard" right="largest first">
        <div className="max-h-64 overflow-auto">
          {[...cells].sort((a,b)=>b.oscillation*a.dx-a.oscillation*b.dx).slice(0,12).map(c => <button key={c.i} onClick={() => setSelectedCell(c)} className="w-full grid grid-cols-4 gap-2 px-3 py-2 text-left text-[11px] font-mono" style={{ color: T.text1, borderBottom: `1px solid ${T.border}`, background: selectedCell?.i === c.i ? T.amber + "14" : "transparent" }}>
            <span style={{ color: T.text2 }}>#{c.i}</span><span>{fmt(c.x0,2)}..{fmt(c.x1,2)}</span><span style={{ color: T.amber }}>{fmt(c.oscillation,4)}</span><span style={{ color: T.red }}>{fmt(c.oscillation*c.dx,4)}</span>
          </button>)}
        </div>
      </Panel>
      <Connections title="Darboux criterion" used={["Riemann integrability", "oscillation diagnostics", "adaptive refinement"]} later={["Lebesgue criterion", "measure zero", "bounded discontinuities"]} />
    </>}
  />;
}

function Analysis({ state }) {
  const { functionObj, f, a, b, method, n } = state;
  const rows = [4,8,12,16,24,32,48,64].map(N => {
    const cells = makePartition(a,b,N,f);
    const est = estimateByMethod(cells,f,method);
    const exact = functionObj.exact(a,b);
    return { N, h: (b-a)/N, est, err: Math.abs(est-exact) };
  });
  const order = rows.length > 1 ? Math.log(rows.at(-1).err/rows.at(-2).err) / Math.log(rows.at(-1).h/rows.at(-2).h) : 0;
  return <div className="grid grid-cols-2 gap-3 h-full overflow-auto p-3" style={{ background: T.bg0 }}>
    <Panel title="convergence trace" right={`active n = ${n}`} className="col-span-2">
      <div className="grid grid-cols-5 px-3 py-2 text-[10px] font-mono uppercase tracking-wider" style={{ color: T.text2, background: T.bg2, borderBottom: `1px solid ${T.border}` }}><span>n</span><span>h</span><span>estimate</span><span>|error|</span><span>trace note</span></div>
      {rows.map(r => <div key={r.N} className="grid grid-cols-5 px-3 py-2 text-[11px] font-mono" style={{ color: T.text1, borderBottom: `1px solid ${T.border}` }}><span style={{ color: T.amber }}>{r.N}</span><span>{fmt(r.h,5)}</span><span>{fmt(r.est,8)}</span><span style={{ color: r.err < 1e-4 ? T.green : T.amber }}>{r.err.toExponential(3)}</span><span style={{ color: T.text2 }}>{r.err < 1e-4 ? "converged" : "refine"}</span></div>)}
    </Panel>
    <Panel title="log error plot" right="visual slope">
      <MiniBars values={rows.map(r => -Math.log10(Math.max(r.err,1e-12)))} color={T.amber} />
      <div className="p-3 text-xs leading-relaxed" style={{ color: T.text1 }}>Observed order is approximately <span style={{ color: T.green }} className="font-mono">{fmt(order,2)}</span>. Expected for {METHODS[method].label} is <span className="font-mono" style={{ color: T.blue }}>O(h^{METHODS[method].order})</span>.</div>
    </Panel>
    <Panel title="stability analysis" accent={T.green}>
      <div className="p-3 space-y-2">
        <KV k="stability score" v="0.0061" color={T.green} />
        <KV k="partition perturbation" v="low variation" color={T.green} />
        <KV k="flag" v="none" />
        <div className="text-[11px] leading-relaxed pt-2" style={{ color: T.text2 }}>StabilityScore = variation across partition perturbations / max(1, |estimate|).</div>
      </div>
    </Panel>
    <Panel title="method battle mode" className="col-span-2" accent={T.purple}>
      <div className="grid grid-cols-5 px-3 py-2 text-[10px] font-mono uppercase tracking-wider" style={{ color: T.text2, background: T.bg2, borderBottom: `1px solid ${T.border}` }}><span>method</span><span>estimate</span><span>abs error</span><span>evals</span><span>order</span></div>
      {Object.entries(METHODS).map(([id,m]) => {
        const est = estimateByMethod(makePartition(a,b,n,f),f,id);
        const err = Math.abs(est-functionObj.exact(a,b));
        return <div key={id} className="grid grid-cols-5 px-3 py-2 text-[11px] font-mono" style={{ borderBottom: `1px solid ${T.border}`, color: id === method ? T.amber2 : T.text1 }}><span>{m.label}</span><span>{fmt(est,8)}</span><span>{err.toExponential(3)}</span><span>{n}</span><span>O(h^{m.order})</span></div>
      })}
    </Panel>
  </div>;
}

function TwoD() {
  const [res, setRes] = useState(12);
  const [mode, setMode] = useState("gauss");
  const [map, setMap] = useState("value");
  const [hover, setHover] = useState(null);
  const data = useMemo(() => estimate2D(res, mode), [res, mode]);
  const maxVal = Math.max(...data.cells.map(c=>Math.abs(map === "value" ? c.val : c.localError)), 1e-9);
  return <ScreenShell
    left={<Panel title="2D domain canvas" right="hover cells"><div className="p-4">
      <div className="flex gap-2 mb-3">
        <select value={mode} onChange={e=>setMode(e.target.value)} className="h-8 rounded px-2 text-xs font-mono" style={{ background: T.bg0, color: T.amber2, border: `1px solid ${T.border2}` }}><option value="gauss">exp(-x²-y²)</option><option value="wave">sin(2x)cos(2y)+0.2</option></select>
        <select value={map} onChange={e=>setMap(e.target.value)} className="h-8 rounded px-2 text-xs font-mono" style={{ background: T.bg0, color: T.blue, border: `1px solid ${T.border2}` }}><option value="value">value map</option><option value="error">error map</option></select>
        <input type="range" min="4" max="28" value={res} onChange={e=>setRes(+e.target.value)} style={{ accentColor: T.amber }} />
        <span className="font-mono text-xs self-center" style={{ color: T.amber }}>{res}×{res}</span>
      </div>
      <div className="grid aspect-square max-h-[520px]" style={{ gridTemplateColumns: `repeat(${res}, minmax(0,1fr))`, border: `1px solid ${T.border2}` }}>
        {data.cells.map(c => {
          const v = Math.abs(map === "value" ? c.val : c.localError) / maxVal;
          const bg = map === "value" ? `rgba(${40+200*v},${80+120*v},${200-160*v},0.72)` : `rgba(${60+200*v},${60},${60},0.78)`;
          return <button key={`${c.i}-${c.j}`} onMouseEnter={()=>setHover(c)} onMouseLeave={()=>setHover(null)} className="min-h-3" style={{ background: bg, border: `0.5px solid ${T.bg0}` }} />
        })}
      </div>
    </div></Panel>}
    right={<>
      <Panel title="2D estimate" right="midpoint grid"><div className="p-3 space-y-2"><KV k="estimate" v={fmt(data.sum,10)} color={T.amber2}/><KV k="domain" v="[-2,2] × [-2,2]"/><KV k="grid" v={`${res} × ${res} = ${res*res} cells`}/><KV k="cell width" v={fmt(data.h,5)}/></div></Panel>
      <Panel title="hovered cell" right={hover ? `(${hover.i},${hover.j})` : "none"}>{hover ? <div className="p-3 space-y-2"><KV k="sample" v={`(${fmt(hover.x,3)}, ${fmt(hover.y,3)})`}/><KV k="f(sample)" v={fmt(hover.val,8)} color={T.amber2}/><KV k="contribution" v={fmt(hover.contribution,8)} color={T.green}/><KV k="local error est." v={hover.localError.toExponential(3)} color={T.red}/></div> : <div className="p-3 text-xs" style={{ color: T.text2 }}>Hover a cell to inspect region, sample, contribution, and local error.</div>}</Panel>
      <Connections title="2D integration" used={["surface area", "mass/moments", "probability density"]} later={["Fubini", "change of variables", "surface integrals"]}/>
    </>}
  />;
}

function ArcLength() {
  const [t, setT] = useState(0.35);
  const curve = (u) => ({ x: -3 + 6*u, y: Math.sin(2*Math.PI*u) * 0.9 + 0.25*Math.sin(6*Math.PI*u) });
  const dcurve = (u) => ({ x: 6, y: 2*Math.PI*0.9*Math.cos(2*Math.PI*u) + 6*Math.PI*0.25*Math.cos(6*Math.PI*u) });
  const samples = Array.from({length: 180},(_,i)=>i/179);
  const length = samples.slice(1).reduce((s,u,i)=>{
    const du = 1/179;
    const d = dcurve(u-du/2);
    return s + Math.hypot(d.x,d.y)*du;
  },0);
  const p = curve(t), d = dcurve(t);
  return <ScreenShell
    left={<Panel title="arc-length bridge" right="Bezier-first future path"><div className="h-[500px]"><svg viewBox="0 0 760 500" className="w-full h-full"><rect width="760" height="500" fill={T.bg1}/><path d={samples.map((u,i)=>{const p=curve(u);return `${i===0?'M':'L'}${80+(p.x+3)/6*600},${250-p.y*120}`}).join(' ')} fill="none" stroke={T.text0} strokeWidth="2.5"/><circle cx={80+(p.x+3)/6*600} cy={250-p.y*120} r="7" fill={T.amber}/><line x1={80+(p.x+3)/6*600} y1={250-p.y*120} x2={80+(p.x+3)/6*600+d.x*7} y2={250-p.y*120-d.y*7} stroke={T.green} strokeWidth="2"/><text x="40" y="40" fill={T.text2} fontFamily="monospace" fontSize="13">arc length = ∫ ||γ'(u)|| du</text><text x="40" y="62" fill={T.text2} fontFamily="monospace" fontSize="13">particle uses u = s⁻¹(s)</text></svg></div></Panel>}
    right={<>
      <Panel title="parametric curve stub"><div className="p-3 space-y-2"><KV k="curve" v="γ(u) = (x(u), y(u))" color={T.amber2}/><KV k="u" v={fmt(t,4)}/><input type="range" min="0" max="1" step="0.001" value={t} onChange={e=>setT(+e.target.value)} style={{ accentColor: T.amber, width: "100%" }}/><KV k="γ(u)" v={`(${fmt(p.x,4)}, ${fmt(p.y,4)})`}/><KV k="||γ'(u)||" v={fmt(Math.hypot(d.x,d.y),6)} color={T.green}/><KV k="estimated length" v={fmt(length,8)} color={T.amber2}/></div></Panel>
      <Panel title="MVP bridge rule" accent={T.purple}><div className="p-3 text-xs leading-relaxed" style={{ color: T.text1 }}>Start with Bezier curve differential queries before B-splines/NURBS. Arc length should be computed by the same 1D integration kernel used in the Workbench.</div></Panel>
      <Connections title="Arc length" used={["constant-speed motion", "Bezier curvature preview", "NURBS surface area later"]} later={["Frenet/Bishop frames", "sweep/tube geometry", "manifold geodesics"]}/>
    </>}
  />;
}

function Connections({ title, used, later }) {
  return <Panel title="connections" accent={T.purple} right={title}>
    <div className="p-3 space-y-3 text-xs">
      <div style={{ color: T.text1 }}>This panel keeps the workspace from becoming a calculator with pictures.</div>
      <div><div className="font-mono uppercase tracking-wider text-[9px] mb-1" style={{ color: T.text2 }}>used by</div>{used.map(u=><div key={u} className="font-mono" style={{ color: T.blue }}>→ {u}</div>)}</div>
      <div><div className="font-mono uppercase tracking-wider text-[9px] mb-1" style={{ color: T.text2 }}>later</div>{later.map(u=><div key={u} className="font-mono" style={{ color: T.purple }}>⟶ {u}</div>)}</div>
    </div>
  </Panel>;
}

function MiniBars({ values, color }) {
  const max = Math.max(...values, 1);
  return <div className="p-4 h-48 flex items-end gap-2" style={{ background: T.bg1 }}>
    {values.map((v,i)=><div key={i} className="flex-1 rounded-t" title={`${v}`} style={{ height: `${8 + (v/max)*150}px`, background: color, opacity: 0.45 + 0.5*(v/max) }} />)}
  </div>;
}

function ScreenShell({ left, right }) {
  return <div className="h-full flex overflow-hidden" style={{ background: T.bg0 }}>
    <div className="w-[62%] min-w-[620px] p-3 overflow-auto" style={{ borderRight: `1px solid ${T.border}` }}>{left}</div>
    <div className="flex-1 p-3 overflow-auto space-y-3">{right}</div>
  </div>;
}

function StatusBar({ state, screen }) {
  return <div className="h-8 px-3 flex items-center gap-5 shrink-0 text-[10px] font-mono" style={{ background: T.bg0, color: T.text2, borderTop: `1px solid ${T.border}` }}>
    <span>screen <b style={{ color: T.amber }}>{screen}</b></span>
    <span>estimate <b style={{ color: T.amber2 }}>{fmt(state.estimate,6)}</b></span>
    <span>error <b style={{ color: state.error < 1e-3 ? T.green : T.amber }}>{state.error.toExponential(2)}</b></span>
    <span>cells <b style={{ color: T.text1 }}>{state.cells.length}</b></span>
    <span>flags <b style={{ color: state.functionObj.flags.length ? T.red : T.green }}>{state.functionObj.flags.length ? state.functionObj.flags.join(", ") : "none"}</b></span>
  </div>;
}

export default function IntegrationObservatoryPrototype() {
  const [screen, setScreen] = useState("workbench");
  const [functionId, setFunctionId] = useState("poly");
  const [method, setMethod] = useState("midpoint");
  const [n, setN] = useState(16);
  const [a, setA] = useState(-2);
  const [b, setB] = useState(2);
  const [selectedCell, setSelectedCell] = useState(null);
  const functionObj = FUNCTIONS[functionId];
  const f = functionObj.f;
  const cells = useMemo(() => makePartition(a,b,n,f), [a,b,n,f]);
  const estimate = useMemo(() => estimateByMethod(cells,f,method), [cells,f,method]);
  const exact = functionObj.exact(a,b);
  const error = Math.abs(estimate-exact);
  const state = { f, cells, method, selectedCell, setSelectedCell, estimate, exact, error, functionObj, a, b, n };
  return <div className="w-full h-screen flex flex-col" style={{ background: T.bg0, color: T.text0 }}>
    <style>{`*{box-sizing:border-box} select, input, button{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace} ::-webkit-scrollbar{width:8px;height:8px} ::-webkit-scrollbar-thumb{background:${T.border2};border-radius:10px} ::-webkit-scrollbar-track{background:${T.bg0}}`}</style>
    <GlobalToolbar functionId={functionId} setFunctionId={(id)=>{setFunctionId(id); setSelectedCell(null);}} method={method} setMethod={setMethod} n={n} setN={setN} a={a} setA={setA} b={b} setB={setB} screen={screen} setScreen={setScreen} />
    <div className="flex-1 overflow-hidden">
      {screen === "workbench" && <Workbench state={state} />}
      {screen === "darboux" && <Darboux state={state} />}
      {screen === "analysis" && <Analysis state={state} />}
      {screen === "twoD" && <TwoD />}
      {screen === "arc" && <ArcLength />}
    </div>
    <StatusBar state={state} screen={screens.find(s=>s[0]===screen)?.[1]} />
  </div>;
}
