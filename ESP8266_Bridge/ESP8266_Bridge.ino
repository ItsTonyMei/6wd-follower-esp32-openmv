// ============================================================================
// 履带车视觉跟随系统 — ESP8266 全功能控制器
// MCU: ESP8266 NodeMCU V3 (ESP-12E), 80MHz, 4MB Flash
// 替代 ESP32: VIS接收 + FollowLogic + STM32通信 + WiFi Dashboard
//
// 引脚:
//   D5 (GPIO14) ← OpenMV P0 SW UART (SoftwareSerial, 4800 baud)
//   D8 (GPIO15)  → STM32 PB11 (UART0 swapped, 115200 baud)
//   D7 (GPIO13)  ← STM32 PB10 (UART0 swapped, 115200 baud)
//   GND          ↔ STM32 GND (共地)
//
// UART0 通过 Serial.swap() 重映射到 GPIO15/13。
// 原 USB-Serial (GPIO1/3) 释放 — 调试通过 WiFi Dashboard。
// ============================================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include "Config.h"
#include "FollowLogic.h"

// ─── 硬件 ───
SoftwareSerial visSerial(PIN_VIS_RX, -1, false);  // RX only
ESP8266WebServer server(80);
FollowLogic      followLogic;

// ─── VIS 接收 buffer ───
char visLine[256];
int  visLen = 0;

// ─── VIS 最新数据 ───
struct {
    bool   valid = false;
    bool   hasPerson = false;
    int    cx = 0, cy = 0, w = 0, h = 0, feetY = 0;
    float  conf = 0, distScore = 0;
    int    tofDist = 0;
    unsigned long ts = 0;
} vis;

// ─── 车辆状态 ───
struct {
    uint16_t throttle = PWM_NEUTRAL;
    uint16_t steering = PWM_NEUTRAL;
    unsigned long ts = 0;
} car;

// ─── VIS 帧统计 ───
unsigned long totalFrames = 0, okFrames = 0, failFrames = 0;
unsigned long lastVisMs = 0;

// ─── CRC8 (与 STM32 一致) ───
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    }
    return crc;
}

// ─── 发送 MotorCmd 到 STM32 (6-byte 下行帧) ───
static void sendMotorCmd(const MotorCmd& mc) {
    uint8_t buf[6];
    buf[0] = 0xAA;
    buf[1] = mc.throttle & 0xFF;
    buf[2] = (mc.throttle >> 8) & 0xFF;
    buf[3] = mc.steering & 0xFF;
    buf[4] = (mc.steering >> 8) & 0xFF;
    buf[5] = crc8(&buf[1], 4);
    Serial.write(buf, 6);
}

// ─── VIS 帧解析 (无校验和 — OpenMV 链路极短, 误码率可忽略) ───
static bool parseVisFrame(char* buf, int len) {
    if (len < 4 || strncmp(buf, "VIS:", 4) != 0) return false;

    char* p = buf + 4;
    char* end = nullptr;

    vis.cx     = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    vis.cy     = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    vis.w      = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    vis.h      = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    vis.feetY  = (int)strtol(p, &end, 10); if (*end != ',') return false; p = end + 1;
    vis.conf   = strtof(p, &end);           if (*end != ',') return false; p = end + 1;

    char* comma = strchr(p, ',');
    if (!comma) return false;
    *comma = '\0';
    vis.hasPerson = (strcmp(p, "PERSON") == 0);
    *comma = ',';
    p = comma + 1;

    vis.distScore = strtof(p, &end);

    vis.tofDist = 0;
    if (*end == ',') {
        p = end + 1;
        vis.tofDist = (int)strtol(p, &end, 10);
    }

    vis.valid = true;
    vis.ts = millis();
    lastVisMs = vis.ts;
    return true;
}

