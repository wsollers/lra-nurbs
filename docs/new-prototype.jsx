import React, { useState, useMemo, useRef } from "react";

// ─────────────────────────────────────────────────────────────────────────────
// DESIGN TOKENS & SYSTEM THEME
// ─────────────────────────────────────────────────────────────────────────────
const T = {
  bg0:      "#08090a",   // Deep obsidian environment
  bg1:      "#0f1115",   // Panel background
  bg2:      "#16191e",   // Table header / surface elevation
  bg3:      "#21262e",   // Interactive hover / active state
  border:   "#262c35",   // Structural grid line / hairline split
  border2:  "#3b4453",   // High-contrast boundary edge
  amber:    "#f59e0b",   // Upper Bounds / Master Accent
  emerald:  "#10b981",   // Lower Bounds / Convergence Signal
  blue:     "#3b82f6",   // Riemann Samples / Interpolations
  purple:   "#8b5cf6",   // Schematic Connectors / Deep Theory
  rose:     "#f43f5e",   // Discontinuity / Singularities / Errors
  text0:    "#f3f4f6",   // High-priority LaTeX or literal readouts
  text1:    "#9ca3af",   // Secondary definitions
  text2:    "#4b5563",   // Inactive states / structural tags
  mono:     "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
  sans:     "system-ui, -apple-system, sans-serif"
};

// ─────────────────────────────────────────────────────────────────────────────
// THE INTEGRATION ZOO (Analytical & Numerical Definitions)
// ─────────────────────────────────────────────────────────────────────────────
const INTEGRANDS = {
  smooth: {
    id: "smooth",
    name: "Runge Cusp / Cauchy Spike",
    expr: "f(x) = 1 / (1 + 25x²)",
    f: (x) => 1.0 / (1.0 + 25.0 * x * x),
    exact: (a, b) => (Math.atan(5.0 * b) - Math.atan(5.0 * a)) / 5.0,
    guardFlags: ["HIGH_LOCAL_VARIATION"],
    description: "Smooth everywhere, but provides a sharp central spike that challenges uniform partitions."
  },
  cusp: {
    id: "cusp",
    name: "Absolute Cusp",
    expr: "f(x) = |x| + 0.2",
    f: (x) => Math.abs(x) + 0.2,
    exact: (a, b) => {
      const F = (v) => v >= 0 ? (0.5 * v * v + 0.2 * v) : (-0.5 * v * v + 0.2 * v);
      return F(b) - F(a);
    },
    guardFlags: ["NON_DIFFERENTIABLE_CUSP"],
    description: "Continuous everywhere, but suffers a non-differentiable cusp at x = 0, causing asymmetric errors."
  },
  discontinuous: {
    id: "discontinuous",
    name: "Heaviside Jump Step",
    expr: "f(x) = x < 0 ? 0.3 : 1.4",
    f: (x) => x < 0 ? 0.3 : 1.4,
    exact: (a, b) => {
      let sum = 0;
      if (a < 0) sum += 0.3 * (Math.min(0, b) - a);
      if (b > 0) sum += 1.4 * (b - Math.max(0, a));
      return sum;
    },
    guardFlags: ["JUMP_DISCONTINUITY"],
    description: "Bounded function with a localized jump discontinuity. Riemann integrable, but limits convergence rates."
  }
};

