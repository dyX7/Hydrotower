#include "web.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <fsm.hpp>
#include <time.h>

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
int cylce_time_minutes = 10;
int watering_minutes = 5;
int flush_minutes = 3;

// ---------------- COMMAND BRIDGE ----------------
volatile system_cmd_t system_cmd = CMD_NONE;

// ---------------- DATA ----------------
extern float ec, ph;
extern float ecTemp, phTemp;
extern fsm_t fsm;
extern uint32_t remaining_seconds;


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
body {
  font-family: Arial;
  text-align: center;
  background-color: #e9fbea;
  margin: 0;
  padding-top: 50px;
}
button { padding:10px; margin:5px; }

.tab { display:none; }

#main {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
}

.timerRow {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 50%;
  margin: 8px auto;
  gap: 10px;
}

.timerValue {
  width: 80px;
  text-align: center;
  font-size: 18px;
}

.section {
  margin-top: 20px;
  width: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
}

#chart {
  width: 300px !important;
  height: 200px !important;
  margin: auto;
}

.timerLabel {
  width: 120px;
  text-align: right;
  margin-right: 10px;
}

#chartEC, #chartPH {
  width: 300px !important;
  height: 200px !important;
  margin: 10px auto;
  display: block;
}

.dataBlock {
  margin-top: 10px;
  font-size: 18px;
  text-align: center;
}

.dashboard {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.topBar {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px 15px;
  box-sizing: border-box;
  background-color: #e9fbea;
}

</style>
</head>

<body>

<div class="topBar">
  <div class="title">HydroTower</div>
  <div id="time">--</div>
</div>

<button onclick="showTab('main')">Control</button>
<button onclick="showTab('graph')">Graph</button>
<button onclick="showTab('monitor')">Log</button>

<!-- ================= CONTROL TAB ================= -->

<div id="main" class="tab">
  <div class="dashboard">

    <div class="stateBox">
      <div id="state">---</div>
      <div><span id="timer">0</span> s</div>
    </div>

    <div class="dataBlock">
      <div>EC: <span id="ec">0</span></div>
      <div>PH: <span id="ph">0</span></div>
    </div>


<div class="section">

<div class="timerRow">
  <div class="timerLabel">Cycle</div>
  <input id="idle" class="timerValue" type="number" min="0">
  <span>min</span>
</div>

<div class="timerRow">
  <div class="timerLabel">Water</div>
  <input id="water" class="timerValue" type="number" min="0">
  <span>min</span>
</div>

<div class="timerRow">
  <div class="timerLabel">Flush</div>
  <input id="flush" class="timerValue" type="number" min="0">
  <span>min</span>
</div>

  <button onclick="save()">Save Timers</button>
</div>

<div class="section">
  <h3>State Machine Control</h3>
  <button onclick="setState('/idle')">IDLE</button>
  <button id="waterBtn" onclick="setState('/water')">WATER</button>
  <button id="measureBtn" onclick="setState('/measure')">MEASURE</button>
  <button id="regulateBtn" onclick="setState('/regulate')">REGULATE</button>
  <button id="flushBtn" onclick="setState('/flush')">FLUSH</button>
  <button id="calibrateBtn" onclick="setState('/calibrate')">CALIBRATE</button>
</div>

</div>

<!-- ================= GRAPH TAB ================= -->

<div id="graph" class="tab">

  EC: <span id="ec">0</span><br>
  PH: <span id="ph">0</span><br>

  <canvas id="chartEC"></canvas>
  <canvas id="chartPH"></canvas>

</div>

<!-- ================= MONITOR TAB ================= -->
<div id="monitor" class="tab">

  <div style="display:flex; justify-content:center;">
    <pre id="log" style="text-align:left;width:300px;height:600px;overflow:auto;background:transparent;color:black;"></pre>
  </div>

</div>

<script>

function showTab(id){
  document.querySelectorAll('.tab').forEach(e=>e.style.display='none');
  document.getElementById(id).style.display='block';
}

function setState(route)
{
  fetch(route.startsWith("/") ? route : "/" + route);
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

let chartEC = new Chart(document.getElementById('chartEC'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'EC',
      data: [],
      borderColor: '#2196F3',
      backgroundColor: 'rgba(0, 0, 255, 0.2)',
      borderWidth: 2,
      tension: 0.3
    }]
  },
  options: {
    responsive: false,
    maintainAspectRatio: false
  }
});