// ─── JSON 状态端点 ───
void handleStatus() {
    unsigned long now = millis();
    unsigned long visAge = vis.valid ? (now - vis.ts) : 9999;

    char json[512];
    snprintf(json, sizeof(json),
        "{"
        "\"vis\":{\"v\":%d,\"hp\":%d,\"cx\":%d,\"cy\":%d,\"w\":%d,\"h\":%d,"
        "\"conf\":%.2f,\"ds\":%.2f,\"tof\":%d,\"fy\":%d,\"age\":%lu,\"ts\":%lu},"
        "\"car\":{\"th\":%u,\"st\":%u,\"ts\":%lu},"
        "\"sys\":{\"uptime\":%lu,\"heap\":%u,\"wifi_clients\":%d},"
        "\"rx\":{\"total\":%lu,\"ok\":%lu,\"fail\":%lu}}",
        vis.valid, vis.hasPerson,
        vis.cx, vis.cy, vis.w, vis.h,
        vis.conf, vis.distScore, vis.tofDist, vis.feetY, visAge, vis.ts,
        car.throttle, car.steering, car.ts,
        now, ESP.getFreeHeap(), WiFi.softAPgetStationNum(),
        totalFrames, okFrames, failFrames);
    server.send(200, "application/json", json);
}

// ─── Dashboard HTML ───
void handleRoot() {
    String html = F(R"raw(
<!DOCTYPE html><html lang="zh"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Rover</title>
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
</style></head><body>

<div class="card" style="display:flex;justify-content:space-between;align-items:center">
  <div><div style="font-size:18px;font-weight:bold;color:#58a6ff">履带车视觉跟随</div>
  <div class="lbl">ESP8266 HC6060A</div></div>
  <div style="text-align:right">
    <div class="val" style="font-size:13px" id="sys-uptime">--</div>
    <div class="lbl">运行时间</div>
  </div>
</div>

<h2>车辆控制</h2>
<div class="card">
  <div class="row"><div class="lbl">控制状态</div><span id="car-badge" class="badge idle">STOP</span></div>
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">油门 (白线)</div><div class="val" id="car-th">1500 μs</div></div>
    <div><div class="lbl">转向 (黄线)</div><div class="val" id="car-st">1500 μs</div></div>
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
  <div class="grid2" style="margin-top:8px">
    <div><div class="lbl">中心 X / 偏移</div><div class="val" id="vis-offset">--</div></div>
    <div><div class="lbl">框 W×H</div><div class="val" id="vis-size">--</div></div>
  </div>
</div>

<h2>通信状态</h2>
<div class="card">
  <div class="row">
    <div><div class="lbl">VIS 帧成功率</div><div class="val" id="rx-rate">--</div></div>
    <div><span id="rx-badge" class="badge ok">--</span></div>
  </div>
  <div class="grid3" style="margin-top:6px">
    <div><div class="lbl">总帧</div><div class="val" id="rx-total" style="font-size:13px">--</div></div>
    <div><div class="lbl">成功</div><div class="val" id="rx-ok" style="font-size:13px;color:#238636">--</div></div>
    <div><div class="lbl">失败</div><div class="val" id="rx-fail" style="font-size:13px;color:#da3633">--</div></div>
  </div>
</div>

<h2>系统</h2>
<div class="card">
  <div class="grid3">
    <div><div class="lbl">WiFi 客户端</div><div class="val" id="sys-clients">--</div></div>
    <div><div class="lbl">可用内存</div><div class="val" id="sys-heap">--</div></div>
    <div><div class="lbl">Heap</div><div class="val" id="sys-heap2" style="font-size:11px">--</div></div>
  </div>
</div>

<div class="footer" id="footer">--</div>

<script>
function distLabel(ds){
  if(ds>=0.85)return'STOP';if(ds>=0.65)return'SLOW';
  if(ds>=0.30)return'MID';return'FAR';
}
function ageLabel(ms){
  if(ms>2000)return'超时';if(ms>1000)return'慢';
  if(ms>500)return'正常';return'实时';
}
function update(){
  fetch('/status').then(r=>r.json()).then(d=>{
    let v=d.vis,c=d.car,s=d.sys,x=d.rx;

    // Car
    let th=c.th||1500,st=c.st||1500,act='STOP';
    if(th>1520)act='FWD';else if(th<1480)act='REV';
    if(st>1520)act+=' +R';else if(st<1480)act+=' +L';
    let cb=document.getElementById('car-badge');
    cb.textContent=act;cb.className='badge '+(act==='STOP'?'idle':'ok');
    document.getElementById('car-th').textContent=th+' μs';
    document.getElementById('car-st').textContent=st+' μs';

    // Vis
    let vb=document.getElementById('vis-badge');
    if(v.v&&v.hp){vb.textContent='有人 PERSON';vb.className='badge ok';}
    else if(v.v){vb.textContent='无人 SCAN';vb.className='badge warn';}
    else{vb.textContent='等待数据';vb.className='badge idle';}
    document.getElementById('vis-conf').textContent=v.hp?(v.conf*100).toFixed(0)+'%':'--';
    document.getElementById('vis-ds').textContent=v.hp?v.ds.toFixed(2):'--';
    document.getElementById('vis-age').textContent=v.v?(v.age||0)+'ms '+ageLabel(v.age||0):'--';
    document.getElementById('vis-tof').textContent=v.tof>0?(v.tof/1000).toFixed(2)+'m':'--';
    document.getElementById('vis-fy').textContent=v.hp?v.fy:'--';
    document.getElementById('vis-offset').textContent=v.hp?(v.cx-96)+' / '+v.cx:'--';
    document.getElementById('vis-size').textContent=v.hp?v.w+'x'+v.h:'--';
    let bar=document.getElementById('dist-bar'),pct=Math.min(100,v.ds*100);
    bar.style.width=pct+'%';
    bar.className='bar-fill '+(v.ds>=0.85?'r':v.ds>=0.65?'y':'g');

    // RX
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

    // Sys
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
    Serial.swap();  // UART0 → GPIO15(TX), GPIO13(RX)
    delay(300);
    Serial.println("\n=== ESP8266 Controller ===");
    Serial.print("[UART] STM32 @ "); Serial.print(STM32_BAUD);
    Serial.print(" baud (TX=D8/GPIO"); Serial.print(PIN_STM32_TX);
    Serial.print(", RX=D7/GPIO"); Serial.print(PIN_STM32_RX);
    Serial.println(")");

    // VIS SoftwareSerial
    visSerial.begin(4800);
    Serial.print("[VIS] D5/GPIO"); Serial.print(PIN_VIS_RX);
    Serial.println(" @ 4800 baud");

    // LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // WiFi AP
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] "); Serial.print(WIFI_SSID);
    Serial.print(" @ "); Serial.println(WiFi.softAPIP());

    // HTTP
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.begin();
    Serial.println("[HTTP] :80");

    // ─── ESC 初始化: 3 秒中位信号 ───
    MotorCmd neutral = {PWM_NEUTRAL, PWM_NEUTRAL};
    unsigned long start = millis();
    while (millis() - start < ESC_INIT_DELAY_MS) {
        sendMotorCmd(neutral);
        delay(50);
    }
    car.throttle = PWM_NEUTRAL;
    car.steering = PWM_NEUTRAL;
    car.ts = millis();
    Serial.println("[ESC] Init complete, ready\n");
}

// ─── Loop ───
void loop() {
    server.handleClient();

    // ─── 1. VIS 接收 ───
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
        } else if (visLen < (int)sizeof(visLine) - 1) {
            visLine[visLen++] = (char)c;
        }
    }

    // ─── 2. FollowLogic ───
    static unsigned long lastCmdMs = 0;
    unsigned long now = millis();
    if (now - lastCmdMs >= STM32_CMD_INTERVAL_MS) {
        lastCmdMs = now;
        MotorCmd mc;
        if (vis.valid && (now - vis.ts) < VISION_TIMEOUT_MS) {
            mc = followLogic.update(vis.hasPerson, vis.cx, vis.feetY, vis.distScore);
        } else {
            mc = {PWM_NEUTRAL, PWM_NEUTRAL};
        }
        sendMotorCmd(mc);
        car.throttle = mc.throttle;
        car.steering = mc.steering;
        car.ts = now;
    }
}
