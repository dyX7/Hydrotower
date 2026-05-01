#include "web.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <fsm.hpp>

AsyncWebServer server(80);
Preferences prefs;

String ssid;
String password;
bool apMode = false;

// ---------------- Pump settings ----------------
int pump_on_minutes = 1;
int pump_cycle_minutes = 10;

// ---------------- COMMAND BRIDGE ----------------
volatile system_cmd_t system_cmd = CMD_NONE;
bool watering_active = false;

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
// HTML (FULL RESTORED + WATER BUTTON)
// ==================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>
body { font-family: Arial; text-align:center; }
.tab { display:none; }
button { padding:10px; margin:5px; }

.stateBox {
  font-size: 22px;
  font-weight: bold;
  padding: 10px;
  margin: 10px;
  background: #222;
  color: #0f0;
  display: inline-block;
}
</style>
</head>

<body>

<h2>ESP32 HydroTower</h2>

<button onclick="show('status')">Status</button>
<button onclick="show('control')">Control</button>

<div id="status" class="tab">

  <h3>Status</h3>

  <div class="stateBox" id="state">---</div><br>

  EC: <span id="ec">0</span><br>
  PH: <span id="ph">0</span><br>

  <canvas id="chart"></canvas>

  <pre id="log" style="text-align:left;height:200px;overflow:auto;background:#111;color:#0f0;"></pre>
</div>

<div id="control" class="tab">

  <h3>Pump Control</h3>

  Pump ON time (minutes):<br>
  <input id="on" type="number"><br>

  Cycle time (minutes):<br>
  <input id="cycle" type="number"><br><br>

  <button onclick="save()">Save</button><br><br>

  <button id="waterBtn" onclick="toggleWater()">Watering: OFF</button>

</div>

<script>

function show(t){
  document.querySelectorAll('.tab').forEach(e=>e.style.display='none');
  document.getElementById(t).style.display='block';
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

async function save(){
  let on = document.getElementById('on').value;
  let cycle = document.getElementById('cycle').value;
  await fetch(`/set?on=${on}&cycle=${cycle}`);
}

function toggleWater()
{
  fetch('/water').then(r => r.text()).then(state => {
    let btn = document.getElementById("waterBtn");

    if(state === "ON")
    {
      btn.innerText = "Watering: ON";
      btn.style.background = "green";
    }
    else
    {
      btn.innerText = "Watering: OFF";
      btn.style.background = "red";
    }
  });
}

setInterval(update, 2000);
setInterval(updateLog, 2000);

show('status');

</script>

</body>
</html>
)rawliteral";

// ==================================================
// WIFI + AP MODE (RESTORED)
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
// WIFI CONNECT + mDNS (RESTORED)
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
      Serial.println("http://hydrotower1.local");
    }
  }
  else
  {
    startAP();
  }
}

// ==================================================
// JSON (FULL RESTORED)
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
// ROUTES (RESTORED + COMMAND BRIDGE)
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
    if(req->hasParam("on"))
      pump_on_minutes = req->getParam("on")->value().toInt();

    if(req->hasParam("cycle"))
      pump_cycle_minutes = req->getParam("cycle")->value().toInt();

    req->send(200, "text/plain", "OK");
  });

server.on("/water", HTTP_GET, [](AsyncWebServerRequest *req)
{
  watering_active = !watering_active;

  if(watering_active)
  {
    system_cmd = CMD_WATER_ON;
  }
  else
  {
    system_cmd = CMD_WATER_OFF;
  }

  req->send(200, "text/plain", watering_active ? "ON" : "OFF");
});
}

// ==================================================
// INIT
// ==================================================
void web_init()
{
  connectWiFi();
  setupRoutes();
  server.begin();
}

// ==================================================
// LOOP (manual trigger logging optional)
// ==================================================
void web_loop()
{
  if(system_cmd == CMD_WATER_ON)
  {
    web_log("Manual watering triggered");
  }
}