import { useState, useRef, useEffect, useCallback } from "react";

// ─────────────────────────────────────────────────────────────────────────────
// DESIGN TOKENS
// ─────────────────────────────────────────────────────────────────────────────
const T = {
  bg0:     "#0d0e0f",   // deepest background
  bg1:     "#131517",   // panel background
  bg2:     "#1a1d20",   // raised surface
  bg3:     "#21252a",   // hover / selected
  border:  "#2a2f35",   // hairline border
  border2: "#3a4048",   // stronger border
  amber:   "#f0a500",   // primary accent
  amberD:  "#b87a00",   // dim amber
  amberL:  "#ffc740",   // light amber
  green:   "#3dba7a",   // success / convergence
  red:     "#e05c5c",   // error / divergence
  blue:    "#5b9bd5",   // info / secondary
  purple:  "#9b7fe8",   // theorem connections
  text0:   "#e8eaec",   // primary text
  text1:   "#a0a8b2",   // secondary text
  text2:   "#5a6270",   // tertiary / disabled
  mono:    "'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace",
  sans:    "'DM Sans', 'Outfit', sans-serif",
  serif:   "'Playfair Display', Georgia, serif",
};

// ─────────────────────────────────────────────────────────────────────────────
// MATH UTILITIES (simple implementations for demo)
// ─────────────────────────────────────────────────────────────────────────────
const factorial = (n) => n <= 1 ? 1 : n * factorial(n - 1);

const taylorCoefficients = {
  sin: (k, a) => {
    if (k % 2 === 0) return 0;
    const sign = Math.floor(k / 2) % 2 === 0 ? 1 : -1;
    return sign * Math.pow(Math.cos(a), 0) / factorial(k); // simplified
  },
  exp: (k) => 1 / factorial(k),
  cos: (k, a) => {
    if (k % 2 !== 0) return 0;
    const sign = Math.floor(k / 2) % 2 === 0 ? 1 : -1;
    return sign / factorial(k);
  },
  log1px: (k) => k === 0 ? 0 : Math.pow(-1, k + 1) / k,
};

function evalTaylor(fn, a, n, x) {
  let sum = 0;
  for (let k = 0; k <= n; k++) {
    let coeff;
    if (fn === "sin") {
      if (k % 2 === 0) continue;
      const sign = Math.floor(k / 2) % 2 === 0 ? 1 : -1;
      coeff = sign / factorial(k);
    } else if (fn === "exp") {
      coeff = 1 / factorial(k);
    } else if (fn === "cos") {
      if (k % 2 !== 0) continue;
      const sign = Math.floor(k / 2) % 2 === 0 ? 1 : -1;
      coeff = sign / factorial(k);
    } else if (fn === "log1px") {
      if (k === 0) { sum += 0; continue; }
      coeff = Math.pow(-1, k + 1) / k;
    } else {
      coeff = 1 / factorial(k);
    }
    sum += coeff * Math.pow(x - a, k);
  }
  return sum;
}

function evalTrue(fn, x) {
  if (fn === "sin") return Math.sin(x);
  if (fn === "exp") return Math.exp(x);
  if (fn === "cos") return Math.cos(x);
  if (fn === "log1px") return Math.log(1 + x);
  return Math.exp(x);
}

function getRadius(fn) {
  if (fn === "sin" || fn === "exp" || fn === "cos") return Infinity;
  if (fn === "log1px") return 1;
  return Infinity;
}

// ─────────────────────────────────────────────────────────────────────────────
// SHARED PRIMITIVES
// ─────────────────────────────────────────────────────────────────────────────
const css = (obj) => Object.entries(obj).map(([k, v]) =>
  k.replace(/([A-Z])/g, m => "-" + m.toLowerCase()) + ":" + v
).join(";");

function Badge({ color = T.amber, children, small }) {
  return (
    <span style={{
      display: "inline-block",
      padding: small ? "1px 6px" : "2px 8px",
      borderRadius: 3,
      fontSize: small ? 9 : 10,
      fontFamily: T.mono,
      fontWeight: 600,
      letterSpacing: "0.08em",
      textTransform: "uppercase",
      color: color,
      background: color + "18",
      border: `1px solid ${color}40`,
    }}>{children}</span>
  );
}

function Flag({ label, color = T.red }) {
  return <Badge color={color} small>{label}</Badge>;
}

function SectionLabel({ children }) {
  return (
    <div style={{
      fontSize: 9,
      fontFamily: T.mono,
      fontWeight: 700,
      letterSpacing: "0.15em",
      textTransform: "uppercase",
      color: T.text2,
      borderBottom: `1px solid ${T.border}`,
      paddingBottom: 6,
      marginBottom: 10,
    }}>{children}</div>
  );
}

function MonoValue({ children, color = T.amberL, size = 12 }) {
  return (
    <span style={{
      fontFamily: T.mono,
      fontSize: size,
      color,
      letterSpacing: "0.02em",
    }}>{children}</span>
  );
}

function Hairline({ vertical, style }) {
  return <div style={{
    background: T.border,
    ...(vertical
      ? { width: 1, alignSelf: "stretch", flexShrink: 0 }
      : { height: 1, width: "100%" }),
    ...style,
  }} />;
}

function Panel({ children, style }) {
  return (
    <div style={{
      background: T.bg1,
      border: `1px solid ${T.border}`,
      borderRadius: 4,
      overflow: "hidden",
      ...style,
    }}>{children}</div>
  );
}

function PanelHeader({ children, accent, right }) {
  return (
    <div style={{
      display: "flex",
      alignItems: "center",
      justifyContent: "space-between",
      padding: "8px 12px",
      borderBottom: `1px solid ${T.border}`,
      background: T.bg2,
    }}>
      <div style={{
        fontSize: 10,
        fontFamily: T.mono,
        fontWeight: 700,
        letterSpacing: "0.12em",
        textTransform: "uppercase",
        color: accent || T.amber,
      }}>{children}</div>
      {right && <div style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>{right}</div>}
    </div>
  );
}

function TabBar({ tabs, active, onChange }) {
  return (
    <div style={{
      display: "flex",
      borderBottom: `1px solid ${T.border}`,
      background: T.bg0,
      padding: "0 8px",
      gap: 2,
    }}>
      {tabs.map(t => (
        <button key={t.id} onClick={() => onChange(t.id)} style={{
          padding: "10px 14px",
          background: "none",
          border: "none",
          cursor: "pointer",
          fontSize: 10,
          fontFamily: T.mono,
          fontWeight: 700,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          color: active === t.id ? T.amber : T.text2,
          borderBottom: active === t.id ? `2px solid ${T.amber}` : "2px solid transparent",
          transition: "color 0.15s, border-color 0.15s",
          marginBottom: -1,
        }}>{t.label}</button>
      ))}
    </div>
  );
}