let chartPH = new Chart(document.getElementById('chartPH'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'PH',
      data: [],
      borderColor: '#4CAF50',
      backgroundColor: 'rgba(0, 255, 0, 0.2)',
      borderWidth: 2,
      tension: 0.3
    }]
  },
  options: {
    responsive: false,
    maintainAspectRatio: false
  }
});

let initDone = false;

async function update(){
  let d = await (await fetch('/data')).json();

  document.getElementById('state').innerText = d.state;
  document.getElementById('ec').innerText = d.ec;
  document.getElementById('ph').innerText = d.ph;
  document.getElementById('timer').innerText = d.timer;
  document.getElementById('time').innerText = d.time;

  chartEC.data.labels = d.labels;
  chartEC.data.datasets[0].data = d.ec_hist;
  chartEC.update();

  chartPH.data.labels = d.labels;
  chartPH.data.datasets[0].data = d.ph_hist;
  chartPH.update();

  if (!initDone) {
    document.getElementById('idle').value = d.idle;
    document.getElementById('water').value = d.water;
    document.getElementById('flush').value = d.flush;
    initDone = true;
  }

}

async function updateLog(){
  let r = await fetch('/log');
  document.getElementById('log').innerText = await r.text();
}

setInterval(update, 500);
setInterval(updateLog, 500);

update();
updateLog();

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

    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  }
  else
  {
    startAP();
  }
}


// ==================================================
// DATE TIME
// ==================================================
String getTimeString()
{
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo))
    return "--:--:--";

  char buf[30];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

// ==================================================
// JSON
// ==================================================
String buildJson()
{
  String json = "{";

  // Basic values
  const char* stateName = fsm.stateName();
  json += "\"state\":\"" + String(stateName ? stateName : "UNKNOWN") + "\",";
  json += "\"timer\":" + String(remaining_seconds) + ",";
  json += "\"ec\":" + String(ec, 2) + ",";
  json += "\"ph\":" + String(ph, 2) + ",";
  json += "\"time\":\"" + getTimeString() + "\",";
  json += "\"idle\":" + String(cylce_time_minutes) + ",";
  json += "\"water\":" + String(watering_minutes) + ",";
  json += "\"flush\":" + String(flush_minutes) + ",";

  const char* img;

  switch(system_cmd)
  {
    case CMD_IDLE:     img = img_idle; break;
    case CMD_WATER:    img = img_idle; break; //img_water; break;
    case CMD_MEASURE:  img = img_idle; break; //img_measure; break;
    case CMD_REGULATE: img = img_idle; break; //img_regulate; break;
    case CMD_FLUSH:    img = img_idle; break; //img_flush; break;
    case CMD_CALIBRATE:img = img_idle; break; //img_calibrate; break;
    default:           img = img_idle; break; //img_idle; break;
  }

  json += "\"img\":\"";
  json += img;
  json += "\",";

  // History count
  int count = hist_full ? MAX_POINTS : hist_index;

  // Labels (NO leading comma bug!)
  json += "\"labels\":[";
  for(int i = 0; i < count; i++){
    json += "\"" + String(i) + "\"";
    if(i < count - 1) json += ",";
  }
  json += "],";

  // EC history
  json += "\"ec_hist\":[";
  for(int i = 0; i < count; i++){
    json += String(ec_hist[i], 2);
    if(i < count - 1) json += ",";
  }
  json += "],";

  // PH history
  json += "\"ph_hist\":[";
  for(int i = 0; i < count; i++){
    json += String(ph_hist[i], 2);
    if(i < count - 1) json += ",";
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
      cylce_time_minutes = req->getParam("idle")->value().toInt();

    if(req->hasParam("water"))
      watering_minutes = req->getParam("water")->value().toInt();

    if(req->hasParam("flush"))
      flush_minutes = req->getParam("flush")->value().toInt();

    web_log("Timers updated");
    req->send(200, "text/plain", "OK");
  });

  server.on("/idle", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_IDLE, "IDLE");
    req->send(200, "text/plain", "OK");
  });

  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_WATER, "WATER");
    req->send(200, "text/plain", "OK");
  });

  server.on("/measure", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_MEASURE, "MEASURE");
    req->send(200, "text/plain", "OK");
  });

  server.on("/regulate", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_REGULATE, "REGULATE");
    req->send(200, "text/plain", "OK");
  });

  server.on("/flush", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_FLUSH, "FLUSH");
    req->send(200, "text/plain", "OK");
  });

  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    setCommand(CMD_CALIBRATE, "CALIBRATE");
    req->send(200, "text/plain", "OK");
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
  static String lastState = "";
  String currentState = String(fsm.stateName());

  if(currentState != lastState)
  {
    lastState = currentState;
  }
}