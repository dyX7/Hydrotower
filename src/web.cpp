#include "web.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <fsm.hpp>

AsyncWebServer server(80);
Preferences prefs;

// ---------------- WiFi ----------------
String ssid;
String password;
bool apMode = false;

// ---------------- Pump settings ----------------
int pump_on_minutes = 1;
int pump_cycle_minutes = 10;

// NEW timers
int idle_minutes = 10;
int watering_minutes = 5;
int flush_minutes = 3;

// ---------------- COMMAND BRIDGE ----------------
volatile system_cmd_t system_cmd = CMD_NONE;
bool watering_active = false;

// NEW toggle states
bool measure_active = false;
bool regulate_active = false;
bool flush_active = false;
bool calibrate_active = false;

// ---------------- DATA ----------------
extern float ec, ph;
extern float ecTemp, phTemp;
extern fsm_t fsm;

// ---------------- LOG ----------------
#define LOG_SIZE 40
String logBuffer[LOG_SIZE];
int logIndex = 0;

// ---------------- HISTORY ----------------
#define MAX_POINTS 60
float ec_hist[MAX_POINTS];
float ph_hist[MAX_POINTS];
int hist_index = 0;
bool hist_full = false;

// ==================================================
// LOGGING
// ==================================================
void web_log(const String &msg)
{
  Serial.println(msg);
  logBuffer[logIndex] = msg;
  logIndex = (logIndex + 1) % LOG_SIZE;
}

// ==================================================
// DATA BUFFER
// ==================================================
void web_add_data(float ec_v, float ph_v)
{
  ec_hist[hist_index] = ec_v;
  ph_hist[hist_index] = ph_v;

  hist_index = (hist_index + 1) % MAX_POINTS;
  if(hist_index == 0) hist_full = true;
}

// ==================================================
// COMMAND HELPERS
// ==================================================
void setCommand(system_cmd_t cmd, const String& name)
{
  system_cmd = cmd;
  web_log(String("CMD ") + name);
}

// ==================================================
// HTML
// ==================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>
body { font-family: Arial; text-align:center; }
button { padding:10px; margin:5px; }

.tab { display:none; }

.stateBox {
  font-size: 22px;
  font-weight: bold;
  padding: 10px;
  margin: 10px;
  background: #ffffff;
  color: rgb(65, 116, 65);
  display: inline-block;
}

.timerRow {
  display: flex;
  justify-content: center;
  align-items: center;
  margin: 8px;
}

.timerRow button {
  width: 40px;
  height: 40px;
}

.timerValue {
  width: 80px;
  text-align: center;
  font-size: 18px;
}

.section {
  margin-top: 20px;
}
</style>
</head>

<body>

<h2>ESP32 HydroTower</h2>

<button onclick="showTab('main')">Control</button>
<button onclick="showTab('monitor')">Monitoring</button>

<!-- ================= CONTROL TAB ================= -->
<div id="main" class="tab">

<div class="stateBox" id="state">---</div><br>

<div class="section">
  <h3>Cycle Timers</h3>

  <div class="timerRow">
    <button onclick="changeVal('idle', 1)">+</button>
    <input id="idle" class="timerValue" type="number" value="10">
    <button onclick="changeVal('idle', -1)">-</button>
  </div>
  <div>Idle time (min)</div>

  <div class="timerRow">
    <button onclick="changeVal('water', 1)">+</button>
    <input id="water" class="timerValue" type="number" value="5">
    <button onclick="changeVal('water', -1)">-</button>
  </div>
  <div>Watering time (min)</div>

  <div class="timerRow">
    <button onclick="changeVal('flush', 1)">+</button>
    <input id="flush" class="timerValue" type="number" value="3">
    <button onclick="changeVal('flush', -1)">-</button>
  </div>
  <div>Flush time (min)</div>

  <button onclick="save()">Save Timers</button>
</div>

<div class="section">
  <h3>Manual Actions</h3>
  <button id="waterBtn" onclick="toggle('/water','waterBtn')">Watering: OFF</button>
  <button id="measureBtn" onclick="toggle('/measure','measureBtn')">Measure: OFF</button>
  <button id="regulateBtn" onclick="toggle('/regulate','regulateBtn')">Regulate: OFF</button>
  <button id="flushBtn" onclick="toggle('/flush','flushBtn')">Flush: OFF</button>
  <button id="calibrateBtn" onclick="toggle('/calibrate','calibrateBtn')">Calibrate: OFF</button>
</div>

</div>

<!-- ================= MONITOR TAB ================= -->
<div id="monitor" class="tab">

EC: <span id="ec">0</span><br>
PH: <span id="ph">0</span><br>

<canvas id="chart"></canvas>

<pre id="log" style="text-align:left;height:200px;overflow:auto;background:#111;color:#0f0;"></pre>

</div>

<script>

function showTab(id){
  document.querySelectorAll('.tab').forEach(e=>e.style.display='none');
  document.getElementById(id).style.display='block';
}

function toggle(url, btnId)
{
  fetch(url).then(r => r.text()).then(state => {
    let btn = document.getElementById(btnId);

    if(state === "ON")
    {
      btn.innerText = btn.innerText.split(":")[0] + ": ON";
    }
    else
    {
      btn.innerText = btn.innerText.split(":")[0] + ": OFF";
    }
  });
}

function changeVal(id, delta)
{
  let el = document.getElementById(id);
  let v = parseInt(el.value) || 0;
  v += delta;
  if (v < 0) v = 0;
  el.value = v;
}

async function save(){
  let idle = document.getElementById('idle').value;
  let water = document.getElementById('water').value;
  let flush = document.getElementById('flush').value;

  await fetch(`/set?idle=${idle}&water=${water}&flush=${flush}`);
}

