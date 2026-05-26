// ============================================================================
// 履带车视觉跟随系统 — ESP8266 Bridge (Phase 0 VIS + Dashboard)
// MCU: ESP8266 NodeMCU V3 (ESP-12E), 80MHz
// VIS 接收: D5(GPIO14) SoftwareSerial @ 4800 ← N6 P0 SW UART
// WiFi Dashboard: HTTP port 80
// ============================================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// ─── 配置 ───
const char* WIFI_SSID = "Tracked Robot";
const char* WIFI_PASS = "12345678";

// VIS 接收
SoftwareSerial visSerial(14, -1, false);  // RX=D5(GPIO14), no TX
char visLine[256];
int  visLen = 0;

// 最新 VIS 数据 (线程不安全, 单线程 ESP8266 无需锁)
struct {
    bool   valid = false;
    bool   hasPerson = false;
    int    cx = 0, cy = 0, w = 0, h = 0, feetY = 0;
    float  conf = 0, distScore = 0;
    int    tofDist = 0;
    unsigned long ts = 0;
} latestVis;

ESP8266WebServer server(80);

// ─── VIS 解析 ───
bool parseVisFrame(char* buf, int len) {
    if (len < 4 || strncmp(buf, "VIS:", 4) != 0) return false;
    char* p = buf + 4;
    char* end = nullptr;

    latestVis.cx     = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    latestVis.cy     = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    latestVis.w      = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    latestVis.h      = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    latestVis.feetY  = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    latestVis.conf   = strtof(p, &end);           if (*end != ',') return false; p = end + 1;

    // type: "PERSON" or "NONE"
    char* comma = strchr(p, ',');
    if (!comma) return false;
    *comma = '\0';
    latestVis.hasPerson = (strcmp(p, "PERSON") == 0);
    p = comma + 1;

    latestVis.distScore = strtof(p, &end);
    latestVis.tofDist = 0;
    if (*end == ',') {
        p = end + 1;
        latestVis.tofDist = (int)strtol(p, &end, 10);
    }

    latestVis.valid = true;
    latestVis.ts = millis();
    return true;
}

// ─── HTTP /status JSON ───
// VIS 帧统计
static unsigned long totalFrames = 0, okFrames = 0, failFrames = 0;
unsigned long updateFrameStats(unsigned long t, unsigned long o, unsigned long f) {
    totalFrames = t; okFrames = o; failFrames = f;
    return t;
}

void handleStatus() {
    unsigned long now = millis();
    unsigned long age = (latestVis.ts > 0) ? (now - latestVis.ts) : 999999;
    char json[640];
    snprintf(json, sizeof(json),
        "{\"vis\":{\"v\":%d,\"hp\":%d,\"cx\":%d,\"cy\":%d,\"w\":%d,\"h\":%d,"
        "\"conf\":%.2f,\"ds\":%.2f,\"tof\":%d,\"fy\":%d,\"age\":%lu,\"ts\":%lu},"
        "\"sys\":{\"uptime\":%lu,\"heap\":%u,\"wifi_clients\":%d},"
        "\"rx\":{\"total\":%lu,\"ok\":%lu,\"fail\":%lu}}",
        latestVis.valid, latestVis.hasPerson,
        latestVis.cx, latestVis.cy, latestVis.w, latestVis.h,
        latestVis.conf, latestVis.distScore, latestVis.tofDist, latestVis.feetY,
        age, latestVis.ts,
        now, ESP.getFreeHeap(), WiFi.softAPgetStationNum(),
        totalFrames, okFrames, failFrames);
    server.send(200, "application/json", json);
}