const TWO_D_FUNCTIONS = {
  gaussian: {
    name: "exp(−(x²+y²))",
    f: (x, y) => Math.exp(-(x * x + y * y))
  },
  interference: {
    name: "sin(2x)cos(2y) + 1.1",
    f: (x, y) => Math.sin(2 * x) * Math.cos(2 * y) + 1.1
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// REUSABLE PRESENTATION LAYOUT COMPONENTS
// ─────────────────────────────────────────────────────────────────────────────
function Badge({ color, children }) {
  return (
    <span style={{
      padding: "2px 6px",
      borderRadius: "3px",
      fontSize: "10px",
      fontFamily: T.mono,
      fontWeight: 600,
      textTransform: "uppercase",
      letterSpacing: "0.05em",
      color: color,
      background: color + "15",
      border: `1px solid ${color}35`
    }}>
      {children}
    </span>
  );
}

function SectionPanel({ title, rightContent, accentColor = T.amber, children }) {
  return (
    <div style={{
      background: T.bg1,
      border: `1px solid ${T.border}`,
      borderRadius: "6px",
      overflow: "hidden",
      display: "flex",
      flexDirection: "column"
    }}>
      <div style={{
        height: "36px",
        padding: "0 12px",
        background: T.bg2,
        borderBottom: `1px solid ${T.border}`,
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between"
      }}>
        <div style={{
          fontSize: "11px",
          fontFamily: T.mono,
          fontWeight: 700,
          textTransform: "uppercase",
          letterSpacing: "0.1em",
          color: accentColor
        }}>
          {title}
        </div>
        {rightContent && <div style={{ fontSize: "11px", fontFamily: T.mono, color: T.text2 }}>{rightContent}</div>}
      </div>
      <div style={{ flex: 1, position: "relative" }}>{children}</div>
    </div>
  );
}

function ReadoutRow({ label, value, valueColor = T.text0 }) {
  return (
    <div style={{
      display: "flex",
      alignItems: "center",
      justifyContent: "space-between",
      padding: "6px 8px",
      background: T.bg0,
      border: `1px solid ${T.border}`,
      borderRadius: "4px"
    }}>
      <span style={{ fontSize: "10px", fontFamily: T.mono, uppercase: true, color: T.text2, letterSpacing: "0.04em" }}>{label}</span>
      <span style={{ fontSize: "12px", fontFamily: T.mono, color: valueColor, fontWeight: 500 }}>{value}</span>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// MASTER APPARATUS COMPONENT
// ─────────────────────────────────────────────────────────────────────────────
export default function IntegrationAnalysisWorkbench() {
  // Navigation & Primary Probes
  const [activeTab, setActiveTab] = useState("darboux"); // darboux | fubini2d | audits
  const [integrandId, setIntegrandId] = useState("smooth");
  const [partitionN, setPartitionN] = useState(16);
  const [domainA, setDomainA] = useState(-2.0);
  const [domainB, setDomainB] = useState(2.0);
  
  // Selection States for Inspection
  const [selectedCellIdx, setSelectedCellIdx] = useState(0);
  const [twoDMode, setTwoDMode] = useState("gaussian");
  const [twoDResolution, setTwoDResolution] = useState(12);
  const [hovered2DCell, setHovered2DCell] = useState(null);

  const activeIntegrand = INTEGRANDS[integrandId];

  // ───────────────────────────────────────────────────────────────────────────
  // CALCULATION ENGINE: 1D PARTITION STRUCTURE
  // ───────────────────────────────────────────────────────────────────────────
  const partitionData = useMemo(() => {
    const a = domainA;
    const b = domainB;
    const n = partitionN;
    const f = activeIntegrand.f;
    const dx = (b - a) / n;
    
    let totalLowerSum = 0;
    let totalUpperSum = 0;
    let totalMidpointSum = 0;
    
    const cells = Array.from({ length: n }, (_, i) => {
      const x0 = a + i * dx;
      const x1 = x0 + dx;
      const mid = x0 + 0.5 * dx;
      
      // Fine local sampling to extract infimum & supremum envelopes
      const samples = Array.from({ length: 15 }, (_, k) => x0 + (k / 14) * dx);
      const sampleVals = samples.map(f);
      const localInf = Math.min(...sampleVals);
      const localSup = Math.max(...sampleVals);
      const oscillation = localSup - localInf;
      
      totalLowerSum += localInf * dx;
      totalUpperSum += localSup * dx;
      totalMidpointSum += f(mid) * dx;
      
      return {
        idx: i,
        x0,
        x1,
        mid,
        dx,
        inf: localInf,
        sup: localSup,
        oscillation,
        midVal: f(mid),
        lowerContribution: localInf * dx,
        upperContribution: localSup * dx
      };
    });

    const exactAnalytical = activeIntegrand.exact(a, b);
    
    return {
      cells,
      meshSize: dx,
      lowerSum: totalLowerSum,
      upperSum: totalUpperSum,
      midpointSum: totalMidpointSum,
      darbouxGap: totalUpperSum - totalLowerSum,
      exact: exactAnalytical,
      midpointError: Math.abs(totalMidpointSum - exactAnalytical)
    };
  }, [activeIntegrand, partitionN, domainA, domainB]);

  // Ensure index boundary safety
  const activeCell = partitionData.cells[selectedCellIdx] || partitionData.cells[0] || {
    idx: 0, x0: 0, x1: 0, mid: 0, dx: 1, inf: 0, sup: 0, oscillation: 0, midVal: 0, lowerContribution: 0, upperContribution: 0
  };

  // ───────────────────────────────────────────────────────────────────────────
  // CALCULATION ENGINE: MULTI-PARTITION REFINEMENT STUDY (AUDITS)
  // ───────────────────────────────────────────────────────────────────────────
  const convergenceSuite = useMemo(() => {
    const steps = [4, 8, 12, 16, 24, 32, 48, 64];
    const f = activeIntegrand.f;
    const exact = activeIntegrand.exact(domainA, domainB);
    
    return steps.map(N => {
      const dx = (domainB - domainA) / N;
      let lSum = 0, uSum = 0, mSum = 0;
      
      for (let i = 0; i < N; i++) {
        const x0 = domainA + i * dx;
        const x1 = x0 + dx;
        const mid = x0 + 0.5 * dx;
        
        const subSamples = Array.from({ length: 10 }, (_, k) => x0 + (k / 9) * dx).map(f);
        lSum += Math.min(...subSamples) * dx;
        uSum += Math.max(...subSamples) * dx;
        mSum += f(mid) * dx;
      }
      
      return {
        N,
        h: dx,
        midpointError: Math.max(Math.abs(mSum - exact), 1e-15),
        darbouxGap: Math.max(uSum - lSum, 1e-15)
      };
    });
  }, [activeIntegrand, domainA, domainB]);

  // Compute observed order of convergence alpha from last two steps
  const empiricalConvergenceOrder = useMemo(() => {
    if (convergenceSuite.length < 2) return "0.00";
    const p1 = convergenceSuite[convergenceSuite.length - 2];
    const p2 = convergenceSuite[convergenceSuite.length - 1];
    const logErrorRatio = Math.log(p2.midpointError / p1.midpointError);
    const logMeshRatio = Math.log(p2.h / p1.h);
    return logMeshRatio !== 0 ? (logErrorRatio / logMeshRatio).toFixed(2) : "0.00";
  }, [convergenceSuite]);

  // ───────────────────────────────────────────────────────────────────────────
  // CALCULATION ENGINE: 2D FUBINI PRODUCT MESH
  // ───────────────────────────────────────────────────────────────────────────
  const fubiniMesh2D = useMemo(() => {
    const res = twoDResolution;
    const minCoord = -2.0;
    const maxCoord = 2.0;
    const stepSize = (maxCoord - minCoord) / res;
    const f2d = TWO_D_FUNCTIONS[twoDMode].f;
    
    let accumulatedVolume = 0;
    const grid = [];
    
    for (let i = 0; i < res; i++) {
      for (let j = 0; j < res; j++) {
        const cx = minCoord + (i + 0.5) * stepSize;
        const cy = minCoord + (j + 0.5) * stepSize;
        const intensity = f2d(cx, cy);
        const cellVol = intensity * stepSize * stepSize;
        accumulatedVolume += cellVol;
        
        grid.push({ i, j, cx, cy, val: intensity, vol: cellVol });
      }
    }
    
    return { grid, volume: accumulatedVolume, stepSize, resolution: res };
  }, [twoDResolution, twoDMode]);

  return (
    <div style={{
      width: "100%",
      height: "100vh",
      background: T.bg0,
      color: T.text0,
      fontFamily: T.sans,
      display: "flex",
      flexDirection: "column",
      overflow: "hidden"
    }}>
      
      {/* GLOBAL MANAGEMENT HEADER BAR */}
      <header style={{
        height: "52px",
        background: T.bg1,
        borderBottom: `1px solid ${T.border}`,
        display: "flex",
        alignItems: "center",
        padding: "0 16px",
        gap: "20px",
        zIndex: 10
      }}>
        {/* Core Project Identity */}
        <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
          <div style={{
            width: "26px",
            height: "26px",
            background: T.amber,
            color: T.bg0,
            borderRadius: "4px",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            fontFamily: T.mono,
            fontWeight: 900,
            fontSize: "14px"
          }}>
            ∫
          </div>
          <div>
            <div style={{ fontSize: "11px", fontFamily: T.mono, fontWeight: 700, color: T.text0 }}>nurbs_dde</div>
            <div style={{ fontSize: "8px", fontFamily: T.mono, color: T.text2, letterSpacing: "0.1em", textTransform: "uppercase" }}>Analysis Core</div>
          </div>
        </div>

        <div style={{ width: "1px", height: "24px", background: T.border }} />

        {/* Global Control Inputs */}
        <div style={{ display: "flex", alignItems: "center", gap: "16px", flex: 1 }}>
          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            <span style={{ fontSize: "9px", fontFamily: T.mono, color: T.text2, textTransform: "uppercase" }}>Integrand</span>
            <select 
              value={integrandId} 
              onChange={(e) => { setIntegrandId(e.target.value); setSelectedCellIdx(0); }}
              style={{
                background: T.bg0,
                color: T.amber,
                border: `1px solid ${T.border2}`,
                borderRadius: "4px",
                padding: "3px 8px",
                fontSize: "11px",
                fontFamily: T.mono,
                outline: "none"
              }}
            >
              {Object.entries(INTEG_ZOO).map(([id, item]) => (
                <option key={id} value={id}>{item.name}</option>
              ))}
            </select>
          </div>

          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            <span style={{ fontSize: "9px", fontFamily: T.mono, color: T.text2, textTransform: "uppercase" }}>Partition N</span>
            <input 
              type="range" 
              min={4} 
              max={64} 
              value={partitionN}
              onChange={(e) => {
                const val = parseInt(e.target.value, 10);
                setPartitionN(val);
                if (selectedCellIdx >= val) setSelectedCellIdx(val - 1);
              }}
              style={{ width: "110px", accentColor: T.amber }}
            />
            <span style={{ fontSize: "11px", fontFamily: T.mono, color: T.amber, width: "20px" }}>{partitionN}</span>
          </div>

          <div style={{ display: "flex", alignItems: "center", gap: "6px" }}>
            <span style={{ fontSize: "9px", fontFamily: T.mono, color: T.text2, textTransform: "uppercase" }}>Bounds</span>
            <input 
              type="number" 
              value={domainA} 
              step={0.5}
              onChange={(e) => setDomainA(parseFloat(e.target.value))}
              style={{ width: "50px", background: T.bg0, border: `1px solid ${T.border}`, borderRadius: "4px", padding: "2px 4px", fontSize: "11px", fontFamily: T.mono, color: T.text1, textAlign: "center" }}
            />
            <span style={{ fontSize: "10px", fontFamily: T.mono, color: T.text2 }}>≤ x ≤</span>
            <input 
              type="number" 
              value={domainB} 
              step={0.5}
              onChange={(e) => setDomainB(parseFloat(e.target.value))}
              style={{ width: "50px", background: T.bg0, border: `1px solid ${T.border}`, borderRadius: "4px", padding: "2px 4px", fontSize: "11px", fontFamily: T.mono, color: T.text1, textAlign: "center" }}
            />
          </div>
        </div>

        {/* Global Sub-System Screen Toggles */}
        <div style={{ display: "flex", gap: "2px", background: T.bg0, padding: "2px", borderRadius: "4px", border: `1px solid ${T.border}` }}>
          {[
            { id: "darboux", label: "Darboux 1D" },
            { id: "fubini2d", label: "Fubini 2D" },
            { id: "audits", label: "Refinement Audits" }
          ].map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              style={{
                padding: "4px 12px",
                border: "none",
                borderRadius: "3px",
                fontSize: "10px",
                fontFamily: T.mono,
                fontWeight: 600,
                textTransform: "uppercase",
                cursor: "pointer",
                background: activeTab === tab.id ? T.bg3 : "transparent",
                color: activeTab === tab.id ? T.amber : T.text1,
                transition: "all 0.15s ease"
              }}
            >
              {tab.label}
            </button>
          ))}
        </div>
      </header>

      {/* CORE INSTRUMENT WORKSPACE CONTAINER */}
      <main style={{ flex: 1, display: "flex", overflow: "hidden", padding: "12px", gap: "12px" }}>
        
        {/* VIEWPORT AREA CONTROLLER */}
        <div style={{ flex: 7, display: "flex", flexDirection: "column" }}>
          {activeTab === "darboux" && (
            <SectionPanel 
              title="Geometric Darboux Riemann Mesh Viewport" 
              rightContent={`Mesh Δx = ${partitionData.meshSize.toFixed(5)}`}
              accentColor={T.blue}
            >
              <div style={{ width: "100%", height: "100%", minHeight: "440px", padding: "16px", background: T.bg0 }}>
                {/* 1D Partition SVG Canvas Engine */}
                <DarbouxPartitionCanvas 
                  cells={partitionData.cells}
                  f={activeIntegrand.f}
                  domainA={domainA}
                  domainB={domainB}
                  selectedCellIdx={selectedCellIdx}
                  onSelectCellIdx={setSelectedCellIdx}
                />
              </div>
            </SectionPanel>
          )}

          {activeTab === "fubini2d" && (
            <SectionPanel 
              title="2D Fubini Tensor Product Mesh Canvas" 
              rightContent={`Grid: ${twoDResolution}² Cells`}
              accentColor={T.purple}
            >
              <div style={{ width: "100%", height: "100%", minHeight: "440px", display: "flex", background: T.bg0, padding: "16px" }}>
                {/* 2D Canvas Matrix Left Column */}
                <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: "12px" }}>
                  <div style={{ display: "flex", gap: "12px", background: T.bg1, padding: "8px 12px", borderRadius: "4px", border: `1px solid ${T.border}` }}>
                    <select
                      value={twoDMode}
                      onChange={(e) => setTwoDMode(e.target.value)}
                      style={{ background: T.bg0, color: T.purple, border: `1px solid ${T.border2}`, padding: "2px 6px", fontFamily: T.mono, fontSize: "11px", borderRadius: "4px" }}
                    >
                      {Object.entries(TWO_D_FUNCTIONS).map(([k, v]) => (
                        <option key={k} value={k}>{v.name}</option>
                      ))}
                    </select>
                    <input 
                      type="range" min={4} max={24} value={twoDResolution} 
                      onChange={(e) => setTwoDResolution(parseInt(e.target.value, 10))} 
                      style={{ accentColor: T.purple }}
                    />
                  </div>
                  <div style={{ flex: 1, position: "relative" }}>
                    <FubiniGridCanvas data={fubiniMesh2D} hoveredCell={hovered2DCell} onHoverCell={setHovered2DCell} />
                  </div>
                </div>
              </div>
            </SectionPanel>
          )}

          {activeTab === "audits" && (
            <SectionPanel title="Numerical Audit & Verification Trail" rightContent="Partition Convergence Log" accentColor={T.emerald}>
              <div style={{ width: "100%", height: "100%", minHeight: "440px", background: T.bg0, padding: "16px", overflowY: "auto" }}>
                <table style={{ width: "100%", borderCollapse: "collapse", fontFamily: T.mono, fontSize: "11px" }}>
                  <thead>
                    <tr style={{ background: T.bg2, borderBottom: `1px solid ${T.border2}`, color: T.text1 }}>
                      <th style={{ padding: "8px", textAlign: "left" }}>Partition Size (N)</th>
                      <th style={{ padding: "8px", textAlign: "right" }}>Step Width (h)</th>
                      <th style={{ padding: "8px", textAlign: "right" }}>Darboux Gap (U - L)</th>
                      <th style={{ padding: "8px", textAlign: "right" }}>Midpoint Absolute Error</th>
                      <th style={{ padding: "8px", textAlign: "right" }}>Empirical Order</th>
                    </tr>
                  </thead>
                  <tbody>
                    {convergenceSuite.map((row, idx) => {
                      const prev = idx > 0 ? convergenceSuite[idx - 1] : null;
                      const alpha = prev ? (Math.log(row.midpointError / prev.midpointError) / Math.log(row.h / prev.h)).toFixed(2) : "—";
                      return (
                        <tr key={row.N} style={{ borderBottom: `1px solid ${T.border}`, background: idx % 2 === 0 ? "transparent" : T.bg1 }}>
                          <td style={{ padding: "8px", color: T.amber, fontWeight: "bold" }}>{row.N} cells</td>
                          <td style={{ padding: "8px", textAlign: "right", color: T.text1 }}>{row.h.toFixed(5)}</td>
                          <td style={{ padding: "8px", textAlign: "right", color: T.amber }}>{row.darbouxGap.toExponential(4)}</td>
                          <td style={{ padding: "8px", textAlign: "right", color: T.rose }}>{row.midpointError.toExponential(4)}</td>
                          <td style={{ padding: "8px", textAlign: "right", color: T.emerald, fontWeight: "bold" }}>{alpha}</td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>
            </SectionPanel>
          )}
        </div>

        {/* METRICS SIDE PANEL (ANALYSIS ENGINE VIEWPORTS) */}
        <div style={{ flex: 4, display: "flex", flexDirection: "column", gap: "12px", overflowY: "auto" }}>
          
          {/* SCHEMA SUB-SYSTEM: STATEMENT PANELS */}
          <SectionPanel title="Integrand Equation Specification" accentColor={T.text1}>
            <div style={{ padding: "10px", display: "flex", flexDirection: "column", gap: "6px" }}>
              <div style={{ fontSize: "14px", fontFamily: T.mono, color: T.amber, fontWeight: "bold", padding: "4px 0" }}>
                {activeIntegrand.expr}
              </div>
              <div style={{ fontSize: "11px", color: T.text1, lineHeight: 1.4 }}>
                {activeIntegrand.description}
              </div>
              <div style={{ display: "flex", gap: "4px", marginTop: "4px", flexWrap: "wrap" }}>
                {activeIntegrand.guardFlags.map(f => (
                  <Badge key={f} color={T.rose}>{f}</Badge>
                ))}
                <Badge color={T.purple}>Riemann Integrable</Badge>
              </div>
            </div>
          </SectionPanel>

          {/* DYNAMIC SCHEMA SUB-SYSTEM PANEL */}
          {activeTab === "darboux" && (
            <>
              <SectionPanel title="Active Global Darboux Schema" accentColor={T.emerald}>
                <div style={{ padding: "10px", display: "flex", flexDirection: "column", gap: "8px" }}>
                  <ReadoutRow label="Lower Darboux Sum L(f, P)" value={partitionData.lowerSum.toFixed(8)} valueColor={T.emerald} />
                  <ReadoutRow label="Upper Darboux Sum U(f, P)" value={partitionData.upperSum.toFixed(8)} valueColor={T.amber} />
                  <ReadoutRow label="Integrability Gap (U - L)" value={partitionData.darbouxGap.toExponential(6)} valueColor={partitionData.darbouxGap < 0.05 ? T.emerald : T.rose} />
                  <ReadoutRow label="Analytical Integration Exact" value={partitionData.exact.toFixed(8)} />
                  <ReadoutRow label="Midpoint Error Bound" value={partitionData.midpointError.toExponential(6)} valueColor={T.blue} />
                </div>
              </SectionPanel>

              <SectionPanel title="Microscopic Sub-Interval Inspection" rightContent={`Cell Index #${activeCell.idx}`} accentColor={T.blue}>
                <div style={{ padding: "10px", display: "flex", flexDirection: "column", gap: "6px" }}>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "6px" }}>
                    <ReadoutRow label="Bound x0" value={activeCell.x0.toFixed(4)} />
                    <ReadoutRow label="Bound x1" value={activeCell.x1.toFixed(4)} />
                  </div>
                  <ReadoutRow label="Infimum (m_i)" value={activeCell.inf.toFixed(6)} valueColor={T.emerald} />
                  <ReadoutRow label="Supremum (M_i)" value={activeCell.sup.toFixed(6)} valueColor={T.amber} />
                  <ReadoutRow label="Oscillation (ω_i)" value={activeCell.oscillation.toFixed(6)} valueColor={T.rose} />
                  <ReadoutRow label="Cell Midpoint Metric" value={activeCell.midVal.toFixed(6)} valueColor={T.blue} />
                </div>
              </SectionPanel>
            </>
          )}

          {activeTab === "fubini2d" && (
            <SectionPanel title="Active 2D Product Space Schema" accentColor={T.purple}>
              <div style={{ padding: "10px", display: "flex", flexDirection: "column", gap: "8px" }}>
                <ReadoutRow label="Double Riemann Sum ∬f dA" value={fubiniMesh2D.volume.toFixed(8)} valueColor={T.purple} />
                <ReadoutRow label="Domain Area Bounds" value="[-2, 2] × [-2, 2]" />
                <ReadoutRow label="Total Tensor Products" value={`${fubiniMesh2D.grid.length} area elements`} />
                <ReadoutRow label="Sub-Element Area dA" value={(fubiniMesh2D.stepSize * fubiniMesh2D.stepSize).toFixed(6)} />
                
                <div style={{ height: "1px", background: T.border, margin: "4px 0" }} />
                
                <div style={{ fontSize: "9px", fontFamily: T.mono, textTransform: "uppercase", color: T.text2 }}>Hover Inspector Readout</div>
                {hovered2DCell ? (
                  <div style={{ padding: "6px", background: T.bg0, border: `1px solid ${T.border}`, borderRadius: "4px", display: "flex", flexDirection: "column", gap: "4px" }}>
                    <div style={{ fontSize: "11px", fontFamily: T.mono, color: T.text0 }}>Coordinates: ({hovered2DCell.cx.toFixed(3)}, {hovered2DCell.cy.toFixed(3)})</div>
                    <div style={{ fontSize: "11px", fontFamily: T.mono, color: T.purple }}>Evaluated Value: {hovered2DCell.val.toFixed(6)}</div>
                    <div style={{ fontSize: "11px", fontFamily: T.mono, color: T.emerald }}>Volume Element Contribution: {hovered2DCell.vol.toFixed(6)}</div>
                  </div>
                ) : (
                  <div style={{ fontSize: "11px", fontFamily: T.mono, color: T.text2, fontStyle: "italic" }}>Hover cursor over surface mesh element cells to inspect...</div>
                )}
              </div>
            </SectionPanel>
          )}

          {activeTab === "audits" && (
            <SectionPanel title="Control-Tolerance Schema Verification" accentColor={T.amber}>
              <div style={{ padding: "10px", display: "flex", flexDirection: "column", gap: "8px" }}>
                <ReadoutRow label="Active Target Tolerance (ε)" value="1.00e-3" valueColor={T.emerald} />
                <ReadoutRow label="Current Max Mesh (δ)" value={partitionData.meshSize.toFixed(5)} valueColor={T.blue} />
                <ReadoutRow label="Empirical Convergence Rate (α)" value={empiricalConvergenceOrder} valueColor={T.amber} />
                <div style={{ fontSize: "10px", fontFamily: T.mono, color: T.text1, lineHeight: "1.4", borderTop: `1px solid ${T.border}`, paddingTop: "8px", marginTop: "4px" }}>
                  <span style={{ color: T.purple, fontWeight: "bold" }}>Asymptotic Guard Signal: </span>
                  An empirical order $\alpha \approx 2.00$ implies a quadratic convergence pattern ($O(h^2)$), typical for a clean Midpoint rule across smooth manifolds. Drastic drops indicate non-differentiable singularity boundaries or jump steps crossing grid lines.
                </div>
              </div>
            </SectionPanel>
          )}

          {/* ARCHITECTURAL THEORY LINK TRACE */}
          <SectionPanel title="Theorem Foundations Dependency Network" accentColor={T.purple}>
            <div style={{ padding: "10px", fontSize: "11px", fontFamily: T.mono, display: "flex", flexDirection: "column", gap: "6px" }}>
              <div style={{ color: T.text1 }}><span style={{ color: T.purple }}>Current Target:</span> Smooth metric locally matching polynomial steps.</div>
              <div style={{ background: T.bg0, padding: "6px", border: `1px solid ${T.border}`, borderRadius: "4px", color: T.text2 }}>
                $\forall \epsilon &gt; 0, \exists P \text{ s.t. } U(f,P) - L(f,P) &lt; \epsilon$
              </div>
              <div style={{ fontSize: "9px", textTransform: "uppercase", color: T.text2, letterSpacing: "0.05em", marginTop: "4px" }}>Downstream Pipelines</div>
              <div style={{ color: T.blue }}>→ Fundamental Theorem of Calculus Verification</div>
              <div style={{ color: T.blue }}>→ Differential Form Geodesics & Arc Length Measures</div>
              <div style={{ color: T.purple }}>⟶ Lebesgue Dominated Convergence Space Transformation</div>
            </div>
          </SectionPanel>

        </div>
      </main>

      {/* FOOTER METRICS DIAGNOSTIC STATUS BAR */}
      <footer style={{
        height: "28px",
        background: T.bg1,
        borderTop: `1px solid ${T.border}`,
        display: "flex",
        alignItems: "center",
        padding: "0 12px",
        fontSize: "10px",
        fontFamily: T.mono,
        color: T.text2,
        justifyContent: "space-between"
      }}>
        <div style={{ display: "flex", gap: "16px" }}>
          <span>Integrand Token: <b style={{ color: T.text1 }}>{integrandId}</b></span>
          <span>Mesh Status: <b style={{ color: T.blue }}>δ = {partitionData.meshSize.toFixed(4)}</b></span>
          <span>Darboux Integrability: <b style={{ color: partitionData.darbouxGap < 1e-3 ? T.emerald : T.amber }}>{partitionData.darbouxGap < 1e-3 ? "VERIFIED" : "REFINING"}</b></span>
        </div>
        <div>
          <span style={{ color: T.text2 }}>Stability Monitor System: </span>
          <span style={{ color: T.emerald, fontWeight: "bold" }}>STABLE // PASSING</span>
        </div>
      </footer>

    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// DATA GRAPH INTERACTION SUB-ENGINE (1D CANVASES)
// ─────────────────────────────────────────────────────────────────────────────
function DarbouxPartitionCanvas({ cells, f, domainA, domainB, selectedCellIdx, onSelectCellIdx }) {
  const width = 680;
  const height = 400;
  const pad = 40;

  // Track function extreme envelopes over entire scale for rendering mapping
  const graphPtsCount = 200;
  const densePts = Array.from({ length: graphPtsCount }, (_, i) => domainA + (i / (graphPtsCount - 1)) * (domainB - domainA));
  const denseVals = densePts.map(f);
  const maxF = Math.max(...denseVals, 1.2);
  const minF = Math.min(...denseVals, -0.2);

  // Scaling Projections
  const mapX = (x) => pad + ((x - domainA) / (domainB - domainA)) * (width - 2 * pad);
  const mapY = (y) => height - pad - ((y - minF) / (maxF - minF)) * (height - 2 * pad);

  // Build true continuous analytical curve vector line path string
  const curvePathD = densePts.map((x, idx) => {
    const prefix = idx === 0 ? "M" : "L";
    return `${prefix}${mapX(x)},${mapY(f(x))}`;
  }).join(" ");

  const zeroYPixel = mapY(0);

  return (
    <svg viewBox={`0 0 ${width} ${height}`} style={{ width: "100%", height: "100%", display: "block" }}>
      {/* Background Frame Surface */}
      <rect width={width} height={height} fill={T.bg0} />
      
      {/* Grid Coordinates Lines */}
      {Array.from({ length: 5 }, (_, idx) => {
        const yVal = minF + (idx / 4) * (maxF - minF);
        return (
          <g key={`y-grid-${idx}`}>
            <line x1={pad} y1={mapY(yVal)} x2={width - pad} y2={mapY(yVal)} stroke={T.border} strokeWidth="0.5" />
            <text x={pad - 8} y={mapY(yVal) + 4} fill={T.text2} fontSize="9" fontFamily={T.mono} textAnchor="end">{yVal.toFixed(1)}</text>
          </g>
        );
      })}
      
      {/* Structural Zero Reference Axis Line */}
      <line x1={pad} y1={zeroYPixel} x2={width - pad} y2={zeroYPixel} stroke={T.border2} strokeWidth="1" />

      {/* ENVELOPE CELL STEP POLYGON LAYER */}
      {cells.map((cell, idx) => {
        const isSelected = selectedCellIdx === idx;
        const x0p = mapX(cell.x0);
        const x1p = mapX(cell.x1);
        const yInf = mapY(cell.inf);
        const ySup = mapY(cell.sup);
        const cellW = x1p - x0p;

        return (
          <g key={`cell-step-${idx}`} style={{ cursor: "pointer" }} onClick={() => onSelectCellIdx(idx)}>
            {/* Upper Sum Envelope Rectangle Block */}
            <rect 
              x={x0p} 
              y={ySup} 
              width={cellW} 
              height={Math.max(zeroYPixel - ySup, 2)} 
              fill={T.amber} 
              opacity={isSelected ? 0.28 : 0.12} 
              stroke={T.amber} 
              strokeWidth={isSelected ? "1.5" : "0.4"} 
            />
            {/* Lower Sum Envelope Rectangle Block */}
            <rect 
              x={x0p} 
              y={yInf} 
              width={cellW} 
              height={Math.max(zeroYPixel - yInf, 2)} 
              fill={T.emerald} 
              opacity={isSelected ? 0.32 : 0.16} 
              stroke={T.emerald} 
              strokeWidth={isSelected ? "1.5" : "0.4"} 
            />
            {/* Midpoint Rule Targeted Sample Node Dot */}
            <circle cx={mapX(cell.mid)} cy={mapY(cell.midVal)} r={isSelected ? "3.5" : "1.8"} fill={T.blue} />
          </g>
        );
      })}

      {/* CONTINUOUS TRUE MATHEMATICAL INTEGRAND CURVE LINE */}
      <path d={curvePathD} fill="none" stroke={T.text0} strokeWidth="1.8" style={{ pointerEvents: "none" }} />

      {/* TARGET SELECTED STEP BORDER HIGH LIGHTS SEPARATOR */}
      {cells[selectedCellIdx] && (
        <g style={{ pointerEvents: "none" }}>
          <line x1={mapX(cells[selectedCellIdx].x0)} y1={pad} x2={mapX(cells[selectedCellIdx].x0)} y2={height - pad} stroke={T.amber} strokeWidth="1.2" strokeDasharray="3,3" />
          <line x1={mapX(cells[selectedCellIdx].x1)} y1={pad} x2={mapX(cells[selectedCellIdx].x1)} y2={height - pad} stroke={T.amber} strokeWidth="1.2" strokeDasharray="3,3" />
        </g>
      )}

      {/* BOUNDS TEXT MARKERS AXES */}
      <text x={pad} y={height - pad + 14} fill={T.text1} fontSize="10" fontFamily={T.mono} textAnchor="middle">a = {domainA.toFixed(1)}</text>
      <text x={width - pad} y={height - pad + 14} fill={T.text1} fontSize="10" fontFamily={T.mono} textAnchor="middle">b = {domainB.toFixed(1)}</text>
    </svg>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// MULTI-VARIATE MATRIX SUB-ENGINE (2D GRID CANVASES)
// ─────────────────────────────────────────────────────────────────────────────
function FubiniGridCanvas({ data, hoveredCell, onHoverCell }) {
  const { grid, resolution } = data;
  const viewSize = 420;
  const cellSize = viewSize / resolution;

  return (
    <div style={{ position: "relative", width: `${viewSize}px`, height: `${viewSize}px`, background: T.bg1, border: `1px solid ${T.border2}`, overflow: "hidden" }}>
      <svg viewBox={`0 0 ${viewSize} ${viewSize}`} style={{ width: "100%", height: "100%", display: "block" }}>
        {grid.map((cell) => {
          // Compute normalization shading intensity factor based on local mathematical value magnitude weight
          const normalizedVal = Math.min(Math.abs(cell.val), 1.0);
          
          // Pure cool-to-warm alpha matrix calculation interpolation
          const cellColor = `rgba(139, 92, 246, ${0.1 + 0.8 * normalizedVal})`; // Purple density space
          
          const isHovered = hoveredCell && hoveredCell.i === cell.i && hoveredCell.j === cell.j;
          
          return (
            <rect
              key={`grid-cell-2d-${cell.i}-${cell.j}`}
              x={cell.i * cellSize}
              y={(resolution - 1 - cell.j) * cellSize} // Invert axis coordinates projection mapping standard orientations
              width={cellSize}
              height={cellSize}
              fill={isHovered ? T.amber : cellColor}
              opacity={isHovered ? 0.8 : 1.0}
              stroke={T.bg0}
              strokeWidth="0.5"
              onMouseEnter={() => onHoverCell(cell)}
              onMouseLeave={() => onHoverCell(null)}
              style={{ transition: "fill 0.05s ease" }}
            />
          );
        })}
      </svg>
    </div>
  );
}

// INTEGRAND ZOO DEFINITION STUB FALLBACK BACKING INTERNALS STORAGE REFERENCE OBJECT
const INTEG_ZOO = INTEGRANDS;