let chart = new Chart(document.getElementById('chart'), {
  type:'line',
  data:{labels:[],datasets:[
    {label:'EC',data:[]},
    {label:'PH',data:[]}
  ]}
});

async function update(){
  let d = await (await fetch('/data')).json();

  document.getElementById('state').innerText = d.state;
  document.getElementById('ec').innerText = d.ec;
  document.getElementById('ph').innerText = d.ph;

  chart.data.labels = d.labels;
  chart.data.datasets[0].data = d.ec_hist;
  chart.data.datasets[1].data = d.ph_hist;
  chart.update();
}

async function updateLog(){
  let r = await fetch('/log');
  document.getElementById('log').innerText = await r.text();
}

setInterval(update, 2000);
setInterval(updateLog, 2000);

showTab('main');

</script>

</body>
</html>
)rawliteral";

// ==================================================
// WIFI + AP MODE
// ==================================================
void startAP()
{
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    req->send(200, "text/html",
      "<h2>Setup WiFi</h2>"
      "<form action='/save'>"
      "SSID:<input name='s'><br>"
      "PASS:<input name='p' type='password'><br>"
      "<button>Save</button>"
      "</form>");
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    String s = req->getParam("s")->value();
    String p = req->getParam("p")->value();

    prefs.begin("wifi", false);
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
    prefs.end();

    req->send(200, "text/plain", "Saved. Rebooting...");
    delay(1000);
    ESP.restart();
  });
}

// ==================================================
// WIFI CONNECT
// ==================================================
void connectWiFi()
{
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  prefs.end();

  if(ssid == "")
  {
    startAP();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int tries = 0;
  while(WiFi.status() != WL_CONNECTED && tries < 20)
  {
    delay(500);
    tries++;
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.println(WiFi.localIP());

    if(MDNS.begin("hydrotower1"))
    {
      MDNS.addService("http", "tcp", 80);
    }
  }
  else
  {
    startAP();
  }
}

// ==================================================
// JSON
// ==================================================
String buildJson()
{
  String json = "{";

  json += "\"state\":\"" + String(fsm.stateName()) + "\",";
  json += "\"ec\":" + String(ec) + ",";
  json += "\"ph\":" + String(ph) + ",";

  int count = hist_full ? MAX_POINTS : hist_index;

  json += "\"labels\":[";
  for(int i=0;i<count;i++){
    json += "\"" + String(i) + "\"";
    if(i<count-1) json += ",";
  }
  json += "],";

  json += "\"ec_hist\":[";
  for(int i=0;i<count;i++){
    json += String(ec_hist[i]);
    if(i<count-1) json += ",";
  }
  json += "],";

  json += "\"ph_hist\":[";
  for(int i=0;i<count;i++){
    json += String(ph_hist[i]);
    if(i<count-1) json += ",";
  }
  json += "]";

  json += "}";
  return json;
}

// ==================================================
// ROUTES
// ==================================================
void setupRoutes()
{
  if(apMode) return;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    req->send(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    req->send(200, "application/json", buildJson());
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    String out;
    for(int i=0;i<LOG_SIZE;i++)
    {
      int idx = (logIndex + i) % LOG_SIZE;
      if(logBuffer[idx].length())
        out += logBuffer[idx] + "\n";
    }
    req->send(200, "text/plain", out);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    if(req->hasParam("idle"))
      idle_minutes = req->getParam("idle")->value().toInt();

    if(req->hasParam("water"))
      watering_minutes = req->getParam("water")->value().toInt();

    if(req->hasParam("flush"))
      flush_minutes = req->getParam("flush")->value().toInt();

    web_log("Timers updated");
    req->send(200, "text/plain", "OK");
  });

  // ===== toggle endpoints =====
  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    watering_active = !watering_active;
    setCommand(watering_active ? CMD_WATER_ON : CMD_WATER_OFF, "WATER");
    req->send(200, "text/plain", watering_active ? "ON" : "OFF");
  });

  server.on("/measure", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    measure_active = !measure_active;
    setCommand(measure_active ? CMD_MEASURE_ON : CMD_MEASURE_OFF, "MEASURE");
    req->send(200, "text/plain", measure_active ? "ON" : "OFF");
  });

  server.on("/regulate", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    regulate_active = !regulate_active;
    setCommand(regulate_active ? CMD_REGULATE_ON : CMD_REGULATE_OFF, "REGULATE");
    req->send(200, "text/plain", regulate_active ? "ON" : "OFF");
  });

  server.on("/flush", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    flush_active = !flush_active;
    setCommand(flush_active ? CMD_FLUSH_ON : CMD_FLUSH_OFF, "FLUSH");
    req->send(200, "text/plain", flush_active ? "ON" : "OFF");
  });

  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    calibrate_active = !calibrate_active;
    setCommand(calibrate_active ? CMD_CALIBRATE_ON : CMD_CALIBRATE_OFF, "CALIBRATE");
    req->send(200, "text/plain", calibrate_active ? "ON" : "OFF");
  });
}

// ==================================================
void web_init()
{
  connectWiFi();
  setupRoutes();
  server.begin();
}

void web_loop() 
{
  State* s = fsm.current;

  if (s == &STATE_IDLE)
  {
    web_log("STATE: IDLE");
  }
  else if (s == &STATE_WATERING)
  {
    web_log("STATE: WATERING");
  }
  else if (s == &STATE_MEASURE)
  {
    web_log("STATE: MEASURE");
  }
  else if (s == &STATE_REGULATE)
  {
    web_log("STATE: REGULATE");
  }
  else if (s == &STATE_FLUSH)
  {
    web_log("STATE: FLUSH");
  }
  else if (s == &STATE_CALIBRATE)
  {
    web_log("STATE: CALIBRATE");
  }
  else
  {
    web_log("STATE: UNKNOWN");
  }
}