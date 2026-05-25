// ============================================================================
// 履带车视觉跟随系统 — Web Dashboard (embedded SPA)
// 实时显示: motor PWM / vision detection / distScore / tofDistance / STM32 遥测
// ============================================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>6轮车 Dashboard</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, sans-serif; background: #1a1a2e; color: #eee;
       max-width: 480px; margin: 0 auto; padding: 8px; }
header { display: flex; justify-content: space-between; align-items: center;
         background: #16213e; padding: 10px 14px; border-radius: 8px; margin-bottom: 8px; }
header h1 { font-size: 15px; color: #00d4ff; }
.snap-btn { background: #00d4ff; color: #1a1a2e; border: none; padding: 6px 14px;
            border-radius: 6px; font-weight: bold; cursor: pointer; font-size: 13px; }
.snap-btn:active { background: #00a8cc; }
.status-bar { display: flex; gap: 12px; background: #16213e; padding: 8px 14px;
              border-radius: 8px; margin-bottom: 8px; font-size: 13px; }
.status-item { display: flex; gap: 6px; align-items: center; }
.badge { padding: 2px 8px; border-radius: 4px; font-size: 12px; font-weight: bold; }
.badge-ok { background: #00c853; color: #fff; }
.badge-warn { background: #ffc107; color: #000; }
.badge-error { background: #f44336; color: #fff; }
.badge-stop { background: #f44336; color: #fff; animation: blink 0.5s infinite; }
@keyframes blink { 50% { opacity: 0.5; } }
.card { background: #16213e; border-radius: 8px; padding: 10px 14px; margin-bottom: 8px; }
.card-title { font-size: 11px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
.us-bars { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.card-item { background: #0f3460; border-radius: 6px; padding: 8px; text-align: center; }
.us-label { font-size: 11px; color: #aaa; margin-bottom: 4px; }
.us-val { font-size: 22px; font-weight: bold; margin-bottom: 4px; }
.us-bar { height: 6px; background: #333; border-radius: 3px; overflow: hidden; }
.us-bar-fill { height: 100%; border-radius: 3px; transition: width 0.3s, background 0.3s; }
.canvas-wrap { background: #0a0a1a; border-radius: 8px; padding: 8px; text-align: center; }
canvas { max-width: 100%; border-radius: 4px; background: #0a0a1a; }
.canvas-note { font-size: 10px; color: #555; margin-top: 4px; }
.motor-bars { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.motor-label { font-size: 11px; color: #aaa; margin-bottom: 4px; }
.motor-val { font-size: 16px; font-weight: bold; margin-bottom: 4px; }
.motor-bar { height: 6px; background: #333; border-radius: 3px; overflow: hidden; }
.motor-bar-fill { height: 100%; background: #00d4ff; border-radius: 3px; transition: width 0.3s; }
.dist-score-wrap { display: flex; align-items: center; gap: 8px; margin-top: 6px; }
.dist-label { font-size: 11px; color: #aaa; }
.dist-bar { flex: 1; height: 8px; background: #333; border-radius: 4px; overflow: hidden; }
.dist-fill { height: 100%; border-radius: 4px; transition: width 0.2s, background 0.2s; }
.dist-val { font-size: 12px; font-weight: bold; min-width: 40px; text-align: right; }
footer { text-align: center; font-size: 10px; color: #444; margin-top: 8px; }
.snap-loading { color: #ffc107; font-size: 12px; }
</style>
</head>
<body>

<header>
  <h1>🚗 6轮车 Dashboard</h1>
  <button class="snap-btn" onclick="requestSnapshot()">📷 Snapshot</button>
</header>

<div class="status-bar">
  <div class="status-item">
    Action: <span id="action" class="badge badge-ok">--</span>
  </div>
  <div class="status-item">
    ESP32: <span id="car-status" class="badge badge-ok">✓</span>
  </div>
  <div class="status-item">
    OpenMV: <span id="vis-status" class="badge badge-ok">✓</span>
  </div>
</div>

<div class="card">
  <div class="card-title">Obstacle Distance</div>
  <div class="us-bars">
    <div class="card-item">
      <div class="us-label">Left US</div>
      <div class="us-val" id="ul-val">--</div>
      <div class="us-bar"><div class="us-bar-fill" id="ul-bar" style="width:0%"></div></div>
    </div>
    <div class="card-item">
      <div class="us-label">Right US</div>
      <div class="us-val" id="ur-val">--</div>
      <div class="us-bar"><div class="us-bar-fill" id="ur-bar" style="width:0%"></div></div>
    </div>
  </div>
</div>

<div class="card">
  <div class="card-title">Top View <span style="font-size:10px;color:#555">(front at bottom)</span></div>
  <div class="canvas-wrap">
    <canvas id="topview"></canvas>
    <div class="canvas-note">Top=far(>3m) · Bottom=near(<1m) · Box color: far=green/med=yellow/near=red</div>
  </div>
</div>

<div class="card">
  <div class="card-title">Distance Score</div>
  <div class="dist-score-wrap">
    <span class="dist-label">Near</span>
    <div class="dist-bar"><div class="dist-fill" id="dist-fill" style="width:0%"></div></div>
    <span class="dist-val" id="dist-val">--</span>
    <span class="dist-label">Far</span>
  </div>
</div>

<div class="card">
  <div class="card-title">Motor Status</div>
  <div class="motor-bars">
    <div class="card-item">
      <div class="motor-label">Left Motor</div>
      <div class="motor-val" id="mot-l">--</div>
      <div class="motor-bar"><div class="motor-bar-fill" id="mot-l-bar" style="width:0%"></div></div>
    </div>
    <div class="card-item">
      <div class="motor-label">Right Motor</div>
      <div class="motor-val" id="mot-r">--</div>
      <div class="motor-bar"><div class="motor-bar-fill" id="mot-r-bar" style="width:0%"></div></div>
    </div>
  </div>
</div>

<footer id="footer">Last update: --</footer>

<script>
// ============================================================================
// Canvas and context (dimensions read from HTML, not hardcoded)
// ============================================================================
const canvas = document.getElementById('topview');
const ctx = canvas.getContext('2d');
const W = canvas.width || 320;
const H = canvas.height || 240;

// Thresholds from ESP32 (dynamically loaded from /status)
const config = {
  ds_stop: 0.85,
  ds_slow: 0.65,
  ds_far: 0.30
};

// ============================================================================
// Draw background grid
// ============================================================================
function drawGrid() {
  ctx.strokeStyle = '#222';
  ctx.lineWidth = 1;
  for (let x = 0; x < W; x += 40) {
    ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,H); ctx.stroke();
  }
  for (let y = 0; y < H; y += 40) {
    ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke();
  }
  ctx.strokeStyle = '#333';
  ctx.setLineDash([4,4]);
  ctx.beginPath(); ctx.moveTo(W/2, 0); ctx.lineTo(W/2, H); ctx.stroke();
  ctx.setLineDash([]);
}

// ============================================================================
// Draw car body (at bottom center)
// ============================================================================
function drawCar() {
  ctx.fillStyle = '#00d4ff';
  ctx.fillRect(W/2-20, H-30, 40, 20);
  ctx.fillStyle = '#fff';
  ctx.font = '9px monospace';
  ctx.textAlign = 'center';
  ctx.fillText('Front', W/2, H-14);
}

// ============================================================================
// Draw detection box from OpenMV
// Maps 192x192 model coords to canvas dimensions
// ============================================================================
function drawDetection(vis) {
  if (!vis.v || !vis.hp) return;

  // Map OpenMV coords (192x192) to canvas (W x H)
  const dvx = vis.cx * (W / 192.0);
  const dvy = H - vis.cy * (H / 192.0);
  const dw = vis.w * (W / 192.0);
  const dh = vis.h * (H / 192.0);

  // Color by dist_score
  const ds = vis.ds || 0;
  let boxColor;
  if (ds >= config.ds_stop) boxColor = '#f44336';
  else if (ds >= config.ds_slow) boxColor = '#ffc107';
  else if (ds >= config.ds_far) boxColor = '#00c853';
  else boxColor = '#4caf50';

  // Draw box
  ctx.strokeStyle = boxColor;
  ctx.lineWidth = 2;
  ctx.strokeRect(dvx - dw/2, dvy - dh/2, dw, dh);

  // Center dot
  ctx.fillStyle = boxColor;
  ctx.beginPath();
  ctx.arc(dvx, dvy, 4, 0, Math.PI*2);
  ctx.fill();

  // Label
  ctx.fillStyle = boxColor;
  ctx.font = 'bold 10px monospace';
  ctx.textAlign = 'center';
  ctx.fillText(vis.type + ' ' + vis.conf.toFixed(2), dvx, dvy - dh/2 - 4);

  // Distance estimate
  let distM = '?';
  if (ds >= config.ds_stop) distM = '<0.5m';
  else if (ds >= config.ds_slow) distM = '0.5-1m';
  else if (ds >= config.ds_far) distM = '1-3m';
  else distM = '>3m';
  ctx.fillStyle = '#aaa';
  ctx.font = '9px monospace';
  ctx.fillText(distM, dvx, dvy + dh/2 + 12);
}

// ============================================================================
// ToF distance indicator (VL53L1X, replaced ultrasonic)
// ============================================================================
function drawTofDistance(vis) {
  if (!vis.v) return;
  const tof = vis.tof || 0;
  if (tof <= 0) return;
  const tofM = (tof / 1000).toFixed(2);
  ctx.fillStyle = '#0ff';
  ctx.font = '9px monospace';
  ctx.textAlign = 'right';
  ctx.fillText('ToF: ' + tofM + 'm', W - 4, 20);
}

// ============================================================================
// Compose topview from sub-components
// ============================================================================
function drawTopview(car, vis) {
  ctx.clearRect(0, 0, W, H);
  drawGrid();
  drawCar();
  drawDetection(vis);
  drawTofDistance(vis);
}

// ============================================================================
// Utility: cm → color
// ============================================================================
function distColor(cm) {
  if (cm <= 0 || cm > 400) return '#555';
  if (cm < 20) return '#f44336';
  if (cm < 40) return '#ffc107';
  return '#00c853';
}

// ============================================================================
// Utility: score → color
// ============================================================================
function scoreColor(s) {
  if (s >= config.ds_stop) return '#f44336';
  if (s >= config.ds_slow) return '#ffc107';
  if (s >= config.ds_far) return '#00c853';
  return '#00c853';
}

// ============================================================================
// Update all UI elements from JSON data
// ============================================================================
function updateUI(data) {
  // Sync thresholds from ESP32
  if (data.cfg) {
    config.ds_stop = data.cfg.ds_stop;
    config.ds_slow = data.cfg.ds_slow;
    config.ds_far = data.cfg.ds_far;
  }

  const car = data.car;
  const vis = data.vis;

  // Status badges
  if (!car.v) {
    document.getElementById('car-status').className = 'badge badge-error';
    document.getElementById('car-status').textContent = '✗';
  } else {
    document.getElementById('car-status').className = 'badge badge-ok';
    document.getElementById('car-status').textContent = '✓';
  }

  if (!vis.v) {
    document.getElementById('vis-status').className = 'badge badge-error';
    document.getElementById('vis-status').textContent = '✗';
  } else {
    document.getElementById('vis-status').className = 'badge badge-ok';
    document.getElementById('vis-status').textContent = '✓';
  }

  // Action badge
  const actEl = document.getElementById('action');
  if (car.v) {
    actEl.textContent = car.act || '--';
    actEl.className = car.act === 'STOP' ? 'badge badge-stop' : 'badge badge-ok';
  }

  // Ultrasonic
  if (car.v) {
    document.getElementById('ul-val').textContent = car.ul + 'cm';
    document.getElementById('ur-val').textContent = car.ur + 'cm';
    // ToF distance display (VL53L1X, mm)
    const tofEl = document.getElementById('tof-val');
    if (tofEl && vis.tof > 0) {
      tofEl.textContent = (vis.tof / 1000).toFixed(2) + 'm';
    } else if (tofEl) {
      tofEl.textContent = '--';
    }
  }

  // Motors
  if (car.v) {
    document.getElementById('mot-l').textContent = car.l;
    document.getElementById('mot-r').textContent = car.r;
    document.getElementById('mot-l-bar').style.width = (car.l/255*100) + '%';
    document.getElementById('mot-r-bar').style.width = (car.r/255*100) + '%';
  }

  // Topview canvas
  drawTopview(car, vis);

  // Distance score bar
  if (vis.v && vis.hp) {
    const ds = vis.ds || 0;
    const dsPct = Math.min(100, ds * 100);
    document.getElementById('dist-fill').style.width = dsPct + '%';
    document.getElementById('dist-fill').style.background = scoreColor(ds);
    document.getElementById('dist-val').textContent = ds.toFixed(2);
  }

  // Footer timestamp
  document.getElementById('footer').textContent = 'Last update: ' + new Date().toLocaleTimeString();
}

// ============================================================================
// Poll /status every 200ms
// ============================================================================
function poll() {
  fetch('/status')
    .then(r => r.json())
    .then(updateUI)
    .catch(() => {
      document.getElementById('car-status').className = 'badge badge-error';
      document.getElementById('car-status').textContent = '✗';
    });
}

// ============================================================================
// Snapshot button (reserved for future use)
// ============================================================================
function requestSnapshot() {
  const btn = document.querySelector('.snap-btn');
  btn.textContent = '🚧 WIP';
  setTimeout(() => { btn.textContent = '📷 Snapshot'; }, 2000);
}

setInterval(poll, 200);
</script>
</body>
</html>
)rawliteral";