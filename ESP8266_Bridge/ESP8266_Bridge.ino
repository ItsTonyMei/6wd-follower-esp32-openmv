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
void handleStatus() {
    char json[512];
    snprintf(json, sizeof(json),
        "{\"vis\":{\"v\":%d,\"hp\":%d,\"cx\":%d,\"cy\":%d,\"w\":%d,\"h\":%d,"
        "\"conf\":%.2f,\"ds\":%.2f,\"tof\":%d,\"fy\":%d,\"ts\":%lu},"
        "\"sys\":{\"uptime\":%lu,\"heap\":%u}}",
        latestVis.valid, latestVis.hasPerson,
        latestVis.cx, latestVis.cy, latestVis.w, latestVis.h,
        latestVis.conf, latestVis.distScore, latestVis.tofDist, latestVis.feetY,
        latestVis.ts,
        millis(), ESP.getFreeHeap());
    server.send(200, "application/json", json);
}

// ─── HTTP 首页 (简约 Dashboard) ───
void handleRoot() {
    String html = F(R"raw(
<!DOCTYPE html><html lang="zh"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>履带车跟随</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#0f1117;color:#c9d1d9;max-width:420px;margin:0 auto;padding:10px}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:12px;margin-bottom:8px}
.lbl{font-size:10px;color:#8b949e;text-transform:uppercase}
.val{font-size:18px;font-weight:bold;color:#e6edf3}
.badge{display:inline-block;padding:3px 10px;border-radius:10px;font-size:14px;font-weight:bold}
.ok{background:#238636;color:#fff}.warn{background:#d2991d;color:#000}.idle{background:#30363d;color:#8b949e}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.bar-wrap{height:10px;background:#21262d;border-radius:5px;overflow:hidden;margin:4px 0}
.bar-fill{height:100%;border-radius:5px;transition:width .3s}
.r{background:#da3633}.y{background:#d2991d}.g{background:#238636}
.footer{text-align:center;font-size:10px;color:#484f58;margin-top:8px}
</style></head><body>
<div class="card"><div style="font-size:18px;font-weight:bold;color:#58a6ff">履带车跟随</div>
<div class="lbl">ESP8266 Bridge — Phase 0</div></div>
<div class="card"><div class="lbl">检测</div>
<span id="vis-badge" class="badge idle">等待</span>
<div class="grid2" style="margin-top:8px">
<div><div class="lbl">置信度</div><div class="val" id="vis-conf">--</div></div>
<div><div class="lbl">距离分</div><div class="val" id="vis-ds">--</div></div>
</div>
<div style="margin-top:8px"><div class="lbl">距离</div><div class="bar-wrap"><div class="bar-fill" id="dist-bar" style="width:0%"></div></div></div>
<div class="grid2" style="margin-top:8px">
<div><div class="lbl">ToF</div><div class="val" id="vis-tof">--</div></div>
<div><div class="lbl">检测框</div><div class="val" id="vis-box">--</div></div>
</div></div>
<div class="card"><div class="lbl">系统</div>
<div id="sys-info" class="val" style="font-size:12px">--</div></div>
<div class="footer" id="footer">ESP8266 Bridge — Phase 0</div>
<script>
function update(){fetch('/status').then(r=>r.json()).then(d=>{
  let v=d.vis,s=d.sys,b=document.getElementById('vis-badge');
  if(v.v&&v.hp){b.textContent='有人 PERSON';b.className='badge ok';}
  else if(v.v){b.textContent='无人';b.className='badge warn';}
  else{b.textContent='等待';b.className='badge idle';}
  document.getElementById('vis-conf').textContent=v.hp?(v.conf*100).toFixed(0)+'%':'--';
  document.getElementById('vis-ds').textContent=v.hp?v.ds.toFixed(2):'--';
  document.getElementById('vis-tof').textContent=v.tof>0?(v.tof/1000).toFixed(2)+'m':'--';
  document.getElementById('vis-box').textContent=v.hp?v.w+'x'+v.h+' @('+v.cx+','+v.cy+')':'--';
  let bar=document.getElementById('dist-bar'),pct=v.ds*100;
  bar.style.width=pct+'%';bar.className='bar-fill '+(v.ds>=0.85?'r':v.ds>=0.65?'y':'g');
  document.getElementById('sys-info').textContent='运行:'+(s.uptime/60000).toFixed(0)+'min 堆:'+s.heap+'B';
  document.getElementById('footer').textContent='更新 '+new Date().toLocaleTimeString();
}).catch(()=>{document.getElementById('footer').textContent='连接中...'});}
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

    // VIS 接收: 直接 read(), SoftwareSerial.available() 在 ESP8266 上不可靠
    int c;
    static unsigned long rxCount = 0, okCount = 0, failCount = 0;
    while ((c = visSerial.read()) >= 0 && c < 256) {
        digitalWrite(LED_BUILTIN, LOW);
        if (c == '\n' || c == '\r') {
            if (visLen > 0) {
                visLine[visLen] = '\0';
                rxCount++;
                if (parseVisFrame(visLine, visLen)) {
                    okCount++;
                } else {
                    failCount++;
                }
                visLen = 0;
            }
        } else if (visLen < 250) {
            visLine[visLen++] = (char)c;
        }
        digitalWrite(LED_BUILTIN, HIGH);
    }
    static unsigned long lastDbg = 0;
    if (millis() - lastDbg > 3000) {
        lastDbg = millis();
        Serial.printf("[RX] total=%lu ok=%lu fail=%lu\n", rxCount, okCount, failCount);
    }
}