// ─── HTTP 首页 (加强版 Dashboard) ───
void handleRoot() {
    String html = F(R"raw(
<!DOCTYPE html><html lang="zh"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>履带车跟随</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#0f1117;color:#c9d1d9;max-width:440px;margin:0 auto;padding:10px 10px 40px}
h2{font-size:13px;color:#58a6ff;margin:14px 0 6px;padding-bottom:4px;border-bottom:1px solid #21262d}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:12px;margin-bottom:8px}
.lbl{font-size:10px;color:#8b949e;text-transform:uppercase;letter-spacing:.5px}
.val{font-size:17px;font-weight:bold;color:#e6edf3}
.badge{display:inline-block;padding:3px 10px;border-radius:10px;font-size:14px;font-weight:bold}
.ok{background:#238636;color:#fff}.warn{background:#d2991d;color:#000}.err{background:#da3633;color:#fff}
.idle{background:#30363d;color:#8b949e}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px}
.col{flex:1}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
.bar-wrap{height:10px;background:#21262d;border-radius:5px;overflow:hidden;margin:4px 0}
.bar-fill{height:100%;border-radius:5px;transition:width .3s,background .3s}
.r{background:#da3633}.y{background:#d2991d}.g{background:#238636}
.footer{text-align:center;font-size:10px;color:#484f58;margin-top:10px}
.mono{font-family:monospace;font-size:12px}
.tag{display:inline-block;font-size:9px;padding:1px 5px;border-radius:4px;margin-right:3px}
.tag-ok{background:#23863620;color:#238636}.tag-warn{background:#d2991d20;color:#d2991d}
</style></head><body>

<div class="card" style="display:flex;justify-content:space-between;align-items:center">
  <div><div style="font-size:18px;font-weight:bold;color:#58a6ff">履带车跟随</div>
  <div class="lbl">ESP8266 Bridge Phase 0</div></div>
  <div style="text-align:right">
    <div class="val" style="font-size:13px" id="sys-uptime">--</div>
    <div class="lbl">运行时间</div>
  </div>
</div>

<h2>视觉感知</h2>
<div class="card">
  <div class="row"><div class="lbl">检测状态</div><span id="vis-badge" class="badge idle">等待</span></div>
  <div class="grid3" style="margin-top:8px">
    <div><div class="lbl">置信度</div><div class="val" id="vis-conf">--</div></div>
    <div><div class="lbl">距离分</div><div class="val" id="vis-ds">--</div></div>
    <div><div class="lbl">数据时效</div><div class="val" id="vis-age" style="font-size:13px">--</div></div>
  </div>
  <div style="margin-top:8px"><div class="lbl">距离估计</div>
    <div class="bar-wrap"><div class="bar-fill" id="dist-bar" style="width:0%"></div></div>
    <div class="row" style="margin-top:2px"><span class="lbl">近</span><span class="lbl">远</span></div>
  </div>
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">ToF 测距</div><div class="val" id="vis-tof">--</div></div>
    <div><div class="lbl">脚部 Y</div><div class="val" id="vis-fy">--</div></div>
  </div>
</div>

<h2>检测框详情</h2>
<div class="card">
  <div class="grid3">
    <div><div class="lbl">中心 X</div><div class="val" id="vis-cx">--</div></div>
    <div><div class="lbl">中心 Y</div><div class="val" id="vis-cy">--</div></div>
    <div><div class="lbl">偏航</div><div class="val" id="vis-offset">--</div></div>
  </div>
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">宽度 W</div><div class="val" id="vis-w">--</div></div>
    <div><div class="lbl">高度 H</div><div class="val" id="vis-h">--</div></div>
  </div>
</div>

<h2>通信状态</h2>
<div class="card">
  <div class="row">
    <div><div class="lbl">VIS 帧成功率</div><div class="val" id="rx-rate">--</div></div>
    <div><span id="rx-badge" class="badge ok">--</span></div>
  </div>
  <div class="grid3" style="margin-top:6px">
    <div><div class="lbl">总帧数</div><div class="val" id="rx-total" style="font-size:13px">--</div></div>
    <div><div class="lbl">成功</div><div class="val" id="rx-ok" style="font-size:13px;color:#238636">--</div></div>
    <div><div class="lbl">失败</div><div class="val" id="rx-fail" style="font-size:13px;color:#da3633">--</div></div>
  </div>
</div>

<h2>系统</h2>
<div class="card">
  <div class="grid2">
    <div><div class="lbl">WiFi 客户端</div><div class="val" id="sys-clients">--</div></div>
    <div><div class="lbl">可用内存</div><div class="val" id="sys-heap">--</div></div>
  </div>
</div>

<div class="footer" id="footer">--</div>

<script>
function distLabel(ds){
  if(ds>=0.85)return'太近 STOP';if(ds>=0.65)return'较近 SLOW';
  if(ds>=0.30)return'适中';return'较远';
}
function ageLabel(ms){
  if(ms>2000)return'超时';if(ms>1000)return'慢';
  if(ms>500)return'正常';return'实时';
}
function update(){
  fetch('/status').then(r=>r.json()).then(d=>{
    let v=d.vis,s=d.sys,x=d.rx,b=document.getElementById('vis-badge');
    if(v.v&&v.hp){b.textContent='有人 PERSON';b.className='badge ok';}
    else if(v.v){b.textContent='无人 SCAN';b.className='badge warn';}
    else{b.textContent='等待数据';b.className='badge idle';}
    document.getElementById('vis-conf').textContent=v.hp?(v.conf*100).toFixed(0)+'%':'--';
    document.getElementById('vis-ds').textContent=v.hp?v.ds.toFixed(2):'--';
    document.getElementById('vis-age').textContent=v.v?(v.age||0)+'ms '+ageLabel(v.age||0):'--';
    document.getElementById('vis-tof').textContent=v.tof>0?(v.tof/1000).toFixed(2)+'m':'--';
    document.getElementById('vis-fy').textContent=v.hp?v.fy:'--';
    document.getElementById('vis-cx').textContent=v.hp?v.cx:'--';
    document.getElementById('vis-cy').textContent=v.hp?v.cy:'--';
    document.getElementById('vis-offset').textContent=v.hp?(v.cx-96).toFixed(0):'--';
    document.getElementById('vis-w').textContent=v.hp?v.w:'--';
    document.getElementById('vis-h').textContent=v.hp?v.h:'--';
    let bar=document.getElementById('dist-bar'),pct=Math.min(100,v.ds*100);
    bar.style.width=pct+'%';
    bar.className='bar-fill '+(v.ds>=0.85?'r':v.ds>=0.65?'y':'g');
    // RX stats
    if(x){
      let rate=x.total?(x.ok*100/x.total).toFixed(1)+'%':'--';
      document.getElementById('rx-rate').textContent=rate;
      document.getElementById('rx-total').textContent=x.total;
      document.getElementById('rx-ok').textContent=x.ok;
      document.getElementById('rx-fail').textContent=x.fail;
      let rb=document.getElementById('rx-badge');
      let rv=x.total?x.ok*100/x.total:0;
      rb.textContent=rv>95?'优秀':rv>80?'良好':rv>50?'一般':'差';
      rb.className='badge '+(rv>95?'ok':rv>80?'warn':'err');
    }
    // System
    let m=Math.floor(s.uptime/60000),sec=Math.floor((s.uptime%60000)/1000);
    document.getElementById('sys-uptime').textContent=m+'m '+sec+'s';
    document.getElementById('sys-clients').textContent=(s.wifi_clients||0)+' 个';
    document.getElementById('sys-heap').textContent=(s.heap/1024).toFixed(1)+' KB';
    document.getElementById('footer').textContent=new Date().toLocaleTimeString()+' 更新';
  }).catch(()=>{document.getElementById('footer').textContent='连接中...';});
}
update();setInterval(update,300);
</script></body></html>
)raw");
    server.send(200, "text/html", html);
}

// ─── Setup ───
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP8266 Bridge — Phase 0 ===");

    // VIS 接收
    visSerial.begin(4800);  // N6 P0 SW UART @ 4800 baud
    Serial.println("[VIS] D5(GPIO14) @ 4800 baud");
    Serial.println("[WiFi] Tracked Robot / 12345678");
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // WiFi AP
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi: "); Serial.print(WIFI_SSID);
    Serial.print(" @ "); Serial.println(WiFi.softAPIP());

    // HTTP
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.begin();
    Serial.println("[HTTP] port 80");
    Serial.println("=== Ready ===\n");
}

// ─── Loop ───
void loop() {
    server.handleClient();

    // VIS 接收
    int c;
    while ((c = visSerial.read()) >= 0 && c < 256) {
        digitalWrite(LED_BUILTIN, LOW);
        if (c == '\n' || c == '\r') {
            if (visLen > 0) {
                visLine[visLen] = '\0';
                totalFrames++;
                if (parseVisFrame(visLine, visLen)) okFrames++;
                else failFrames++;
                visLen = 0;
            }
        } else if (visLen < 250) {
            visLine[visLen++] = (char)c;
        }
        digitalWrite(LED_BUILTIN, HIGH);
    }
    static unsigned long lastDbg = 0;
    if (millis() - lastDbg > 5000) {
        lastDbg = millis();
        Serial.printf("[RX] t=%lu ok=%lu fail=%lu (%.0f%%)\n",
            totalFrames, okFrames, failFrames,
            totalFrames ? okFrames*100.0/totalFrames : 0);
    }
}
