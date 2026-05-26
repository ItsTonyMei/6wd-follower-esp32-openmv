// ============================================================================
// 履带车视觉跟随系统 — 手机端 Web Dashboard (Phase 0.2)
// 深色主题, 200ms 轮询 /status, 无外部依赖
// ============================================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=no">
<title>履带车跟随</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,'Segoe UI',sans-serif;background:#0f1117;color:#c9d1d9;
     max-width:420px;margin:0 auto;padding:10px 10px 30px}
h2{font-size:14px;color:#58a6ff;margin:14px 0 6px;padding-bottom:4px;border-bottom:1px solid #21262d}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:12px;margin-bottom:8px}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px}
.col{flex:1}
.lbl{font-size:10px;color:#8b949e;text-transform:uppercase;letter-spacing:.5px}
.val{font-size:16px;font-weight:bold;color:#e6ed3f}
.val-big{font-size:28px;font-weight:bold}
.badge{display:inline-block;padding:3px 10px;border-radius:10px;font-size:13px;font-weight:bold}
.ok{background:#238636;color:#fff}
.warn{background:#d2991d;color:#000}
.err{background:#da3633;color:#fff}
.idle{background:#30363d;color:#8b949e}
.bar-wrap{height:10px;background:#21262d;border-radius:5px;overflow:hidden;margin:4px 0}
.bar-fill{height:100%;border-radius:5px;transition:width .3s,background .3s}
.r{background:#da3633}.y{background:#d2991d}.g{background:#238636}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.footer{text-align:center;font-size:10px;color:#484f58;margin-top:8px}
.mono{font-family:'SF Mono',Consolas,monospace;font-size:13px}
</style>
</head>
<body>

<!-- Header -->
<div class="card" style="display:flex;justify-content:space-between;align-items:center">
  <div>
    <div style="font-size:18px;font-weight:bold;color:#58a6ff">履带车跟随系统</div>
    <div class="lbl">Tracked Vehicle Follower</div>
  </div>
  <div style="text-align:right">
    <span id="wifi-badge" class="badge idle">WiFi</span>
    <div class="lbl" style="margin-top:2px" id="uptime">--</div>
  </div>
</div>

<!-- 视觉感知 -->
<h2>视觉感知</h2>
<div class="card">
  <div class="row">
    <div>
      <div class="lbl">检测</div>
      <span id="vis-badge" class="badge idle" style="font-size:15px">等待数据</span>
    </div>
    <div style="text-align:right">
      <div class="lbl">置信度</div>
      <div class="val" id="vis-conf">--</div>
    </div>
  </div>
  <div style="margin-top:10px">
    <div class="lbl">距离估计</div>
    <div class="row" style="align-items:baseline">
      <div class="val-big" id="vis-dist">--</div>
      <div class="lbl" style="font-size:13px" id="vis-dist-tag"></div>
    </div>
    <div class="bar-wrap"><div class="bar-fill" id="dist-bar" style="width:0%"></div></div>
    <div class="row" style="margin-top:2px"><span class="lbl">近</span><span class="lbl">远</span></div>
  </div>
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">ToF 测距</div><div class="val" id="vis-tof">--</div></div>
    <div><div class="lbl">检测框</div><div class="val" id="vis-box">--</div></div>
  </div>
</div>

<!-- 车辆状态 -->
<h2>车辆状态</h2>
<div class="card">
  <div class="row">
    <div>
      <div class="lbl">动作</div>
      <span id="car-act" class="badge idle" style="font-size:16px">STOP</span>
    </div>
    <div style="text-align:right">
      <div class="lbl">数据</div>
      <span id="car-ok" class="badge idle">--</span>
    </div>
  </div>
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">左 PWM</div><div class="val" id="car-l">0</div></div>
    <div><div class="lbl">右 PWM</div><div class="val" id="car-r">0</div></div>
  </div>
</div>

<h2>系统</h2>
<div class="card">
  <div class="grid2">
    <div><div class="lbl">运行时间</div><div class="val" id="sys-uptime">--</div></div>
    <div><div class="lbl">WiFi 客户端</div><div class="val" id="sys-clients">--</div></div>
  </div>
</div>

<div class="footer" id="footer">正在连接...</div>

<script>
let fail = 0;
function barColor(ds, cfg) {
  if (ds >= (cfg.ds_stop||0.85)) return 'r';
  if (ds >= (cfg.ds_slow||0.65)) return 'y';
  return 'g';
}
function distTag(ds, cfg) {
  if (ds >= (cfg.ds_stop||0.85)) return '太近';
  if (ds >= (cfg.ds_slow||0.65)) return '较近';
  if (ds >= (cfg.ds_far||0.30)) return '适中';
  return '较远';
}

function update() {
  fetch('/status').then(r=>r.json()).then(d=>{
    fail=0;
    let v=d.vis||{}, c=d.car||{}, cfg=d.cfg||{}, sys=d.sys||{};

    // 视觉
    let vb=document.getElementById('vis-badge');
    if(v.v && v.hp){ vb.textContent='有人 PERSON'; vb.className='badge ok'; }
    else if(v.v){ vb.textContent='无人 SCAN'; vb.className='badge warn'; }
    else { vb.textContent='等待数据'; vb.className='badge idle'; }
    document.getElementById('vis-conf').textContent=v.hp?(v.conf*100).toFixed(0)+'%':'--';
    document.getElementById('vis-tof').textContent=v.tof>0?(v.tof/1000).toFixed(2)+'m':'--';
    let ds=v.ds||0, pct=Math.min(100,Math.max(0,ds*100));
    let bar=document.getElementById('dist-bar');
    bar.style.width=pct+'%'; bar.className='bar-fill '+barColor(ds,cfg);
    document.getElementById('vis-dist').textContent=ds.toFixed(2);
    document.getElementById('vis-dist-tag').textContent=distTag(ds,cfg);
    document.getElementById('vis-box').textContent=v.hp?v.w+'x'+v.h+' @('+v.cx+','+v.cy+')':'--';

    // 车辆
    let act=(c.act||'STOP'), ae=document.getElementById('car-act');
    ae.textContent=act;
    ae.className='badge '+(act==='STOP'?'idle':act.indexOf('AVD')>=0?'warn':'ok');
    document.getElementById('car-ok').textContent=c.v?'OK':'--';
    document.getElementById('car-ok').className='badge '+(c.v?'ok':'idle');
    document.getElementById('car-l').textContent=c.l||0;
    document.getElementById('car-r').textContent=c.r||0;

    // 系统
    let up=sys.uptime||0, m=Math.floor(up/60000), s=Math.floor((up%60000)/1000);
    document.getElementById('sys-uptime').textContent=m+'m '+s+'s';
    document.getElementById('sys-clients').textContent=(sys.wifi_clients||0)+' 个';
    document.getElementById('uptime').textContent='运行 '+m+'m'+s+'s';
    document.getElementById('footer').textContent='更新: '+new Date().toLocaleTimeString();
  }).catch(()=>{
    fail++;
    document.getElementById('footer').textContent='失败 x'+fail+' '+new Date().toLocaleTimeString();
  });
}
update(); setInterval(update, 200);
</script>
</body>
</html>
)rawliteral";