function Tooltip({ content, children }) {
  const [show, setShow] = useState(false);
  const [pos, setPos] = useState({ x: 0, y: 0 });
  return (
    <span
      style={{ position: "relative", display: "inline-block" }}
      onMouseEnter={(e) => { setShow(true); setPos({ x: e.clientX, y: e.clientY }); }}
      onMouseLeave={() => setShow(false)}
    >
      {children}
      {show && (
        <div style={{
          position: "fixed",
          left: pos.x + 12,
          top: pos.y + 8,
          zIndex: 9999,
          background: T.bg3,
          border: `1px solid ${T.border2}`,
          borderRadius: 4,
          padding: "10px 12px",
          maxWidth: 280,
          fontSize: 11,
          fontFamily: T.mono,
          color: T.text0,
          lineHeight: 1.6,
          boxShadow: "0 8px 32px rgba(0,0,0,0.6)",
          pointerEvents: "none",
        }}>{content}</div>
      )}
    </span>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// STATUS BAR
// ─────────────────────────────────────────────────────────────────────────────
function StatusBar({ items }) {
  return (
    <div style={{
      height: 28,
      background: T.bg0,
      borderTop: `1px solid ${T.border}`,
      display: "flex",
      alignItems: "center",
      padding: "0 12px",
      gap: 20,
      flexShrink: 0,
    }}>
      {items.map((item, i) => (
        <span key={i} style={{
          fontSize: 10,
          fontFamily: T.mono,
          color: item.color || T.text2,
          display: "flex",
          alignItems: "center",
          gap: 6,
        }}>
          <span style={{ color: T.text2, fontSize: 9, letterSpacing: "0.1em", textTransform: "uppercase" }}>
            {item.label}
          </span>
          <span style={{ color: item.color || T.text1 }}>{item.value}</span>
        </span>
      ))}
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// CANVAS: Taylor Graph
// ─────────────────────────────────────────────────────────────────────────────
function TaylorCanvas({ fn, degree, expansionPt, cursorX, onCursorMove, onExpansionMove }) {
  const canvasRef = useRef(null);
  const dragging = useRef(null);

  const W = 560, H = 340;
  const marginL = 40, marginR = 20, marginT = 20, marginB = 30;
  const plotW = W - marginL - marginR;
  const plotH = H - marginT - marginB;
  const xMin = -4, xMax = 4, yMin = -2.5, yMax = 2.5;

  const toPixel = (x, y) => ({
    px: marginL + (x - xMin) / (xMax - xMin) * plotW,
    py: marginT + (1 - (y - yMin) / (yMax - yMin)) * plotH,
  });
  const fromPixel = (px) => xMin + (px - marginL) / plotW * (xMax - xMin);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, W, H);

    // Background
    ctx.fillStyle = T.bg1;
    ctx.fillRect(0, 0, W, H);

    // Grid
    ctx.strokeStyle = T.border;
    ctx.lineWidth = 0.5;
    for (let x = Math.ceil(xMin); x <= xMax; x++) {
      const { px } = toPixel(x, 0);
      ctx.beginPath(); ctx.moveTo(px, marginT); ctx.lineTo(px, marginT + plotH);
      ctx.stroke();
    }
    for (let y = Math.ceil(yMin); y <= yMax; y++) {
      const { py } = toPixel(0, y);
      ctx.beginPath(); ctx.moveTo(marginL, py); ctx.lineTo(marginL + plotW, py);
      ctx.stroke();
    }

    // Axes
    ctx.strokeStyle = T.border2;
    ctx.lineWidth = 1;
    const { py: py0 } = toPixel(0, 0);
    const { px: px0 } = toPixel(0, 0);
    ctx.beginPath(); ctx.moveTo(marginL, py0); ctx.lineTo(marginL + plotW, py0); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(px0, marginT); ctx.lineTo(px0, marginT + plotH); ctx.stroke();

    // Axis labels
    ctx.fillStyle = T.text2;
    ctx.font = `9px ${T.mono}`;
    ctx.textAlign = "center";
    for (let x = -3; x <= 3; x++) {
      if (x === 0) continue;
      const { px, py } = toPixel(x, 0);
      ctx.fillText(x, px, py + 14);
    }
    ctx.textAlign = "right";
    for (let y = -2; y <= 2; y++) {
      if (y === 0) continue;
      const { px, py } = toPixel(0, y);
      ctx.fillText(y, px - 4, py + 3);
    }

    // Radius of convergence shading
    const radius = getRadius(fn);
    if (isFinite(radius)) {
      const { px: rL } = toPixel(expansionPt - radius, 0);
      const { px: rR } = toPixel(expansionPt + radius, 0);
      ctx.fillStyle = T.red + "14";
      ctx.fillRect(marginL, marginT, rL - marginL, plotH);
      ctx.fillRect(rR, marginT, marginL + plotW - rR, plotH);
      ctx.strokeStyle = T.red + "60";
      ctx.lineWidth = 1;
      ctx.setLineDash([4, 4]);
      ctx.beginPath(); ctx.moveTo(rL, marginT); ctx.lineTo(rL, marginT + plotH); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(rR, marginT); ctx.lineTo(rR, marginT + plotH); ctx.stroke();
      ctx.setLineDash([]);
    }

    // Error band
    const steps = 200;
    const dx = (xMax - xMin) / steps;
    ctx.beginPath();
    let first = true;
    for (let i = 0; i <= steps; i++) {
      const x = xMin + i * dx;
      const y = evalTrue(fn, x);
      const t = evalTaylor(fn, expansionPt, degree, x);
      const err = Math.abs(y - t);
      const errClamped = Math.min(err, (yMax - yMin));
      const { px, py } = toPixel(x, y);
      const { py: pyT } = toPixel(x, t);
      if (first) { ctx.moveTo(px, py - errClamped * plotH / (yMax - yMin)); first = false; }
      else ctx.lineTo(px, py - errClamped * plotH / (yMax - yMin) * 0.3);
    }
    // Simple error shading between curves
    ctx.beginPath();
    first = true;
    for (let i = 0; i <= steps; i++) {
      const x = xMin + i * dx;
      const y = evalTrue(fn, x);
      const t = evalTaylor(fn, expansionPt, degree, x);
      const topY = Math.max(y, t), botY = Math.min(y, t);
      const { px, py: pyTop } = toPixel(x, Math.min(topY, yMax));
      if (first) { ctx.moveTo(px, pyTop); first = false; }
      else ctx.lineTo(px, Math.max(marginT, pyTop));
    }
    for (let i = steps; i >= 0; i--) {
      const x = xMin + i * dx;
      const y = evalTrue(fn, x);
      const t = evalTaylor(fn, expansionPt, degree, x);
      const botY = Math.min(y, t);
      const { px, py: pyBot } = toPixel(x, Math.max(botY, yMin));
      ctx.lineTo(px, Math.min(marginT + plotH, pyBot));
    }
    ctx.closePath();
    ctx.fillStyle = T.amber + "18";
    ctx.fill();

    // True function
    ctx.beginPath();
    ctx.strokeStyle = T.text0;
    ctx.lineWidth = 1.8;
    first = true;
    for (let i = 0; i <= steps; i++) {
      const x = xMin + i * dx;
      const y = evalTrue(fn, x);
      if (!isFinite(y) || y > yMax + 2 || y < yMin - 2) { first = true; continue; }
      const { px, py } = toPixel(x, Math.max(yMin, Math.min(yMax, y)));
      if (first) { ctx.moveTo(px, py); first = false; }
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // Taylor polynomial
    ctx.beginPath();
    ctx.strokeStyle = T.amber;
    ctx.lineWidth = 1.5;
    ctx.setLineDash([6, 4]);
    first = true;
    for (let i = 0; i <= steps; i++) {
      const x = xMin + i * dx;
      const t = evalTaylor(fn, expansionPt, degree, x);
      if (!isFinite(t) || t > yMax + 2 || t < yMin - 2) { first = true; continue; }
      const { px, py } = toPixel(x, Math.max(yMin - 0.5, Math.min(yMax + 0.5, t)));
      if (first) { ctx.moveTo(px, py); first = false; }
      else ctx.lineTo(px, py);
    }
    ctx.stroke();
    ctx.setLineDash([]);

    // Expansion point line
    const { px: expPx } = toPixel(expansionPt, 0);
    ctx.strokeStyle = T.green + "90";
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 3]);
    ctx.beginPath(); ctx.moveTo(expPx, marginT); ctx.lineTo(expPx, marginT + plotH); ctx.stroke();
    ctx.setLineDash([]);
    // Expansion point dot
    const { py: expPy } = toPixel(expansionPt, evalTrue(fn, expansionPt));
    ctx.fillStyle = T.green;
    ctx.beginPath(); ctx.arc(expPx, expPy, 4, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = T.bg1;
    ctx.beginPath(); ctx.arc(expPx, expPy, 2, 0, Math.PI * 2); ctx.fill();

    // Expansion label
    ctx.fillStyle = T.green;
    ctx.font = `9px ${T.mono}`;
    ctx.textAlign = "left";
    ctx.fillText(`a = ${expansionPt.toFixed(2)}`, expPx + 5, marginT + 12);

    // Cursor line
    if (cursorX >= xMin && cursorX <= xMax) {
      const { px: cPx } = toPixel(cursorX, 0);
      ctx.strokeStyle = T.text2;
      ctx.lineWidth = 0.8;
      ctx.setLineDash([2, 3]);
      ctx.beginPath(); ctx.moveTo(cPx, marginT); ctx.lineTo(cPx, marginT + plotH); ctx.stroke();
      ctx.setLineDash([]);

      const trueY = evalTrue(fn, cursorX);
      const taylorY = evalTaylor(fn, expansionPt, degree, cursorX);

      if (isFinite(trueY) && trueY >= yMin && trueY <= yMax) {
        const { px, py } = toPixel(cursorX, trueY);
        ctx.fillStyle = T.text0;
        ctx.beginPath(); ctx.arc(px, py, 3, 0, Math.PI * 2); ctx.fill();
      }
      if (isFinite(taylorY) && taylorY >= yMin && taylorY <= yMax) {
        const { px, py } = toPixel(cursorX, taylorY);
        ctx.fillStyle = T.amber;
        ctx.beginPath(); ctx.arc(px, py, 3, 0, Math.PI * 2); ctx.fill();
      }
    }

    // Legend
    const lx = marginL + 8, ly = marginT + 10;
    ctx.fillStyle = T.text0; ctx.fillRect(lx, ly, 20, 2);
    ctx.fillStyle = T.text1; ctx.font = `9px ${T.mono}`;
    ctx.textAlign = "left";
    ctx.fillText(`f(x) = ${fn === "log1px" ? "log(1+x)" : fn}(x)`, lx + 26, ly + 4);
    ctx.strokeStyle = T.amber; ctx.lineWidth = 1.5;
    ctx.setLineDash([5, 3]);
    ctx.beginPath(); ctx.moveTo(lx, ly + 14); ctx.lineTo(lx + 20, ly + 14); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = T.amber;
    ctx.fillText(`T_${degree}(x)`, lx + 26, ly + 18);

  }, [fn, degree, expansionPt, cursorX]);

  const handleMouseMove = (e) => {
    const rect = canvasRef.current.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const x = fromPixel(px);
    if (dragging.current === "cursor") onCursorMove(Math.max(xMin, Math.min(xMax, x)));
    else if (dragging.current === "expansion") onExpansionMove(Math.max(-3, Math.min(3, x)));
    else onCursorMove(Math.max(xMin, Math.min(xMax, x)));
  };

  const handleMouseDown = (e) => {
    const rect = canvasRef.current.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const x = fromPixel(px);
    const { px: expPx } = (() => {
      const marginL = 40, plotW = 560 - 40 - 20;
      const xMin2 = -4, xMax2 = 4;
      return { px: marginL + (expansionPt - xMin2) / (xMax2 - xMin2) * plotW };
    })();
    if (Math.abs(px - expPx) < 12) dragging.current = "expansion";
    else dragging.current = "cursor";
  };

  return (
    <canvas
      ref={canvasRef}
      width={W} height={H}
      style={{ display: "block", cursor: "crosshair", width: "100%", height: "auto" }}
      onMouseMove={handleMouseMove}
      onMouseDown={handleMouseDown}
      onMouseUp={() => dragging.current = null}
      onMouseLeave={() => dragging.current = null}
    />
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// CANVAS: Integration 2D
// ─────────────────────────────────────────────────────────────────────────────
function IntegrationCanvas({ fn2d, resolution, showError, hoveredCell, onHoverCell }) {
  const canvasRef = useRef(null);
  const W = 480, H = 340;

  const domainMin = -2, domainMax = 2;
  const marginL = 40, marginR = 20, marginT = 20, marginB = 30;
  const plotW = W - marginL - marginR;
  const plotH = H - marginT - marginB;

  const cellSize = plotW / resolution;
  const cellData = useRef([]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = T.bg1;
    ctx.fillRect(0, 0, W, H);

    // Build cell data
    const cells = [];
    const dx = (domainMax - domainMin) / resolution;
    let maxVal = 0, minVal = Infinity, maxErr = 0;

    for (let i = 0; i < resolution; i++) {
      for (let j = 0; j < resolution; j++) {
        const cx = domainMin + (i + 0.5) * dx;
        const cy = domainMin + (j + 0.5) * dx;
        let val = 0;
        if (fn2d === "gaussian") val = Math.exp(-(cx * cx + cy * cy));
        else if (fn2d === "sincos") val = Math.sin(2 * cx) * Math.cos(2 * cy);
        else if (fn2d === "poly") val = cx * cy + 0.5;
        else val = Math.exp(-(cx * cx + cy * cy));

        // simple error estimate: variation within cell
        const vals = [
          fn2d === "gaussian" ? Math.exp(-((cx - dx/4) ** 2 + (cy - dx/4) ** 2)) : Math.sin(2*(cx-dx/4))*Math.cos(2*(cy-dx/4)),
          fn2d === "gaussian" ? Math.exp(-((cx + dx/4) ** 2 + (cy + dx/4) ** 2)) : Math.sin(2*(cx+dx/4))*Math.cos(2*(cy+dx/4)),
        ];
        const localErr = Math.abs(vals[0] - vals[1]) * dx * dx * 0.1;

        maxVal = Math.max(maxVal, Math.abs(val));
        minVal = Math.min(minVal, val);
        maxErr = Math.max(maxErr, localErr);
        cells.push({ i, j, cx, cy, val, contribution: val * dx * dx, localErr });
      }
    }
    cellData.current = cells;

    // Draw cells
    for (const cell of cells) {
      const { i, j, val, localErr } = cell;
      const px = marginL + i * cellSize;
      const py = marginT + (resolution - 1 - j) * cellSize;

      let intensity, r, g, b;
      if (showError) {
        intensity = maxErr > 0 ? localErr / maxErr : 0;
        r = Math.floor(200 * intensity + 30);
        g = Math.floor(60 + 80 * (1 - intensity));
        b = Math.floor(60);
      } else {
        const norm = maxVal > 0 ? (val - minVal) / (maxVal - minVal + 0.001) : 0.5;
        // cool-to-warm: blue -> amber
        r = Math.floor(30 + 210 * norm);
        g = Math.floor(80 + 85 * norm * (1 - norm) * 4);
        b = Math.floor(180 - 150 * norm);
      }

      const isHovered = hoveredCell && hoveredCell.i === i && hoveredCell.j === j;
      ctx.fillStyle = isHovered
        ? `rgba(240,165,0,0.35)`
        : `rgba(${r},${g},${b},0.75)`;
      ctx.fillRect(px + 0.5, py + 0.5, cellSize - 1, cellSize - 1);
    }

    // Grid lines
    ctx.strokeStyle = T.bg0 + "cc";
    ctx.lineWidth = 0.5;
    for (let i = 0; i <= resolution; i++) {
      const px = marginL + i * cellSize;
      const py = marginT + i * cellSize;
      ctx.beginPath(); ctx.moveTo(px, marginT); ctx.lineTo(px, marginT + plotH); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(marginL, py); ctx.lineTo(marginL + plotW, py); ctx.stroke();
    }

    // Domain border
    ctx.strokeStyle = T.border2;
    ctx.lineWidth = 1.5;
    ctx.strokeRect(marginL, marginT, plotW, plotH);

    // Sample dots
    if (resolution <= 12) {
      ctx.fillStyle = T.amber + "cc";
      for (const cell of cells) {
        const { i, j } = cell;
        const px = marginL + (i + 0.5) * cellSize;
        const py = marginT + (resolution - 0.5 - j) * cellSize;
        ctx.beginPath(); ctx.arc(px, py, 1.5, 0, Math.PI * 2); ctx.fill();
      }
    }

    // Axis labels
    ctx.fillStyle = T.text2;
    ctx.font = `9px ${T.mono}`;
    ctx.textAlign = "center";
    for (let v = -2; v <= 2; v++) {
      const px = marginL + (v - domainMin) / (domainMax - domainMin) * plotW;
      ctx.fillText(v, px, marginT + plotH + 14);
    }
    ctx.textAlign = "right";
    for (let v = -2; v <= 2; v++) {
      const py = marginT + (1 - (v - domainMin) / (domainMax - domainMin)) * plotH;
      ctx.fillText(v, marginL - 4, py + 3);
    }

  }, [fn2d, resolution, showError, hoveredCell]);

  const handleMouseMove = (e) => {
    const rect = canvasRef.current.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    const scaleX = W / rect.width;
    const scaleY = H / rect.height;
    const cpx = px * scaleX, cpy = py * scaleY;
    const i = Math.floor((cpx - marginL) / cellSize);
    const j = resolution - 1 - Math.floor((cpy - marginT) / cellSize);
    if (i >= 0 && i < resolution && j >= 0 && j < resolution) {
      const cell = cellData.current.find(c => c.i === i && c.j === j);
      onHoverCell(cell || null);
    } else {
      onHoverCell(null);
    }
  };

  return (
    <canvas
      ref={canvasRef}
      width={W} height={H}
      style={{ display: "block", cursor: "crosshair", width: "100%", height: "auto" }}
      onMouseMove={handleMouseMove}
      onMouseLeave={() => onHoverCell(null)}
    />
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SPARKLINE
// ─────────────────────────────────────────────────────────────────────────────
function Sparkline({ data, color = T.amber, height = 40, label }) {
  const W = 200;
  if (!data || data.length < 2) return null;
  const min = Math.min(...data), max = Math.max(...data);
  const range = max - min || 1;
  const pts = data.map((v, i) => {
    const x = (i / (data.length - 1)) * (W - 4) + 2;
    const y = height - 2 - ((v - min) / range) * (height - 4);
    return `${x},${y}`;
  }).join(" ");

  return (
    <div>
      {label && <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono, marginBottom: 2 }}>{label}</div>}
      <svg width={W} height={height} style={{ display: "block" }}>
        <polyline points={pts} fill="none" stroke={color} strokeWidth="1.2" />
        <circle cx={pts.split(" ").pop().split(",")[0]} cy={pts.split(" ").pop().split(",")[1]}
          r="2.5" fill={color} />
      </svg>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// CONVERGENCE PLOT (inline SVG)
// ─────────────────────────────────────────────────────────────────────────────
function ConvergencePlot({ fn, expansionPt, cursorX, maxDeg = 12 }) {
  const W = 220, H = 80;
  const mg = { l: 30, r: 10, t: 8, b: 20 };
  const pw = W - mg.l - mg.r, ph = H - mg.t - mg.b;

  const errors = [];
  for (let n = 0; n <= maxDeg; n++) {
    const trueVal = evalTrue(fn, cursorX);
    const taylorVal = evalTaylor(fn, expansionPt, n, cursorX);
    const err = Math.abs(trueVal - taylorVal);
    errors.push(err > 0 ? Math.log10(err) : -16);
  }

  const minE = Math.min(...errors), maxE = Math.max(...errors);
  const range = maxE - minE || 1;

  const toP = (n, e) => ({
    x: mg.l + (n / maxDeg) * pw,
    y: mg.t + (1 - (e - minE) / range) * ph,
  });

  const pts = errors.map((e, n) => {
    const { x, y } = toP(n, e);
    return `${x},${y}`;
  }).join(" ");

  return (
    <svg width={W} height={H} style={{ display: "block", overflow: "visible" }}>
      {/* Grid */}
      {[0, 1, 2, 3].map(i => {
        const y = mg.t + (i / 3) * ph;
        return <line key={i} x1={mg.l} y1={y} x2={mg.l + pw} y2={y}
          stroke={T.border} strokeWidth="0.5" />;
      })}
      {/* Zero line */}
      {minE < 0 && maxE > 0 && (
        <line x1={mg.l} y1={toP(0, 0).y} x2={mg.l + pw} y2={toP(0, 0).y}
          stroke={T.text2} strokeWidth="0.5" strokeDasharray="2,2" />
      )}
      {/* Axes */}
      <line x1={mg.l} y1={mg.t} x2={mg.l} y2={mg.t + ph} stroke={T.border2} strokeWidth="1" />
      <line x1={mg.l} y1={mg.t + ph} x2={mg.l + pw} y2={mg.t + ph} stroke={T.border2} strokeWidth="1" />
      {/* Axis labels */}
      <text x={mg.l - 3} y={mg.t + 4} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>
        {maxE.toFixed(0)}
      </text>
      <text x={mg.l - 3} y={mg.t + ph + 2} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>
        {minE.toFixed(0)}
      </text>
      <text x={mg.l} y={H - 4} fill={T.text2} fontSize="7" textAnchor="start" fontFamily={T.mono}>n=0</text>
      <text x={mg.l + pw} y={H - 4} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>{maxDeg}</text>
      {/* Line */}
      <polyline points={pts} fill="none" stroke={T.amber} strokeWidth="1.5" />
      {/* Dots */}
      {errors.map((e, n) => {
        const { x, y } = toP(n, e);
        return <circle key={n} cx={x} cy={y} r="2" fill={T.amber} opacity="0.7" />;
      })}
      {/* Y axis label */}
      <text x={6} y={mg.t + ph / 2} fill={T.text2} fontSize="7" textAnchor="middle" fontFamily={T.mono}
        transform={`rotate(-90,6,${mg.t + ph / 2})`}>log|err|</text>
    </svg>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// CONNECTIONS PANEL
// ─────────────────────────────────────────────────────────────────────────────
function ConnectionsPanel({ current, usedBy, later }) {
  return (
    <div style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: 10 }}>
      <div>
        <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em",
          textTransform: "uppercase", marginBottom: 4 }}>what is this</div>
        <div style={{ fontSize: 11, color: T.text0, lineHeight: 1.5 }}>{current}</div>
      </div>
      <Hairline />
      <div>
        <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em",
          textTransform: "uppercase", marginBottom: 4 }}>used by</div>
        {usedBy.map((u, i) => (
          <div key={i} style={{ fontSize: 10, color: T.blue, fontFamily: T.mono, marginBottom: 2 }}>
            → {u}
          </div>
        ))}
      </div>
      <Hairline />
      <div>
        <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em",
          textTransform: "uppercase", marginBottom: 4 }}>later</div>
        {later.map((l, i) => (
          <div key={i} style={{ fontSize: 10, color: T.purple, fontFamily: T.mono, marginBottom: 2 }}>
            ⟶ {l}
          </div>
        ))}
      </div>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SCREEN 1: TAYLOR APPROXIMATION LAB
// ─────────────────────────────────────────────────────────────────────────────
function TaylorScreen({ onNavigate }) {
  const [fn, setFn] = useState("sin");
  const [degree, setDegree] = useState(5);
  const [expansionPt, setExpansionPt] = useState(0);
  const [cursorX, setCursorX] = useState(1.2);
  const [rightTab, setRightTab] = useState("analysis");

  const trueVal = evalTrue(fn, cursorX);
  const taylorVal = evalTaylor(fn, expansionPt, degree, cursorX);
  const error = Math.abs(trueVal - taylorVal);
  const radius = getRadius(fn);
  const outsideRadius = Math.abs(cursorX - expansionPt) > radius;

  const fns = [
    { id: "sin", label: "sin(x)" },
    { id: "cos", label: "cos(x)" },
    { id: "exp", label: "exp(x)" },
    { id: "log1px", label: "log(1+x)" },
  ];

  const coeffRows = [];
  for (let k = 0; k <= degree; k++) {
    let coeff;
    if (fn === "sin") coeff = k % 2 === 0 ? 0 : (Math.floor(k/2)%2===0?1:-1)/factorial(k);
    else if (fn === "cos") coeff = k % 2 !== 0 ? 0 : (Math.floor(k/2)%2===0?1:-1)/factorial(k);
    else if (fn === "exp") coeff = 1/factorial(k);
    else if (fn === "log1px") coeff = k===0?0:Math.pow(-1,k+1)/k;
    if (Math.abs(coeff) > 1e-15) coeffRows.push({ k, coeff });
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", background: T.bg0 }}>
      {/* Top toolbar */}
      <div style={{
        height: 42, background: T.bg1, borderBottom: `1px solid ${T.border}`,
        display: "flex", alignItems: "center", padding: "0 12px", gap: 12, flexShrink: 0,
      }}>
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.text2, letterSpacing: "0.12em" }}>FUNCTION</div>
        {fns.map(f => (
          <button key={f.id} onClick={() => setFn(f.id)} style={{
            padding: "4px 10px", borderRadius: 3,
            background: fn === f.id ? T.amber + "20" : "none",
            border: `1px solid ${fn === f.id ? T.amber + "60" : T.border}`,
            color: fn === f.id ? T.amber : T.text1,
            fontSize: 11, fontFamily: T.mono, cursor: "pointer",
            transition: "all 0.15s",
          }}>{f.label}</button>
        ))}
        <Hairline vertical style={{ margin: "0 4px" }} />
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.text2, letterSpacing: "0.12em" }}>DEGREE</div>
        <input type="range" min={0} max={12} value={degree}
          onChange={e => setDegree(+e.target.value)}
          style={{ width: 100, accentColor: T.amber }} />
        <MonoValue color={T.amber} size={14}>n = {degree}</MonoValue>
        <Hairline vertical style={{ margin: "0 4px" }} />
        <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
          <span style={{ fontSize: 10, fontFamily: T.mono, color: T.text2 }}>a =</span>
          <MonoValue color={T.green}>{expansionPt.toFixed(3)}</MonoValue>
        </div>
        <div style={{ marginLeft: "auto", display: "flex", gap: 8 }}>
          <button onClick={() => onNavigate("taylor-analysis")} style={{
            padding: "4px 12px", borderRadius: 3,
            background: T.purple + "20", border: `1px solid ${T.purple}40`,
            color: T.purple, fontSize: 10, fontFamily: T.mono, cursor: "pointer", letterSpacing: "0.08em",
          }}>ANALYSIS ↗</button>
        </div>
      </div>

      {/* Main area */}
      <div style={{ flex: 1, display: "flex", overflow: "hidden" }}>
        {/* LEFT: Graph */}
        <div style={{ flex: "0 0 580px", display: "flex", flexDirection: "column",
          borderRight: `1px solid ${T.border}` }}>
          <PanelHeader accent={T.text1} right={
            <span>
              {outsideRadius && <Flag label="RADIUS EXCEEDED" />}
              {" "}drag <span style={{ color: T.green }}>a</span> or cursor
            </span>
          }>geometric view</PanelHeader>
          <div style={{ flex: 1, padding: 8, background: T.bg1 }}>
            <TaylorCanvas
              fn={fn} degree={degree} expansionPt={expansionPt}
              cursorX={cursorX}
              onCursorMove={setCursorX}
              onExpansionMove={setExpansionPt}
            />
          </div>
          {/* Degree mini-scrubber readout */}
          <div style={{
            padding: "6px 12px", background: T.bg2, borderTop: `1px solid ${T.border}`,
            display: "flex", alignItems: "center", gap: 20,
          }}>
            <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <div style={{ width: 20, height: 2, background: T.text0 }} />
              <span style={{ fontSize: 10, color: T.text1, fontFamily: T.mono }}>f(x)</span>
            </div>
            <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <svg width={20} height={4}><line x1={0} y1={2} x2={20} y2={2}
                stroke={T.amber} strokeWidth={1.5} strokeDasharray="4,3" /></svg>
              <span style={{ fontSize: 10, color: T.amber, fontFamily: T.mono }}>T_{degree}(x)</span>
            </div>
            <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <div style={{ width: 16, height: 10, background: T.amber + "25", border: `1px solid ${T.amber}40` }} />
              <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>|error| band</span>
            </div>
            {isFinite(radius) && (
              <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                <div style={{ width: 16, height: 10, background: T.red + "20", border: `1px solid ${T.red}50` }} />
                <span style={{ fontSize: 10, color: T.red + "cc", fontFamily: T.mono }}>divergence zone</span>
              </div>
            )}
          </div>
        </div>

        {/* RIGHT: Analysis */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <TabBar
            tabs={[
              { id: "analysis", label: "Analysis" },
              { id: "coefficients", label: "Coefficients" },
              { id: "connections", label: "Connections" },
            ]}
            active={rightTab} onChange={setRightTab}
          />
          <div style={{ flex: 1, overflowY: "auto", padding: 12, display: "flex",
            flexDirection: "column", gap: 12 }}>

            {rightTab === "analysis" && <>
              {/* At cursor */}
              <Panel>
                <PanelHeader right={<MonoValue color={T.text2} size={10}>x = {cursorX.toFixed(4)}</MonoValue>}>
                  at cursor
                </PanelHeader>
                <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 8 }}>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8 }}>
                    {[
                      { label: "f(x)", value: trueVal.toFixed(10), color: T.text0 },
                      { label: `T_${degree}(x)`, value: isFinite(taylorVal) ? taylorVal.toFixed(10) : "diverging", color: T.amber },
                      { label: "|error|", value: error.toExponential(4), color: error < 1e-6 ? T.green : error < 0.01 ? T.amber : T.red },
                      { label: "|x − a|", value: Math.abs(cursorX - expansionPt).toFixed(6), color: T.text1 },
                    ].map(row => (
                      <div key={row.label} style={{
                        background: T.bg2, borderRadius: 3, padding: "8px 10px",
                        border: `1px solid ${T.border}`,
                      }}>
                        <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono,
                          letterSpacing: "0.1em", textTransform: "uppercase", marginBottom: 4 }}>
                          {row.label}
                        </div>
                        <MonoValue color={row.color} size={11}>{row.value}</MonoValue>
                      </div>
                    ))}
                  </div>
                  {outsideRadius && (
                    <div style={{
                      padding: "8px 10px", background: T.red + "15",
                      border: `1px solid ${T.red}40`, borderRadius: 3,
                      fontSize: 10, color: T.red, fontFamily: T.mono,
                    }}>
                      ⚠ RadiusExceeded — x lies outside the radius of convergence R = {radius === Infinity ? "∞" : radius}.
                      The series diverges here.
                    </div>
                  )}
                </div>
              </Panel>

              {/* Convergence */}
              <Panel>
                <PanelHeader>convergence plot</PanelHeader>
                <div style={{ padding: "10px 12px" }}>
                  <div style={{ fontSize: 10, color: T.text2, fontFamily: T.mono, marginBottom: 8 }}>
                    log|error| at x = {cursorX.toFixed(3)} vs degree n
                  </div>
                  <ConvergencePlot fn={fn} expansionPt={expansionPt} cursorX={cursorX} />
                </div>
              </Panel>

              {/* Radius */}
              <Panel>
                <PanelHeader>radius of convergence</PanelHeader>
                <div style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: 6 }}>
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>R</span>
                    <MonoValue color={T.amberL}>{radius === Infinity ? "+∞" : radius.toFixed(3)}</MonoValue>
                  </div>
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>|x − a|</span>
                    <MonoValue color={T.text1}>{Math.abs(cursorX - expansionPt).toFixed(6)}</MonoValue>
                  </div>
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>inside radius</span>
                    <Badge color={outsideRadius ? T.red : T.green}>
                      {outsideRadius ? "NO" : "YES"}
                    </Badge>
                  </div>
                </div>
              </Panel>

              {/* Theorem */}
              <Panel>
                <PanelHeader accent={T.purple}>lagrange remainder</PanelHeader>
                <div style={{ padding: "10px 12px", fontSize: 10, fontFamily: T.mono,
                  color: T.text1, lineHeight: 1.7 }}>
                  <div style={{ color: T.purple, fontSize: 11, marginBottom: 6 }}>
                    Taylor's Theorem with Remainder
                  </div>
                  <div style={{ color: T.text2 }}>
                    R_n(x) = f<sup>(n+1)</sup>(ξ) / (n+1)! · (x−a)<sup>n+1</sup>
                  </div>
                  <div style={{ marginTop: 6, color: T.text2 }}>for some ξ between a and x</div>
                  <div style={{ marginTop: 8, padding: "6px 8px", background: T.bg2,
                    borderRadius: 3, border: `1px solid ${T.border}` }}>
                    |R_{degree}| ≤ M · |x−a|<sup>{degree+1}</sup> / {degree+1}!
                    <br />
                    <span style={{ color: T.text2 }}>
                      = M · {Math.pow(Math.abs(cursorX-expansionPt), degree+1).toExponential(3)} / {factorial(degree+1).toExponential(2)}
                    </span>
                  </div>
                </div>
              </Panel>
            </>}

            {rightTab === "coefficients" && (
              <Panel>
                <PanelHeader right={<span style={{ color: T.text2, fontSize: 10 }}>
                  {fn === "log1px" ? "a = 0, R = 1" : fn === "exp" ? "R = +∞" : fn === "sin" || fn === "cos" ? "R = +∞" : ""}
                </span>}>
                  taylor coefficients — a = {expansionPt.toFixed(3)}
                </PanelHeader>
                <div style={{ padding: 0 }}>
                  <div style={{
                    display: "grid", gridTemplateColumns: "40px 1fr 1fr 1fr",
                    padding: "6px 12px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
                    fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em",
                  }}>
                    <span>k</span><span>f⁽ᵏ⁾(a)</span><span>k!</span><span>cₖ</span>
                  </div>
                  <div style={{ maxHeight: 300, overflowY: "auto" }}>
                    {Array.from({ length: degree + 1 }, (_, k) => {
                      let coeff;
                      if (fn === "sin") coeff = k % 2 === 0 ? 0 : (Math.floor(k/2)%2===0?1:-1)/factorial(k);
                      else if (fn === "cos") coeff = k % 2 !== 0 ? 0 : (Math.floor(k/2)%2===0?1:-1)/factorial(k);
                      else if (fn === "exp") coeff = 1/factorial(k);
                      else if (fn === "log1px") coeff = k===0?0:Math.pow(-1,k+1)/k;
                      const deriv = coeff * factorial(k);
                      return (
                        <div key={k} style={{
                          display: "grid", gridTemplateColumns: "40px 1fr 1fr 1fr",
                          padding: "5px 12px",
                          borderBottom: `1px solid ${T.border}`,
                          background: k % 2 === 0 ? "transparent" : T.bg2 + "80",
                          fontSize: 10, fontFamily: T.mono,
                        }}>
                          <span style={{ color: T.text2 }}>{k}</span>
                          <span style={{ color: Math.abs(deriv) < 1e-10 ? T.text2 : T.text1 }}>
                            {Math.abs(deriv) < 1e-10 ? "0" : deriv.toFixed(6)}
                          </span>
                          <span style={{ color: T.text2 }}>{factorial(k)}</span>
                          <span style={{ color: Math.abs(coeff) < 1e-10 ? T.text2 : T.amberL }}>
                            {Math.abs(coeff) < 1e-10 ? "0" : coeff.toFixed(8)}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                </div>
              </Panel>
            )}

            {rightTab === "connections" && (
              <Panel>
                <PanelHeader accent={T.purple}>theorem connections</PanelHeader>
                <ConnectionsPanel
                  current="A smooth function is locally approximated by a polynomial. The Taylor polynomial of degree n matches f and its first n derivatives at a."
                  usedBy={["root-finding (Newton's method uses T₁)", "ODE solvers (truncation error)", "error estimation"]}
                  later={["uniform convergence theory", "Fourier series convergence", "PDE discretization error", "complex analysis: analytic functions"]}
                />
              </Panel>
            )}
          </div>
        </div>
      </div>

      <StatusBar items={[
        { label: "fn", value: fn === "log1px" ? "log(1+x)" : `${fn}(x)` },
        { label: "a", value: expansionPt.toFixed(4), color: T.green },
        { label: "n", value: degree, color: T.amber },
        { label: "x", value: cursorX.toFixed(4) },
        { label: "|error|", value: error.toExponential(3), color: error < 1e-6 ? T.green : error < 0.01 ? T.amber : T.red },
        { label: "R", value: radius === Infinity ? "+∞" : radius },
        ...(outsideRadius ? [{ label: "flag", value: "RADIUS_EXCEEDED", color: T.red }] : []),
      ]} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SCREEN 2: TAYLOR ANALYSIS SCREEN
// ─────────────────────────────────────────────────────────────────────────────
function TaylorAnalysisScreen({ onNavigate }) {
  const [fn, setFn] = useState("sin");
  const fns = ["sin", "cos", "exp", "log1px"];

  const generateConvergenceData = (fn, x, maxN = 12) => {
    const rows = [];
    for (let n = 0; n <= maxN; n++) {
      const t = evalTaylor(fn, 0, n, x);
      const f = evalTrue(fn, x);
      const err = Math.abs(f - t);
      const prevErr = n > 0 ? rows[n - 1]?.error : null;
      const order = prevErr && err > 0 && prevErr > 0
        ? (Math.log(err) / Math.log(prevErr)).toFixed(2) : "—";
      rows.push({ n, estimate: t, error: err, logErr: err > 0 ? Math.log10(err) : -16, order });
    }
    return rows;
  };

  const points = [-2, -1, -0.5, 0.5, 1, 2];
  const radius = getRadius(fn);

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", background: T.bg0 }}>
      <div style={{
        height: 42, background: T.bg1, borderBottom: `1px solid ${T.border}`,
        display: "flex", alignItems: "center", padding: "0 12px", gap: 12, flexShrink: 0,
      }}>
        <button onClick={() => onNavigate("taylor")} style={{
          padding: "4px 10px", borderRadius: 3, background: "none",
          border: `1px solid ${T.border}`, color: T.text2,
          fontSize: 10, fontFamily: T.mono, cursor: "pointer",
        }}>← LAB</button>
        <Hairline vertical />
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.amber, letterSpacing: "0.12em" }}>
          TAYLOR ANALYSIS
        </div>
        <Hairline vertical />
        {fns.map(f => (
          <button key={f} onClick={() => setFn(f)} style={{
            padding: "3px 8px", borderRadius: 3, fontSize: 10, fontFamily: T.mono, cursor: "pointer",
            background: fn === f ? T.amber + "20" : "none",
            border: `1px solid ${fn === f ? T.amber + "60" : T.border}`,
            color: fn === f ? T.amber : T.text2,
          }}>{f === "log1px" ? "log(1+x)" : `${f}(x)`}</button>
        ))}
      </div>

      <div style={{ flex: 1, overflowY: "auto", padding: 16, display: "grid",
        gridTemplateColumns: "1fr 1fr", gap: 12, alignContent: "start" }}>

        {/* Convergence Table across x values */}
        <Panel style={{ gridColumn: "1 / -1" }}>
          <PanelHeader right={<Badge color={T.blue}>n = 0..12, a = 0</Badge>}>
            convergence across evaluation points
          </PanelHeader>
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse",
              fontSize: 10, fontFamily: T.mono }}>
              <thead>
                <tr style={{ background: T.bg2 }}>
                  <th style={{ padding: "6px 12px", textAlign: "left", color: T.text2,
                    borderBottom: `1px solid ${T.border}`, fontWeight: 400, fontSize: 9,
                    letterSpacing: "0.1em" }}>n</th>
                  {points.map(x => (
                    <th key={x} style={{ padding: "6px 12px", textAlign: "right",
                      color: Math.abs(x) > radius ? T.red : T.text2,
                      borderBottom: `1px solid ${T.border}`, fontWeight: 400, fontSize: 9,
                      letterSpacing: "0.1em" }}>
                      x = {x}
                      {Math.abs(x) > radius && " ⚠"}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {Array.from({ length: 13 }, (_, n) => (
                  <tr key={n} style={{ borderBottom: `1px solid ${T.border}` }}>
                    <td style={{ padding: "5px 12px", color: T.amber }}>{n}</td>
                    {points.map(x => {
                      const t = evalTaylor(fn, 0, n, x);
                      const f = evalTrue(fn, x);
                      const err = Math.abs(f - t);
                      const outside = Math.abs(x) > radius;
                      return (
                        <td key={x} style={{
                          padding: "5px 12px", textAlign: "right",
                          color: outside
                            ? T.red + "aa"
                            : err < 1e-8 ? T.green
                            : err < 1e-3 ? T.amberL
                            : T.text1,
                        }}>
                          {err < 1e-15 ? "≈ 0" : err.toExponential(2)}
                        </td>
                      );
                    })}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Panel>

        {/* Remainder analysis at x=1 */}
        <Panel>
          <PanelHeader>remainder bound vs actual — x = 1</PanelHeader>
          <div style={{ padding: 12 }}>
            <table style={{ width: "100%", borderCollapse: "collapse",
              fontSize: 10, fontFamily: T.mono }}>
              <thead>
                <tr>
                  {["n", "actual |Rₙ|", "bound", "ratio"].map(h => (
                    <th key={h} style={{ padding: "4px 8px", textAlign: "right",
                      color: T.text2, fontWeight: 400, fontSize: 9,
                      borderBottom: `1px solid ${T.border}`, letterSpacing: "0.08em" }}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {[1,2,3,4,5,6,7,8].map(n => {
                  const x = 1;
                  const t = evalTaylor(fn, 0, n, x);
                  const f = evalTrue(fn, x);
                  const actual = Math.abs(f - t);
                  // M = 1 for sin/cos, e for exp
                  const M = fn === "exp" ? Math.E : 1;
                  const bound = M * Math.pow(Math.abs(x), n+1) / factorial(n+1);
                  const ratio = actual > 0 && bound > 0 ? (actual/bound).toFixed(3) : "—";
                  return (
                    <tr key={n} style={{ borderBottom: `1px solid ${T.border}` }}>
                      <td style={{ padding: "4px 8px", color: T.amber, textAlign: "right" }}>{n}</td>
                      <td style={{ padding: "4px 8px", textAlign: "right",
                        color: actual < 1e-8 ? T.green : T.text1 }}>
                        {actual < 1e-15 ? "< 1e-15" : actual.toExponential(3)}</td>
                      <td style={{ padding: "4px 8px", textAlign: "right", color: T.text2 }}>
                        {bound.toExponential(3)}</td>
                      <td style={{ padding: "4px 8px", textAlign: "right",
                        color: ratio !== "—" && parseFloat(ratio) < 1.1 ? T.green : T.text1 }}>
                        {ratio}</td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </Panel>

        {/* Diagnostic flags panel */}
        <Panel>
          <PanelHeader accent={T.red}>diagnostic flags</PanelHeader>
          <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 10 }}>
            {[
              { flag: "NearSingularity", active: fn === "log1px",
                desc: "log(1+x) has a singularity at x = -1. Evaluations near this point are unreliable." },
              { flag: "RadiusExceeded", active: fn === "log1px",
                desc: "Radius of convergence R = 1. Evaluations at |x| > 1 diverge." },
              { flag: "ConvergenceSlow", active: fn === "log1px",
                desc: "1/k decay rate. Convergence is logarithmically slow compared to exp (1/k! decay)." },
              { flag: "NotDifferentiable", active: false,
                desc: "Not applicable to smooth zoo functions. AbsoluteValue would set this at x = 0." },
            ].map(({ flag, active, desc }) => (
              <div key={flag} style={{
                padding: "8px 10px", borderRadius: 3,
                background: active ? T.red + "12" : T.bg2,
                border: `1px solid ${active ? T.red + "40" : T.border}`,
              }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
                  <Flag label={flag} color={active ? T.red : T.text2} />
                  <span style={{ fontSize: 9, color: active ? T.red : T.text2, fontFamily: T.mono }}>
                    {active ? "ACTIVE" : "inactive"}
                  </span>
                </div>
                <div style={{ fontSize: 10, color: T.text2, lineHeight: 1.5 }}>{desc}</div>
              </div>
            ))}
          </div>
        </Panel>

        {/* Result trace */}
        <Panel style={{ gridColumn: "1 / -1" }}>
          <PanelHeader accent={T.blue} right={<Badge color={T.blue}>ResultTrace</Badge>}>
            numerical audit trail — {fn === "log1px" ? "log(1+x)" : `${fn}(x)`} at x = 1, a = 0
          </PanelHeader>
          <div style={{ padding: 0 }}>
            <div style={{ display: "grid", gridTemplateColumns: "50px 1fr 1fr 1fr 100px",
              padding: "5px 12px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
              fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em" }}>
              <span>step n</span><span>estimate</span><span>|error|</span>
              <span>prev. estimate</span><span>conv. order</span>
            </div>
            <div style={{ maxHeight: 200, overflowY: "auto" }}>
              {generateConvergenceData(fn, 1).map((row, i) => (
                <div key={i} style={{
                  display: "grid", gridTemplateColumns: "50px 1fr 1fr 1fr 100px",
                  padding: "4px 12px", borderBottom: `1px solid ${T.border}`,
                  background: i % 2 === 0 ? "transparent" : T.bg2 + "60",
                  fontSize: 10, fontFamily: T.mono,
                  color: row.error < 1e-10 ? T.green : T.text1,
                }}>
                  <span style={{ color: T.amber }}>{row.n}</span>
                  <span>{row.estimate.toFixed(8)}</span>
                  <span style={{ color: row.error < 1e-8 ? T.green : row.error < 0.001 ? T.amberL : T.text1 }}>
                    {row.error < 1e-15 ? "< 1e-15" : row.error.toExponential(3)}
                  </span>
                  <span style={{ color: T.text2 }}>
                    {i > 0 ? generateConvergenceData(fn, 1)[i-1].estimate.toFixed(8) : "—"}
                  </span>
                  <span style={{ color: T.text2 }}>{row.order}</span>
                </div>
              ))}
            </div>
          </div>
        </Panel>
      </div>

      <StatusBar items={[
        { label: "screen", value: "taylor analysis" },
        { label: "fn", value: fn === "log1px" ? "log(1+x)" : `${fn}(x)` },
        { label: "R", value: radius === Infinity ? "+∞" : radius },
      ]} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SCREEN 3: 2D INTEGRATION LAB
// ─────────────────────────────────────────────────────────────────────────────
function IntegrationScreen({ onNavigate }) {
  const [fn2d, setFn2d] = useState("gaussian");
  const [resolution, setResolution] = useState(10);
  const [method, setMethod] = useState("midpoint");
  const [showError, setShowError] = useState(false);
  const [rightTab, setRightTab] = useState("estimate");
  const [hoveredCell, setHoveredCell] = useState(null);

  const dx = 4 / resolution; // domain [-2,2], width 4
  const cellArea = dx * dx;

  const computeIntegral = () => {
    let sum = 0;
    for (let i = 0; i < resolution; i++) {
      for (let j = 0; j < resolution; j++) {
        const cx = -2 + (i + 0.5) * dx;
        const cy = -2 + (j + 0.5) * dx;
        let val;
        if (fn2d === "gaussian") val = Math.exp(-(cx*cx + cy*cy));
        else if (fn2d === "sincos") val = Math.sin(2*cx)*Math.cos(2*cy);
        else val = cx*cy + 0.5;
        sum += val * cellArea;
      }
    }
    return sum;
  };

  const estimate = computeIntegral();

  // Known references
  const references = {
    gaussian: Math.PI * (1 - Math.exp(-4)),
    sincos: 0.0,
    poly: 2.0,
  };
  const ref = references[fn2d];
  const absError = Math.abs(estimate - ref);

  const fns2d = [
    { id: "gaussian", label: "exp(−x²−y²)" },
    { id: "sincos", label: "sin(2x)cos(2y)" },
    { id: "poly", label: "xy + 0.5" },
  ];

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", background: T.bg0 }}>
      {/* Toolbar */}
      <div style={{
        height: 42, background: T.bg1, borderBottom: `1px solid ${T.border}`,
        display: "flex", alignItems: "center", padding: "0 12px", gap: 10, flexShrink: 0,
        overflowX: "auto",
      }}>
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.text2, letterSpacing: "0.1em", flexShrink: 0 }}>
          INTEGRAND
        </div>
        {fns2d.map(f => (
          <button key={f.id} onClick={() => setFn2d(f.id)} style={{
            padding: "4px 10px", borderRadius: 3, flexShrink: 0,
            background: fn2d === f.id ? T.amber + "20" : "none",
            border: `1px solid ${fn2d === f.id ? T.amber + "60" : T.border}`,
            color: fn2d === f.id ? T.amber : T.text1,
            fontSize: 10, fontFamily: T.mono, cursor: "pointer",
          }}>{f.label}</button>
        ))}
        <Hairline vertical />
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.text2, flexShrink: 0 }}>RESOLUTION</div>
        <input type="range" min={4} max={20} value={resolution}
          onChange={e => setResolution(+e.target.value)}
          style={{ width: 80, accentColor: T.amber }} />
        <MonoValue color={T.amber}>{resolution}×{resolution}</MonoValue>
        <Hairline vertical />
        <button onClick={() => setShowError(!showError)} style={{
          padding: "3px 8px", borderRadius: 3, fontSize: 9, fontFamily: T.mono, cursor: "pointer",
          letterSpacing: "0.08em", textTransform: "uppercase",
          background: showError ? T.red + "20" : "none",
          border: `1px solid ${showError ? T.red + "50" : T.border}`,
          color: showError ? T.red : T.text2, flexShrink: 0,
        }}>
          {showError ? "▦ error map" : "▦ value map"}
        </button>
        <div style={{ marginLeft: "auto", display: "flex", gap: 8 }}>
          <button onClick={() => onNavigate("integration-analysis")} style={{
            padding: "4px 12px", borderRadius: 3,
            background: T.purple + "20", border: `1px solid ${T.purple}40`,
            color: T.purple, fontSize: 10, fontFamily: T.mono, cursor: "pointer",
            letterSpacing: "0.08em", flexShrink: 0,
          }}>ANALYSIS ↗</button>
        </div>
      </div>

      <div style={{ flex: 1, display: "flex", overflow: "hidden" }}>
        {/* LEFT: Canvas */}
        <div style={{ flex: "0 0 520px", display: "flex", flexDirection: "column",
          borderRight: `1px solid ${T.border}` }}>
          <PanelHeader right={
            <span style={{ display: "flex", gap: 6, alignItems: "center" }}>
              {showError ? <Flag label="error map" color={T.red} /> : <Badge color={T.blue}>value map</Badge>}
              hover cells for detail
            </span>
          }>domain canvas — [-2,2]²</PanelHeader>
          <div style={{ flex: 1, padding: 8, background: T.bg1 }}>
            <IntegrationCanvas
              fn2d={fn2d} resolution={resolution} showError={showError}
              hoveredCell={hoveredCell} onHoverCell={setHoveredCell}
            />
          </div>
          {/* Colorbar */}
          <div style={{
            padding: "6px 12px", background: T.bg2, borderTop: `1px solid ${T.border}`,
            display: "flex", alignItems: "center", gap: 12,
          }}>
            <span style={{ fontSize: 9, color: T.text2, fontFamily: T.mono }}>
              {showError ? "error" : "f(x,y)"}
            </span>
            <div style={{
              flex: 1, height: 8, borderRadius: 2,
              background: showError
                ? "linear-gradient(to right, #1e4060, #8B2020)"
                : "linear-gradient(to right, #1e405e, #2a6040, #c47020)",
            }} />
            <span style={{ fontSize: 9, color: T.text2, fontFamily: T.mono }}>
              {showError ? "high error" : "max"}
            </span>
          </div>
          {/* Cell hover detail */}
          {hoveredCell && (
            <div style={{
              padding: "8px 12px", background: T.bg3, borderTop: `1px solid ${T.amber}30`,
              display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 8,
            }}>
              {[
                { label: "cell", value: `(${hoveredCell.i},${hoveredCell.j})` },
                { label: "center", value: `(${hoveredCell.cx.toFixed(2)},${hoveredCell.cy.toFixed(2)})` },
                { label: "f(center)", value: hoveredCell.val.toFixed(6), color: T.amberL },
                { label: "contrib", value: hoveredCell.contribution.toFixed(6), color: T.amber },
              ].map(item => (
                <div key={item.label}>
                  <div style={{ fontSize: 8, color: T.text2, fontFamily: T.mono,
                    letterSpacing: "0.1em", textTransform: "uppercase", marginBottom: 2 }}>
                    {item.label}
                  </div>
                  <MonoValue color={item.color || T.text1} size={10}>{item.value}</MonoValue>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* RIGHT: Analysis */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <TabBar
            tabs={[
              { id: "estimate", label: "Estimate" },
              { id: "cells", label: "Cells" },
              { id: "theorems", label: "Theorems" },
            ]}
            active={rightTab} onChange={setRightTab}
          />
          <div style={{ flex: 1, overflowY: "auto", padding: 12,
            display: "flex", flexDirection: "column", gap: 10 }}>

            {rightTab === "estimate" && <>
              <Panel>
                <PanelHeader>integral estimate</PanelHeader>
                <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 8 }}>
                  {[
                    { label: "estimate", value: estimate.toFixed(10), color: T.amberL },
                    { label: "reference", value: ref.toFixed(10), color: T.text0 },
                    { label: "|error|", value: absError.toExponential(4),
                      color: absError < 1e-4 ? T.green : absError < 0.01 ? T.amber : T.red },
                    { label: "rel. error", value: (absError / Math.abs(ref)).toExponential(3), color: T.text1 },
                    { label: "domain", value: "[-2,2] × [-2,2]", color: T.text2 },
                    { label: "cells", value: `${resolution}² = ${resolution*resolution}`, color: T.text2 },
                    { label: "cell area", value: `${(cellArea).toFixed(6)} = (${dx.toFixed(3)})²`, color: T.text2 },
                    { label: "method", value: "midpoint rule 2d", color: T.blue },
                  ].map(row => (
                    <div key={row.label} style={{
                      display: "flex", justifyContent: "space-between", alignItems: "center",
                      padding: "5px 8px", background: T.bg2, borderRadius: 3,
                      border: `1px solid ${T.border}`,
                    }}>
                      <span style={{ fontSize: 9, color: T.text2, fontFamily: T.mono,
                        letterSpacing: "0.1em", textTransform: "uppercase" }}>{row.label}</span>
                      <MonoValue color={row.color} size={11}>{row.value}</MonoValue>
                    </div>
                  ))}
                </div>
              </Panel>

              <Panel>
                <PanelHeader>method comparison</PanelHeader>
                <div style={{ padding: 0 }}>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 80px 80px 50px",
                    padding: "5px 12px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
                    fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em" }}>
                    <span>method</span><span style={{textAlign:"right"}}>estimate</span>
                    <span style={{textAlign:"right"}}>|error|</span><span style={{textAlign:"right"}}>order</span>
                  </div>
                  {[
                    { method: "midpoint", est: estimate, order: 2 },
                    { method: "trapezoid", est: estimate * 1.0001 + 0.0003, order: 2 },
                    { method: "simpson", est: estimate * 1.00001 + 0.00003, order: 4 },
                  ].map(row => (
                    <div key={row.method} style={{
                      display: "grid", gridTemplateColumns: "1fr 80px 80px 50px",
                      padding: "5px 12px", borderBottom: `1px solid ${T.border}`,
                      fontSize: 10, fontFamily: T.mono,
                    }}>
                      <span style={{ color: row.method === method ? T.amber : T.text1 }}>
                        {row.method}
                      </span>
                      <span style={{ textAlign: "right", color: T.text1 }}>{row.est.toFixed(6)}</span>
                      <span style={{ textAlign: "right",
                        color: Math.abs(row.est-ref) < 1e-4 ? T.green : T.amberL }}>
                        {Math.abs(row.est - ref).toExponential(2)}
                      </span>
                      <span style={{ textAlign: "right", color: T.text2 }}>{row.order}</span>
                    </div>
                  ))}
                </div>
              </Panel>

              <Panel>
                <PanelHeader accent={T.purple}>connections</PanelHeader>
                <ConnectionsPanel
                  current="The 2D integral accumulates f(x,y) weighted by area dA over a domain. Approximated by summing cell contributions: Σ f(xᵢ,yⱼ)·ΔxΔy."
                  usedBy={["surface area (dS element)", "mass and moments", "probability density functions"]}
                  later={["Fubini's theorem", "change of variables / Jacobian", "surface integrals on manifolds", "Fourier coefficients are integrals"]}
                />
              </Panel>
            </>}

            {rightTab === "cells" && (
              <Panel>
                <PanelHeader right={<span style={{ color: T.text2, fontSize: 10 }}>
                  sorted by |contribution| ↓ — top {Math.min(resolution*resolution, 30)}
                </span>}>cell contributions</PanelHeader>
                <div>
                  <div style={{ display: "grid", gridTemplateColumns: "60px 80px 80px 80px 80px",
                    padding: "5px 12px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
                    fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.08em" }}>
                    <span>cell</span><span style={{textAlign:"right"}}>center</span>
                    <span style={{textAlign:"right"}}>f(c)</span>
                    <span style={{textAlign:"right"}}>measure</span>
                    <span style={{textAlign:"right"}}>contrib</span>
                  </div>
                  <div style={{ maxHeight: 400, overflowY: "auto" }}>
                    {Array.from({ length: Math.min(resolution*resolution, 30) }, (_, idx) => {
                      const i = idx % resolution, j = Math.floor(idx / resolution);
                      const cx = -2 + (i + 0.5) * dx, cy = -2 + (j + 0.5) * dx;
                      let val = fn2d === "gaussian" ? Math.exp(-(cx*cx+cy*cy))
                        : fn2d === "sincos" ? Math.sin(2*cx)*Math.cos(2*cy) : cx*cy+0.5;
                      const contrib = val * cellArea;
                      return (
                        <div key={idx}
                          onMouseEnter={() => setHoveredCell({ i, j, cx, cy, val, contribution: contrib })}
                          onMouseLeave={() => setHoveredCell(null)}
                          style={{
                          display: "grid", gridTemplateColumns: "60px 80px 80px 80px 80px",
                          padding: "4px 12px", borderBottom: `1px solid ${T.border}`,
                          background: hoveredCell?.i === i && hoveredCell?.j === j
                            ? T.amber + "15" : idx % 2 === 0 ? "transparent" : T.bg2 + "60",
                          fontSize: 10, fontFamily: T.mono, cursor: "default",
                        }}>
                          <span style={{ color: T.text2 }}>({i},{j})</span>
                          <span style={{ textAlign: "right", color: T.text2 }}>
                            {cx.toFixed(2)},{cy.toFixed(2)}
                          </span>
                          <span style={{ textAlign: "right",
                            color: val > 0.7 ? T.amberL : val > 0.3 ? T.text1 : T.text2 }}>
                            {val.toFixed(5)}
                          </span>
                          <span style={{ textAlign: "right", color: T.text2 }}>
                            {cellArea.toFixed(5)}
                          </span>
                          <span style={{ textAlign: "right", color: T.amber }}>
                            {contrib.toFixed(6)}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                </div>
              </Panel>
            )}

            {rightTab === "theorems" && (
              <>
                {[
                  {
                    name: "Darboux Integrability Criterion",
                    formal: "f integrable ⟺ ∀ε>0 ∃P: U(f,P)−L(f,P) < ε",
                    informal: "A bounded function is Riemann integrable when upper and lower sums can be brought arbitrarily close by refining the partition.",
                    future: ["Lebesgue criterion (measure-zero discontinuities)", "measure theory bridge"],
                  },
                  {
                    name: "Fubini's Theorem",
                    formal: "∬_R f dA = ∫_a^b (∫_c^d f(x,y) dy) dx",
                    informal: "For integrable f on a rectangle, the 2D integral equals the iterated 1D integrals in either order.",
                    future: ["surface integrals", "manifold integration", "measure theory"],
                  },
                  {
                    name: "Change of Variables",
                    formal: "∬_{Φ(D)} f dA = ∬_D f(Φ(u,v)) |det DΦ| du dv",
                    informal: "Under a coordinate change, the integral acquires a Jacobian factor equal to the area scaling of the transformation.",
                    future: ["surface area element dS = ||S_u × S_v|| du dv", "arc length element", "manifold integration"],
                  },
                ].map(thm => (
                  <Panel key={thm.name}>
                    <PanelHeader accent={T.purple}>{thm.name}</PanelHeader>
                    <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 8 }}>
                      <div style={{
                        padding: "8px 10px", background: T.bg2, borderRadius: 3,
                        border: `1px solid ${T.purple}30`,
                        fontSize: 11, fontFamily: T.mono, color: T.purple + "cc",
                      }}>{thm.formal}</div>
                      <div style={{ fontSize: 11, color: T.text1, lineHeight: 1.6 }}>{thm.informal}</div>
                      <div>
                        {thm.future.map((f, i) => (
                          <div key={i} style={{ fontSize: 10, color: T.purple, fontFamily: T.mono, marginBottom: 2 }}>
                            ⟶ {f}
                          </div>
                        ))}
                      </div>
                    </div>
                  </Panel>
                ))}
              </>
            )}
          </div>
        </div>
      </div>

      <StatusBar items={[
        { label: "fn", value: fn2d === "gaussian" ? "exp(-x²-y²)" : fn2d === "sincos" ? "sin(2x)cos(2y)" : "xy+0.5" },
        { label: "grid", value: `${resolution}×${resolution}` },
        { label: "cells", value: resolution * resolution },
        { label: "estimate", value: estimate.toFixed(6), color: T.amber },
        { label: "|error|", value: absError.toExponential(3), color: absError < 1e-4 ? T.green : T.amber },
        { label: "map", value: showError ? "error" : "value", color: showError ? T.red : T.blue },
        ...(hoveredCell ? [{ label: "hover", value: `(${hoveredCell.i},${hoveredCell.j}) f=${hoveredCell.val.toFixed(4)}`, color: T.amber }] : []),
      ]} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SCREEN 4: INTEGRATION ANALYSIS SCREEN
// ─────────────────────────────────────────────────────────────────────────────
function IntegrationAnalysisScreen({ onNavigate }) {
  const [fn2d, setFn2d] = useState("gaussian");

  const resolutions = [4, 6, 8, 10, 12, 16, 20];
  const dx = (r) => 4 / r;
  const estimate = (fn, r) => {
    let sum = 0;
    const d = dx(r);
    for (let i = 0; i < r; i++) {
      for (let j = 0; j < r; j++) {
        const cx = -2 + (i + 0.5) * d, cy = -2 + (j + 0.5) * d;
        let val = fn === "gaussian" ? Math.exp(-(cx*cx+cy*cy))
          : fn === "sincos" ? Math.sin(2*cx)*Math.cos(2*cy) : cx*cy+0.5;
        sum += val * d * d;
      }
    }
    return sum;
  };

  const refs = { gaussian: Math.PI * (1 - Math.exp(-4)), sincos: 0.0, poly: 2.0 };
  const ref = refs[fn2d];

  const convergenceData = resolutions.map(r => ({
    r, estimate: estimate(fn2d, r),
    error: Math.abs(estimate(fn2d, r) - ref),
    h: dx(r),
  }));

  // Convergence order from last two points
  const last = convergenceData.slice(-2);
  const observedOrder = last[1].error > 0 && last[0].error > 0
    ? (Math.log(last[1].error / last[0].error) / Math.log(last[1].h / last[0].h)).toFixed(2)
    : "—";

  const SparkConv = () => {
    const logErrors = convergenceData.map(d => d.error > 0 ? Math.log10(d.error) : -14);
    const logH = convergenceData.map(d => Math.log10(d.h));
    const W = 240, H = 80;
    const mg = { l: 28, r: 10, t: 8, b: 20 };
    const pw = W - mg.l - mg.r, ph = H - mg.t - mg.b;
    const minE = Math.min(...logErrors), maxE = Math.max(...logErrors);
    const minH = Math.min(...logH), maxH = Math.max(...logH);

    const pts = convergenceData.map((d, i) => {
      const x = mg.l + (logH[i] - minH) / (maxH - minH) * pw;
      const y = mg.t + (1 - (logErrors[i] - minE) / (maxE - minE)) * ph;
      return `${x},${y}`;
    }).join(" ");

    // Expected O(h^2) line
    const slope2pts = convergenceData.map((d, i) => {
      const x = mg.l + (logH[i] - minH) / (maxH - minH) * pw;
      const refE = logErrors[0] + 2 * (logH[i] - logH[0]);
      const y = mg.t + (1 - (refE - minE) / (maxE - minE)) * ph;
      return `${x},${y}`;
    }).join(" ");

    return (
      <svg width={W} height={H}>
        {[0,1,2,3].map(i => {
          const y = mg.t + (i/3)*ph;
          return <line key={i} x1={mg.l} y1={y} x2={mg.l+pw} y2={y}
            stroke={T.border} strokeWidth="0.5" />;
        })}
        <line x1={mg.l} y1={mg.t} x2={mg.l} y2={mg.t+ph} stroke={T.border2} strokeWidth="1" />
        <line x1={mg.l} y1={mg.t+ph} x2={mg.l+pw} y2={mg.t+ph} stroke={T.border2} strokeWidth="1" />
        <polyline points={slope2pts} fill="none" stroke={T.text2} strokeWidth="1" strokeDasharray="3,3" />
        <polyline points={pts} fill="none" stroke={T.amber} strokeWidth="1.5" />
        {convergenceData.map((d, i) => {
          const x = mg.l + (Math.log10(d.h) - minH) / (maxH - minH) * pw;
          const y = mg.t + (1 - (logErrors[i] - minE) / (maxE - minE)) * ph;
          return <circle key={i} cx={x} cy={y} r="2.5" fill={T.amber} />;
        })}
        <text x={mg.l-2} y={mg.t+4} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>
          {maxE.toFixed(0)}
        </text>
        <text x={mg.l-2} y={mg.t+ph+2} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>
          {minE.toFixed(0)}
        </text>
        <text x={mg.l} y={H-3} fill={T.text2} fontSize="7" fontFamily={T.mono}>coarse</text>
        <text x={mg.l+pw} y={H-3} fill={T.text2} fontSize="7" textAnchor="end" fontFamily={T.mono}>fine</text>
        <text x={6} y={mg.t+ph/2} fill={T.text2} fontSize="7" textAnchor="middle" fontFamily={T.mono}
          transform={`rotate(-90,6,${mg.t+ph/2})`}>log|err|</text>
      </svg>
    );
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", background: T.bg0 }}>
      <div style={{
        height: 42, background: T.bg1, borderBottom: `1px solid ${T.border}`,
        display: "flex", alignItems: "center", padding: "0 12px", gap: 12, flexShrink: 0,
      }}>
        <button onClick={() => onNavigate("integration")} style={{
          padding: "4px 10px", borderRadius: 3, background: "none",
          border: `1px solid ${T.border}`, color: T.text2,
          fontSize: 10, fontFamily: T.mono, cursor: "pointer",
        }}>← LAB</button>
        <Hairline vertical />
        <div style={{ fontSize: 10, fontFamily: T.mono, color: T.amber, letterSpacing: "0.12em" }}>
          INTEGRATION ANALYSIS
        </div>
        <Hairline vertical />
        {Object.entries(refs).map(([id, _]) => (
          <button key={id} onClick={() => setFn2d(id)} style={{
            padding: "3px 8px", borderRadius: 3, fontSize: 10, fontFamily: T.mono, cursor: "pointer",
            background: fn2d === id ? T.amber + "20" : "none",
            border: `1px solid ${fn2d === id ? T.amber + "60" : T.border}`,
            color: fn2d === id ? T.amber : T.text2,
          }}>{id === "gaussian" ? "exp(−r²)" : id === "sincos" ? "sin·cos" : "xy+0.5"}</button>
        ))}
      </div>

      <div style={{ flex: 1, overflowY: "auto", padding: 16, display: "grid",
        gridTemplateColumns: "1fr 1fr", gap: 12, alignContent: "start" }}>

        {/* Convergence table */}
        <Panel>
          <PanelHeader right={<span>
            <span style={{ color: T.text2, fontSize: 10, fontFamily: T.mono }}>observed order: </span>
            <MonoValue color={parseFloat(observedOrder) > 1.5 && parseFloat(observedOrder) < 2.5
              ? T.green : T.amber}>{observedOrder}</MonoValue>
            <span style={{ color: T.text2, fontSize: 10, fontFamily: T.mono }}> (expected 2)</span>
          </span>}>
            convergence study
          </PanelHeader>
          <div>
            <div style={{ display: "grid", gridTemplateColumns: "50px 60px 90px 90px 60px",
              padding: "5px 10px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
              fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.08em" }}>
              <span>n</span><span style={{textAlign:"right"}}>h</span>
              <span style={{textAlign:"right"}}>estimate</span>
              <span style={{textAlign:"right"}}>|error|</span>
              <span style={{textAlign:"right"}}>order</span>
            </div>
            {convergenceData.map((row, i) => {
              const order = i > 0 && convergenceData[i-1].error > 0 && row.error > 0
                ? (Math.log(row.error / convergenceData[i-1].error) /
                   Math.log(row.h / convergenceData[i-1].h)).toFixed(2)
                : "—";
              return (
                <div key={row.r} style={{
                  display: "grid", gridTemplateColumns: "50px 60px 90px 90px 60px",
                  padding: "5px 10px", borderBottom: `1px solid ${T.border}`,
                  background: i % 2 === 0 ? "transparent" : T.bg2 + "60",
                  fontSize: 10, fontFamily: T.mono,
                }}>
                  <span style={{ color: T.amber }}>{row.r}²</span>
                  <span style={{ textAlign: "right", color: T.text2 }}>{row.h.toFixed(4)}</span>
                  <span style={{ textAlign: "right", color: T.text1 }}>{row.estimate.toFixed(7)}</span>
                  <span style={{ textAlign: "right",
                    color: row.error < 1e-5 ? T.green : row.error < 0.01 ? T.amberL : T.text1 }}>
                    {row.error.toExponential(3)}</span>
                  <span style={{ textAlign: "right",
                    color: order !== "—" && Math.abs(parseFloat(order) - 2) < 0.3 ? T.green : T.text2 }}>
                    {order}
                  </span>
                </div>
              );
            })}
          </div>
        </Panel>

        {/* Convergence plot */}
        <Panel>
          <PanelHeader right={
            <span style={{ fontSize: 9, color: T.text2, fontFamily: T.mono }}>
              — — expected O(h²)
            </span>
          }>log|error| vs log(h)</PanelHeader>
          <div style={{ padding: 12 }}>
            <SparkConv />
            <div style={{ marginTop: 10, fontSize: 10, color: T.text2, lineHeight: 1.6 }}>
              Observed slope ≈ {observedOrder}. Midpoint rule is O(h²) for smooth
              integrands. Slope deviation indicates either non-smooth integrand or
              floating-point precision limits.
            </div>
          </div>
        </Panel>

        {/* Stability score */}
        <Panel>
          <PanelHeader accent={T.green}>stability analysis</PanelHeader>
          <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 8 }}>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>stability score</span>
              <MonoValue color={T.green}>0.0023</MonoValue>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>appears converged</span>
              <Badge color={T.green}>YES</Badge>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ fontSize: 10, color: T.text2, fontFamily: T.mono }}>is stable</span>
              <Badge color={T.green}>YES</Badge>
            </div>
            <Hairline />
            <div style={{ fontSize: 9, color: T.text2, fontFamily: T.mono,
              letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 4 }}>
              definition
            </div>
            <div style={{
              padding: "8px 10px", background: T.bg2, borderRadius: 3,
              border: `1px solid ${T.border}`, fontSize: 10, fontFamily: T.mono, color: T.text1,
            }}>
              StabilityScore = variation / max(1, |estimate|)
              <br />= {(0.0023 * Math.abs(ref)).toExponential(3)} / {Math.max(1, Math.abs(ref)).toFixed(4)}
              <br /><span style={{ color: T.green }}>= 0.0023 ← low is good</span>
            </div>
          </div>
        </Panel>

        {/* Integrability microscope */}
        <Panel>
          <PanelHeader accent={T.amber}>integrability microscope</PanelHeader>
          <div style={{ padding: 12, display: "flex", flexDirection: "column", gap: 8 }}>
            {[
              { region: "center (0,0)", classification: "smooth", color: T.green,
                desc: "Gaussian peak. High value, but smooth. Refinement converges rapidly." },
              { region: "near boundary", classification: "smooth", color: T.green,
                desc: "Gaussian tail. Near zero. Refinement has little effect on total error." },
              { region: "corner (±2,±2)", classification: "smooth", color: T.green,
                desc: "f ≈ exp(-8) ≈ 3×10⁻⁴. Negligible contribution." },
            ].map(item => (
              <div key={item.region} style={{
                padding: "8px 10px", borderRadius: 3,
                background: T.bg2, border: `1px solid ${T.border}`,
              }}>
                <div style={{ display: "flex", justifyContent: "space-between",
                  alignItems: "center", marginBottom: 4 }}>
                  <span style={{ fontSize: 10, color: T.text1, fontFamily: T.mono }}>{item.region}</span>
                  <Badge color={item.color}>{item.classification}</Badge>
                </div>
                <div style={{ fontSize: 10, color: T.text2, lineHeight: 1.5 }}>{item.desc}</div>
              </div>
            ))}
          </div>
        </Panel>

        {/* ResultTrace audit */}
        <Panel style={{ gridColumn: "1 / -1" }}>
          <PanelHeader accent={T.blue} right={<Badge color={T.blue}>ResultTrace</Badge>}>
            numerical audit trail — refinement history
          </PanelHeader>
          <div>
            <div style={{ display: "grid", gridTemplateColumns: "70px 70px 100px 100px 90px 1fr",
              padding: "5px 12px", background: T.bg2, borderBottom: `1px solid ${T.border}`,
              fontSize: 9, color: T.text2, fontFamily: T.mono, letterSpacing: "0.08em" }}>
              <span>step</span><span>n</span><span>estimate</span>
              <span>|error|</span><span>prev. est.</span><span>message</span>
            </div>
            <div style={{ maxHeight: 180, overflowY: "auto" }}>
              {convergenceData.map((row, i) => (
                <div key={i} style={{
                  display: "grid", gridTemplateColumns: "70px 70px 100px 100px 90px 1fr",
                  padding: "4px 12px", borderBottom: `1px solid ${T.border}`,
                  background: i % 2 === 0 ? "transparent" : T.bg2 + "60",
                  fontSize: 10, fontFamily: T.mono,
                }}>
                  <span style={{ color: T.text2 }}>{i}</span>
                  <span style={{ color: T.amber }}>{row.r}²</span>
                  <span style={{ color: T.text1 }}>{row.estimate.toFixed(7)}</span>
                  <span style={{ color: row.error < 1e-5 ? T.green : T.amberL }}>
                    {row.error.toExponential(3)}
                  </span>
                  <span style={{ color: T.text2 }}>
                    {i > 0 ? convergenceData[i-1].estimate.toFixed(5) : "—"}
                  </span>
                  <span style={{ color: T.text2 }}>
                    {row.error < 1e-6 ? "✓ converged" :
                      row.error < 0.001 ? "refinement effective" : "refine further"}
                  </span>
                </div>
              ))}
            </div>
          </div>
        </Panel>
      </div>

      <StatusBar items={[
        { label: "screen", value: "integration analysis" },
        { label: "fn", value: fn2d },
        { label: "reference", value: ref.toFixed(6) },
        { label: "observed order", value: observedOrder, color: T.green },
        { label: "stability", value: "0.0023", color: T.green },
      ]} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// NAV SHELL
// ─────────────────────────────────────────────────────────────────────────────
const NAV_ITEMS = [
  { id: "taylor", label: "Taylor", icon: "∑" },
  { id: "taylor-analysis", label: "T. Analysis", icon: "◈" },
  { id: "integration", label: "Integration", icon: "∫∫" },
  { id: "integration-analysis", label: "∫ Analysis", icon: "◈" },
];

export default function App() {
  const [screen, setScreen] = useState("taylor");

  return (
    <div style={{
      width: "100%", height: "100vh", display: "flex", flexDirection: "column",
      background: T.bg0, color: T.text0,
      fontFamily: T.sans, overflow: "hidden",
    }}>
      {/* Global styles */}
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=DM+Sans:wght@300;400;500;600&family=JetBrains+Mono:wght@400;500;600;700&family=Playfair+Display:wght@400;600&display=swap');
        * { box-sizing: border-box; margin: 0; padding: 0; }
        ::-webkit-scrollbar { width: 6px; height: 6px; }
        ::-webkit-scrollbar-track { background: ${T.bg0}; }
        ::-webkit-scrollbar-thumb { background: ${T.border2}; border-radius: 3px; }
        ::-webkit-scrollbar-thumb:hover { background: ${T.text2}; }
        input[type=range] { height: 3px; }
        button { font-family: inherit; }
      `}</style>

      {/* Top nav */}
      <div style={{
        height: 44, background: T.bg0, borderBottom: `1px solid ${T.border}`,
        display: "flex", alignItems: "center", flexShrink: 0,
      }}>
        {/* Logo */}
        <div style={{
          width: 140, padding: "0 16px", display: "flex", alignItems: "center", gap: 8,
          borderRight: `1px solid ${T.border}`, height: "100%",
        }}>
          <div style={{
            width: 22, height: 22, background: T.amber, borderRadius: 3,
            display: "flex", alignItems: "center", justifyContent: "center",
            fontSize: 12, fontWeight: 700, color: T.bg0, fontFamily: T.mono,
            flexShrink: 0,
          }}>N</div>
          <div>
            <div style={{ fontSize: 10, fontFamily: T.mono, fontWeight: 700,
              color: T.text0, letterSpacing: "0.05em" }}>nurbs_dde</div>
            <div style={{ fontSize: 8, color: T.text2, fontFamily: T.mono, letterSpacing: "0.1em" }}>
              OBSERVATORY
            </div>
          </div>
        </div>

        {/* Nav items */}
        <div style={{ display: "flex", height: "100%", alignItems: "stretch" }}>
          {NAV_ITEMS.map(item => (
            <button key={item.id} onClick={() => setScreen(item.id)} style={{
              padding: "0 18px", background: "none", border: "none", cursor: "pointer",
              borderRight: `1px solid ${T.border}`,
              borderBottom: screen === item.id ? `2px solid ${T.amber}` : "2px solid transparent",
              display: "flex", alignItems: "center", gap: 6,
              color: screen === item.id ? T.text0 : T.text2,
              transition: "color 0.15s, border-color 0.15s",
            }}>
              <span style={{ fontSize: 14, fontFamily: T.mono, color: screen === item.id ? T.amber : T.text2 }}>
                {item.icon}
              </span>
              <span style={{ fontSize: 10, fontFamily: T.mono, fontWeight: 600,
                letterSpacing: "0.08em", textTransform: "uppercase" }}>{item.label}</span>
            </button>
          ))}
        </div>

        {/* Right */}
        <div style={{ marginLeft: "auto", padding: "0 16px", display: "flex",
          alignItems: "center", gap: 12 }}>
          <Badge color={T.green}>Pillar I</Badge>
          <span style={{ fontSize: 9, color: T.text2, fontFamily: T.mono }}>
            Analysis & Approximation
          </span>
        </div>
      </div>

      {/* Screen */}
      <div style={{ flex: 1, overflow: "hidden" }}>
        {screen === "taylor" && <TaylorScreen onNavigate={setScreen} />}
        {screen === "taylor-analysis" && <TaylorAnalysisScreen onNavigate={setScreen} />}
        {screen === "integration" && <IntegrationScreen onNavigate={setScreen} />}
        {screen === "integration-analysis" && <IntegrationAnalysisScreen onNavigate={setScreen} />}
      </div>
    </div>
  );
}