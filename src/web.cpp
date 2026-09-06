#include "web.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <fsm.hpp>
#include <time.h>
#include <settings.hpp>
#include <sensor.hpp>
#include <Preferences.h>

Preferences prefs;
AsyncWebServer server(80);

// ---------------- WiFi ----------------
bool apMode = false;
String ssid;
String password;


// ---------------- COMMAND BRIDGE ----------------
portMUX_TYPE cmdMux = portMUX_INITIALIZER_UNLOCKED;
system_cmd_t system_cmd = CMD_NONE;

void setCommand(system_cmd_t cmd, const String& name)
{
  portENTER_CRITICAL(&cmdMux);
  system_cmd = cmd;
  portEXIT_CRITICAL(&cmdMux);

  web_log(String("CMD ") + name);
}

std::array<pump_dir, 5> pumps = {
  pump_dir::STOP, // MAIN_PUMP
  pump_dir::STOP, // PH_PLUS
  pump_dir::STOP, // PH_MINUS
  pump_dir::STOP, // FERTILIZER_A
  pump_dir::STOP  // FERTILIZER_B
};

extern fsm_t fsm;
extern uint32_t remaining_seconds;

// ---------------- LOG ----------------
#define LOG_SIZE 40
String logBuffer[LOG_SIZE];
int logIndex = 0;

// ---------------- HISTORY ----------------
#define MAX_POINTS 1000
float ec_hist[MAX_POINTS];
float ph_hist[MAX_POINTS];
float temp_hist[MAX_POINTS];
int hist_index = 0;
bool hist_full = false;
void saveHistory(bool log = true);
void loadHistory();

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
// LOGGING
// ==================================================
void web_log(const String &msg)
{
  String line = "[" + getTimeString() + "] " + msg;
  Serial.println(line);
  logBuffer[logIndex] = line;
  logIndex = (logIndex + 1) % LOG_SIZE;
}

void web_pumps(pumps_t pump, pump_dir dir, float duty)
{
  // get actual dir
  auto actualDir = pumps[static_cast<size_t>(pump)];

  // set new dir
  pumps[static_cast<size_t>(pump)] = dir;

  // trace in case of new dir
  if(actualDir != dir)
  {
    String pumpName;
    switch(pump)
    {
      case pumps_t::MAIN_PUMP: pumpName = "MAIN_PUMP"; break;
      case pumps_t::PH_PLUS: pumpName = "PH_PLUS"; break;
      case pumps_t::PH_MINUS: pumpName = "PH_MINUS"; break;
      case pumps_t::FERTILIZER_A: pumpName = "FERTILIZER_A"; break;
      case pumps_t::FERTILIZER_B: pumpName = "FERTILIZER_B"; break;
    }

    String dirName;
    switch(dir)
    {
      case pump_dir::STOP: dirName = "STOP"; break;
      case pump_dir::PUMP: dirName = "PUMP"; break;
      case pump_dir::REVERSE: dirName = "REVERSE"; break;
    }

    web_log(pumpName + String(" ") + dirName + String(" duty=") + duty );
  }
}

// ==================================================
// DATA BUFFER
// ==================================================
void web_add_data_hist(float ec_v, float ph_v, float temp)
{
  ec_hist[hist_index] = ec_v;
  ph_hist[hist_index] = ph_v;
  temp_hist[hist_index] = temp;

  hist_index++;

  if (hist_index >= MAX_POINTS)
  {
    hist_index = 0;
    hist_full = true;

    saveHistory();
  }

  web_log(String("HIST idx=") + hist_index +
          " ec=" + String(ec_v, 2) +
          " ph=" + String(ph_v, 2) +
          " t=" + String(temp, 2));
  
}

void saveCalibrationPh1()
{
    prefs.begin("calibration", false);
    prefs.putBool("ph1_valid", ph_cal_1.valid);
    prefs.putFloat("ph1_value", ph_cal_1.value);
    prefs.putFloat("ph1_temp", ph_cal_1.temp);
    prefs.putFloat("ph1_voltage", ph_cal_1.voltage);
    prefs.end();

    web_log(
        "PH1\n\tvalue=" + String(ph_cal_1.value, 2) +
        "\n\ttemp=" + String(ph_cal_1.temp, 1) +
        "\n\tvoltage=" + String(ph_cal_1.voltage, 3)
    );
}

void saveCalibrationPh2()
{
    prefs.begin("calibration", false);
    prefs.putBool("ph2_valid", ph_cal_2.valid);
    prefs.putFloat("ph2_value", ph_cal_2.value);
    prefs.putFloat("ph2_temp", ph_cal_2.temp);
    prefs.putFloat("ph2_voltage", ph_cal_2.voltage);
    prefs.end();

    web_log(
        "PH2\n\tvalue=" + String(ph_cal_2.value, 2) +
        "\n\ttemp=" + String(ph_cal_2.temp, 1) +
        "\n\tvoltage=" + String(ph_cal_2.voltage, 3)
    );
}

void saveCalibrationEc1()
{
    prefs.begin("calibration", false);
    prefs.putBool("ec1_valid", ec_cal_1.valid);
    prefs.putFloat("ec1_value", ec_cal_1.value);
    prefs.putFloat("ec1_temp", ec_cal_1.temp);
    prefs.putFloat("ec1_voltage", ec_cal_1.voltage);
    prefs.end();

    web_log(
        "EC1\n\tvalue=" + String(ec_cal_1.value, 2) +
        "\n\ttemp=" + String(ec_cal_1.temp, 1) +
        "\n\tvoltage=" + String(ec_cal_1.voltage, 3)
    );
}

void saveCalibrationEc2()
{
    prefs.begin("calibration", false);
    prefs.putBool("ec2_valid", ec_cal_2.valid);
    prefs.putFloat("ec2_value", ec_cal_2.value);
    prefs.putFloat("ec2_temp", ec_cal_2.temp);
    prefs.putFloat("ec2_voltage", ec_cal_2.voltage);
    prefs.end();

    web_log(
        "EC2\n\tvalue=" + String(ec_cal_2.value, 2) +
        "\n\ttemp=" + String(ec_cal_2.temp, 1) +
        "\n\tvoltage=" + String(ec_cal_2.voltage, 3)
    );
}

void clearEcHistory()
{
    for (int i = 0; i < MAX_POINTS; i++)
    {
        ec_hist[i] = 0.0f;
    }

    saveHistory(false);
}

void clearPhHistory()
{
    for (int i = 0; i < MAX_POINTS; i++)
    {
        ph_hist[i] = 0.0f;
    }

    saveHistory(false);
}

// ==================================================
// SAVE HISTORY
// ==================================================
void saveHistory(bool log)
{
  prefs.begin("history", false);

  prefs.putBytes("ec_hist", ec_hist, sizeof(ec_hist));
  prefs.putBytes("ph_hist", ph_hist, sizeof(ph_hist));
  prefs.putBytes("tmp_hist", temp_hist, sizeof(temp_hist));

  prefs.putBool("full", hist_full);
  prefs.putInt("index", hist_index);

  prefs.end();

  if (log)
  {
    web_log(
      "History saved\n\tpoints=" + String(MAX_POINTS) +
      "\n\tec=" + String(ec_hist[0], 2) +
      "\n\tph=" + String(ph_hist[0], 2) +
      "\n\ttemp=" + String(temp_hist[0], 2)
    );
  }
}

// ==================================================
// LOAD HISTORY
// ==================================================
void loadHistory()
{
  prefs.begin("history", true);

  size_t ecSize  = prefs.getBytesLength("ec_hist");
  size_t phSize  = prefs.getBytesLength("ph_hist");
  size_t tmpSize = prefs.getBytesLength("tmp_hist");

  if(ecSize == sizeof(ec_hist))
  {
    prefs.getBytes("ec_hist", ec_hist, sizeof(ec_hist));
  }

  if(phSize == sizeof(ph_hist))
  {
    prefs.getBytes("ph_hist", ph_hist, sizeof(ph_hist));
  }

  if(tmpSize == sizeof(temp_hist))
  {
    prefs.getBytes("tmp_hist", temp_hist, sizeof(temp_hist));
  }

  hist_full = prefs.getBool("full", false);
  hist_index = prefs.getInt("index", 0);

  prefs.end();

  if(hist_full)
  {
    hist_index = 0;

    web_log(
      "Load History\n\tpoints=" + String(MAX_POINTS) +
      "\n\tec=" + String(ec_hist[0], 2) +
      "\n\tph=" + String(ph_hist[0], 2) +
      "\n\ttemp=" + String(temp_hist[0], 2)
    );
  }
}

// ==================================================
// HTML
// ==================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>
body {
  font-family: Arial;
  text-align: center;
  background-color: #f0efed;
  margin: 0;
}
button { padding:10px; margin:5px; }

.tab { display:none; }

#main {
  display: flex;
  width: 100%;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
}

#chartEC, #chartPH, #chartTEMP {
  width: 350px !important;
  height: 150px !important;
  margin: 10px auto;
  display: block;
}

.dashboard {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}

.topBar {
  position: sticky;
  top: 0;
  left: 0;
  width: 100%;
  display: flex;
  flex-direction: column;
  padding: 10px 15px;
  box-sizing: border-box;
  background-color: #ddf9df;
  gap: 4px;
}

.topRow1 {
  display: flex;
  justify-content: space-between;
  align-items: center;
  width: 100%;
}

.title {
  font-weight: bold;
  font-size: 20px;
}

.topRow2 {
  display: flex;
  justify-content: space-between;
  align-items: center;
  width: 100%;
}

.tabBar {
  width: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 5px;
  flex-wrap: wrap;
}

.tabBtn {
  width: 60px;
  height: 25px;
  margin: 2px;
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  font-size: 14px;
  font-weight: bold;
  border-radius: 5px;
  background-color: #ddf9df;
  border: 1px solid #b7d8b9;
  cursor: pointer;
}

.buttonGrid {
  display: flex;
  justify-content: center;
  width: 100%;
}

.stateBtn {
  width: 90px;
  height: 42px;
  margin: 3px;
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  font-size: 12px;
  font-weight: bold;
  border-radius: 7px;
}

.activeTab {
  background-color: #8fd694;
  border: 2px solid #4f9c57;
}

.stateButtonGrid {
  display: grid;
  grid-template-columns: repeat(3, 90px);
  grid-template-rows: repeat(2, 42px);
  gap: 6px;
  width: fit-content;
  justify-content: center;
  margin: 0 auto;
}

.pumpLamps {
  display: flex;
  justify-content: space-evenly;
  align-items: center;

  width: 100%;
  margin: 2px 0 0 0;
  padding: 2px 0;
}

.lamp {
  width: 18px;
  height: 18px;
  border-radius: 50%;

  background: #888;
  box-shadow: inset 0 0 4px rgba(0,0,0,0.4);
}

.lamp.pump {
  background: #4caf50;
  box-shadow: 0 0 8px #4caf50;
}

.lamp.reverse {
  background: #f44336;
  box-shadow: 0 0 8px #f44336;
}

.lampWrap {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;

  flex: 1;
  min-width: 45px;
}

.lampLabel {
  font-size: 10px;
  font-weight: 600;
  color: #333;
  margin-bottom: 2px;
}

.lampWrap {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;

  flex: 1;
  min-width: 60px;
}

.lampLabel {
  font-size: 12px;
  font-weight: 600;
  color: #333;
  margin-bottom: 6px;
}

.calibGrid {
  width: 100%;
  max-width: 420px;
}

.timerRow.compact {
  display: grid;
  grid-template-columns: 70px 60px 70px 40px;
  gap: 4px;
  align-items: center;
  font-size: 12px;
}

.timerRow.compact .timerLabel {
  font-size: 12px;
}

.timerRow.compact .timerValue {
  width: 60px;
  height: 28px;
  font-size: 12px;
}

.timerRow.compact .timerUnit {
  font-size: 11px;
}

.timerRow.compact .calibBtn {
  height: 20px;
  font-size: 10px;
  padding: 0 5px;
  min-width: 70px;
}

.timerRow {
  display: grid;
  grid-template-columns: 140px 120px 60px;
  align-items: center;
  justify-content: center;
  gap: 8px;
  width: 100%;
}

.timerLabel {
  text-align: right;
  font-size: 13px;
  font-weight: bold;
}

.timerValue {
  width: 60px;
  height: 30px;
  font-size: 13px;
  text-align: center;
  box-sizing: border-box;
}

.timerUnit {
  text-align: left;
  font-size: 12px;
  font-weight: bold;
  box-sizing: border-box;
  opacity: 0.6;
}

/* ================= PARAM TAB ONLY ================= */

.paramRow {
  display: grid;
  grid-template-columns: 80px 60px 40px;
  gap: 2px;
  align-items: center;
  font-size: 12px;
}

.paramRow .timerLabel {
  font-size: 12px;
  text-align: right;
}

.paramRow .timerValue {
  width: 54px;
  height: 26px;
  font-size: 12px;
}

.paramRow .timerUnit {
  font-size: 11px;
  opacity: 0.7;
}

.graphGrid {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 16px;
  width: 100%;
  padding: 10px;
  box-sizing: border-box;
}

.chartBox {
  width: 95%;
  max-width: 700px;
  height: 180px;

  background: white;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);

  padding: 10px;
  box-sizing: border-box;

  display: flex;
  align-items: center;
  justify-content: center;
}

.chartBox canvas {
  width: 100% !important;
  height: 100% !important;
}

.calibRow {
  display: grid;
  grid-template-columns: 1fr 140px;
  align-items: center;
  gap: 12px;
  width: 100%;
}

.calibBtn {
  width: 100%;
  height: 55px;
}

.calibInputWrap {
  display: flex;
  align-items: center;
  gap: 6px;
  justify-content: flex-end;
}

.calibInput {
  width: 90px;
  height: 40px;
  font-size: 16px;
  text-align: center;
}

.unitLabel {
  font-size: 14px;
  opacity: 0.7;
  min-width: 45px;
}

</style>
</head>

<body>

<div class="topBar">

  <div class="topRow1">
    <div class="title">HydroTower</div>

    <div style="flex:1; display:flex; justify-content:center; gap:12px; align-items:center;">
      <div id="stateControl" style="font-weight:bold; font-size:16px;">---</div>
      <div style="font-weight:bold;">
        <span id="timerControl"></span>
        <span id="timerUnit">s</span>
      </div>
    </div>

    <div id="time">--</div>
  </div>

<div class="topRow2">

  <div>
    <span id="tempControlValue">0</span> °C
  </div>

  <div>
    <span id="ecControlValue">0</span> mS/cm
    <span id="ecControlCalStatus"></span>
  </div>

  <div>
    <span id="phControlValue">0</span> pH
    <span id="phControlCalStatus"></span>
  </div>

</div>

<!-- ================= PUMP STATUS ================= -->

<div class="pumpLamps">

  <div class="lampWrap">
    <div class="lampLabel">MAIN</div>
    <div class="lamp" id="lamp0"></div>
  </div>

  <div class="lampWrap">
    <div class="lampLabel">PH+</div>
    <div class="lamp" id="lamp1"></div>
  </div>

  <div class="lampWrap">
    <div class="lampLabel">PH-</div>
    <div class="lamp" id="lamp2"></div>
  </div>

  <div class="lampWrap">
    <div class="lampLabel">FERT A</div>
    <div class="lamp" id="lamp3"></div>
  </div>

  <div class="lampWrap">
    <div class="lampLabel">FERT B</div>
    <div class="lamp" id="lamp4"></div>
  </div>

</div>

  <div class="tabBar">
    <button class="tabBtn" data-tab="main" onclick="showTab('main')">Control</button>
    <button class="tabBtn" data-tab="graph" onclick="showTab('graph')">Graph</button>
    <button class="tabBtn" data-tab="param" onclick="showTab('param')">Param</button>
    <button class="tabBtn" data-tab="calib" onclick="showTab('calib')">Calib</button>
    <button class="tabBtn" data-tab="monitor" onclick="showTab('monitor')">Log</button>
  </div>

</div>


<!-- ================= CONTROL TAB ================= -->

<div id="main" class="tab" style="display:block;">

  <div class="dashboard">

    <div class="buttonGrid">

      <div class="stateButtonGrid">
        <button class="stateBtn" onclick="setState('/idle')">IDLE</button>
        <button class="stateBtn" onclick="setState('/stop')">STOP</button>

        <button class="stateBtn" onclick="setState('/water')">WATER</button>
        <button class="stateBtn" onclick="setState('/measure')">MEASURE</button>

        <button class="stateBtn" onclick="setState('/regulate')">REGULATE</button>
        <button class="stateBtn" onclick="setState('/flush')">FLUSH</button>
      </div>

    </div>

    <div style="
      display:flex;
      justify-content:center;
      margin-top:8px;
    ">

      <button
        class="stateBtn"
        id="pumpLockBtn"
        onclick="togglePumpLock()"
        style="
          width:160px;
          height:40px;
          font-size:12px;
          background:#d9534f;
          color:white;
          border:none;
          border-radius:6px;
          margin:0;
        ">
        PUMPS LOCKED
      </button>

    </div>

  </div>

  
<!-- ================= MANUAL PUMP CONTROL ================= -->

<div id="pumpControlSection"
     style="
       display:none;
       flex-direction:column;
       align-items:center;
       gap:6px;
     ">

  <hr style="width:260px;margin:15px 0 8px;">

  <h3 style="margin:5px 0;font-size:16px;">
    Manual Pump Control
  </h3>

  <div id="pumpControls"
       style="
         display:flex;
         flex-direction:column;
         gap:4px;
         width:300px;
       ">
  </div>

</div>
  

</div>


<!-- ================= GRAPH TAB ================= -->
<div id="graph" class="tab">

  <div class="graphGrid">

    <div class="chartBox">
      <canvas id="chartEC"></canvas>
    </div>

    <div class="chartBox">
      <canvas id="chartPH"></canvas>
    </div>

    <div class="chartBox">
      <canvas id="chartTEMP"></canvas>
    </div>

  </div>

</div>


<!-- ================= PARAM TAB ================= -->

<div id="param" class="tab">

    <div style="
      display:flex;
      justify-content:center;
      align-items:flex-start;
      gap:2px;
      flex-wrap:nowrap;
    ">

    <div style="
      width:fit-content;
      max-width:100%;
      display:flex;
      flex-direction:column;
      align-items:center;
      gap:10px;
    ">

      <!-- SAVE BUTTON -->
      <button
        onclick="save()"
        style="
          width:260px;
          height:40px;
          font-size:14px;
          background:#d9534f;
          color:white;
          border:none;
          border-radius:6px;
        ">
        Save
      </button>

      <!-- ===== COMPACT 2 COLUMN WRAP ===== -->
      <div style="
        display:flex;
        justify-content:center;
        align-items:flex-start;
        gap:10px;
        flex-wrap:nowrap;
      ">

        <!-- LEFT COLUMN -->
        <div style="
          display:flex;
          flex-direction:column;
          gap:6px;
        ">

          <div class="paramRow">
            <div class="timerLabel">EC Tgt</div>
            <input id="ecReg" class="timerValue" type="number" step="0.01">
            <div class="timerUnit">mS</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">EC Tol</div>
            <input id="ecTol" class="timerValue" type="number" step="0.01">
            <div class="timerUnit">-</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">PH Tgt</div>
            <input id="phReg" class="timerValue" type="number" step="0.01">
            <div class="timerUnit">pH</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">PH Tol</div>
            <input id="phTol" class="timerValue" type="number" step="0.01">
            <div class="timerUnit">±</div>
          </div>

        </div>

        <!-- RIGHT COLUMN -->
        <div style="
          display:flex;
          flex-direction:column;
          gap:6px;
        ">

          <div class="paramRow">
            <div class="timerLabel">Cycle</div>
            <input id="idle" class="timerValue" type="number">
            <div class="timerUnit">min</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">Water</div>
            <input id="water" class="timerValue" type="number">
            <div class="timerUnit">min</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">Flush</div>
            <input id="flush" class="timerValue" type="number">
            <div class="timerUnit">min</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">Fert</div>
            <input id="fertilize" class="timerValue" type="number">
            <div class="timerUnit">sec</div>
          </div>

          <div class="paramRow">
            <div class="timerLabel">pH D</div>
            <input id="phSeconds" class="timerValue" type="number">
            <div class="timerUnit">sec</div>
          </div>

        </div>

      </div>

      <!-- ================= SLEEP / WAKE INLINE ================= -->
      <div style="
        margin-top:25px;
        display:flex;
        flex-direction:column;
        align-items:center;
        gap:10px;
      ">

        <!-- SLEEP -->
        <div style="
          display:flex;
          align-items:center;
          gap:10px;
        ">
          <div class="timerLabel">Sleep</div>

          <div style="display:flex; align-items:center; gap:4px;">
            <input id="sleepStartHour"
                  type="number"
                  min="0" max="23"
                  style="width:45px; height:28px; text-align:center;">
            <span>:</span>
            <input id="sleepStartMinute"
                  type="number"
                  min="0" max="59"
                  style="width:45px; height:28px; text-align:center;">
          </div>

          <div class="timerUnit">START</div>
        </div>

        <!-- WAKE -->
        <div style="
          display:flex;
          align-items:center;
          gap:10px;
        ">
          <div class="timerLabel">Wake</div>

          <div style="display:flex; align-items:center; gap:4px;">
            <input id="sleepEndHour"
                  type="number"
                  min="0" max="23"
                  style="width:45px; height:28px; text-align:center;">
            <span>:</span>
            <input id="sleepEndMinute"
                  type="number"
                  min="0" max="59"
                  style="width:45px; height:28px; text-align:center;">
          </div>

          <div class="timerUnit">END</div>
        </div>

      </div>

    </div> <!-- inner container -->

  </div> <!-- centered wrapper -->

</div> <!-- PARAM TAB END -->


<!-- ================= CALIB TAB ================= -->

<div id="calib" class="tab">

  <div style="
    width:100%;
    display:flex;
    justify-content:center;
    padding:6px;
    box-sizing:border-box;
  ">

<div style="
  width:fit-content;
  max-width:100%;
  display:flex;
  flex-direction:column;
  align-items:center;
  gap:10px;
">

  <!-- ================= CALIBRATION ENABLE BUTTONS ================= -->

<div style="
  display:flex;
  gap:6px;
  width:330px;
">

  <button
    class="stateBtn"
    id="calibEcLockBtn"
    onclick="toggleEcCalibLock()"
    style="
      flex:1;
      height:40px;
      font-size:12px;
      background:#d9534f;
      color:white;
      border:none;
      border-radius:6px;
      margin:0;
    ">
    EC LOCKED
  </button>

  <button
    class="stateBtn"
    id="calibPhLockBtn"
    onclick="togglePhCalibLock()"
    style="
      flex:1;
      height:40px;
      font-size:12px;
      background:#d9534f;
      color:white;
      border:none;
      border-radius:6px;
      margin:0;
    ">
    PH LOCKED
  </button>

</div>

  <!-- ================================================= -->
  <!-- EC CALIBRATION SECTION - HIDDEN INITIALLY -->
  <!-- ================================================= -->

  <div id="ecCalibSection"
       style="
         display:none;
         flex-direction:column;
         gap:6px;
       ">

    <!-- EC1 -->

    <div class="timerRow compact">

      <button id="calib_ec_p1"
              class="calibBtn"
              onclick="setState('/calibrate_ec_p1?value=' + encodeURIComponent(document.getElementById('ecCal1Value').value))">
        EC1
      </button>

      <div style="
        font-size:11px;
        line-height:1.2;
        margin-left:6px;
        min-width:70px;
      ">

        <div id="live_ec_p1" style="display:none;">
          <span id="live_ec_voltage_p1">--</span> V<br>
          <span id="live_ec_temp_p1">--</span> °C
        </div>

        <div id="stored_ec_p1">
          <span id="cal_ec_p1_v">--</span><br>
          <span id="cal_ec_p1_temp">--</span>
        </div>

      </div>

      <input id="ecCal1Value"
             class="timerValue"
             type="number"
             step="0.01"
             placeholder="EC">

      <div class="timerUnit">mS/cm</div>

    </div>


    <!-- EC2 -->

    <div class="timerRow compact">

      <button id="calib_ec_p2"
              class="calibBtn"
              onclick="setState('/calibrate_ec_p2?value=' + encodeURIComponent(document.getElementById('ecCal2Value').value))">
        EC2
      </button>

      <div style="
        font-size:11px;
        line-height:1.2;
        margin-left:6px;
        min-width:70px;
      ">

        <div id="live_ec_p2" style="display:none;">
          <span id="live_ec_voltage_p2">--</span> V<br>
          <span id="live_ec_temp_p2">--</span> °C
        </div>

        <div id="stored_ec_p2">
          <span id="cal_ec_p2_v">--</span><br>
          <span id="cal_ec_p2_temp">--</span>
        </div>

      </div>

      <input id="ecCal2Value"
             class="timerValue"
             type="number"
             step="0.01"
             placeholder="EC">

      <div class="timerUnit">mS/cm</div>

    </div>

  </div>
  <!-- END EC CALIBRATION SECTION -->


  <!-- ================================================= -->
  <!-- PH CALIBRATION SECTION - HIDDEN INITIALLY -->
  <!-- ================================================= -->

  <div id="phCalibSection"
       style="
         display:none;
         flex-direction:column;
         gap:6px;
       ">

    <!-- PH1 -->

    <div class="timerRow compact">

      <button id="calib_ph_p1"
              class="calibBtn"
              onclick="setState('/calibrate_ph_p1?value=' + encodeURIComponent(document.getElementById('phCal1Value').value))">
        pH1
      </button>

      <div style="
        font-size:11px;
        line-height:1.2;
        margin-left:6px;
        min-width:70px;
      ">

        <div id="live_ph_p1" style="display:none;">
          <span id="live_ph_voltage_p1">--</span> V<br>
          <span id="live_ph_temp_p1">--</span> °C
        </div>

        <div id="stored_ph_p1">
          <span id="cal_ph_p1_v">--</span><br>
          <span id="cal_ph_p1_temp">--</span>
        </div>

      </div>

      <input id="phCal1Value"
             class="timerValue"
             type="number"
             step="0.01"
             placeholder="pH">

      <div class="timerUnit">pH</div>

    </div>


    <!-- PH2 -->

    <div class="timerRow compact">

      <button id="calib_ph_p2"
              class="calibBtn"
              onclick="setState('/calibrate_ph_p2?value=' + encodeURIComponent(document.getElementById('phCal2Value').value))">
        pH2
      </button>

      <div style="
        font-size:11px;
        line-height:1.2;
        margin-left:6px;
        min-width:70px;
      ">

        <div id="live_ph_p2" style="display:none;">
          <span id="live_ph_voltage_p2">--</span> V<br>
          <span id="live_ph_temp_p2">--</span> °C
        </div>

        <div id="stored_ph_p2">
          <span id="cal_ph_p2_v">--</span><br>
          <span id="cal_ph_p2_temp">--</span>
        </div>

      </div>

      <input id="phCal2Value"
             class="timerValue"
             type="number"
             step="0.01"
             placeholder="pH">

      <div class="timerUnit">pH</div>

    </div>

  </div>
  <!-- END PH CALIBRATION SECTION -->

<!-- ================= MONITOR TAB ================= -->

<div id="monitor" class="tab">

<div style="display:flex; justify-content:center; width:100%; padding:10px; box-sizing:border-box;">
  <pre id="log"
    style="
      text-align:left;
      width:100%;
      max-width:1200px;
      height:80vh;
      overflow:auto;
      background:rgba(255,255,255,0.7);
      color:black;
      padding:12px;
      border-radius:10px;
      box-shadow:0 2px 8px rgba(0,0,0,0.1);
      font-size:12px;
      line-height:1.4;
      white-space:pre-wrap;
      word-break:break-word;
    "></pre>
</div>

</div>

<script>

let calEcHist = [];
let calPhHist = [];
let calTempHist = [];

const CAL_POINTS = 60;
const MAX_HOURS = 72;

function showTab(id)
{
  // detect previous active tab
  let previousTab = null;

  document.querySelectorAll('.tab').forEach(e => {
    if(e.style.display !== 'none')
      previousTab = e.id;

    e.style.display = 'none';
  });

  // remove active style
  document.querySelectorAll('.tabBtn').forEach(btn => {
    btn.classList.remove('activeTab');
  });

  // show selected tab
  document.getElementById(id).style.display = 'block';

  // activate button
  document
    .querySelector(`.tabBtn[data-tab="${id}"]`)
    .classList.add('activeTab');

  // ================= GRAPH RESIZE =================

  if(id === 'graph')
  {
    setTimeout(() => {

      chartEC.resize();
      chartPH.resize();
      chartTEMP.resize();

      chartEC.update();
      chartPH.update();
      chartTEMP.update();

    }, 100);
  }
}

function setState(route)
{
  fetch(route.startsWith("/") ? route : "/" + route);
}


let ecCalibEnabled = false;
let phCalibEnabled = false;
let pumpControlEnabled = false;

function updatePumpLock()
{
    const section = document.getElementById('pumpControlSection');
    const lockBtn = document.getElementById('pumpLockBtn');

    if(section)
    {
        section.style.display =
            pumpControlEnabled ? 'flex' : 'none';
    }

    if(lockBtn)
    {
        if(pumpControlEnabled)
        {
            lockBtn.innerHTML = 'MANUAL CONTROL';
            lockBtn.style.background = '#4CAF50';
        }
        else
        {
            lockBtn.innerHTML = 'PUMPS LOCKED';
            lockBtn.style.background = '#d9534f';
        }
    }
}

function togglePumpLock()
{
    pumpControlEnabled = !pumpControlEnabled;
    updatePumpLock();
}


function updateEcCalibLock()
{
    const enabled = ecCalibEnabled;

    // Show / hide EC calibration section
    const section = document.getElementById('ecCalibSection');

    if (section) {
        section.style.display = enabled ? 'flex' : 'none';
    }

    // Enable / disable EC calibration buttons
    document.querySelectorAll(
        '#calib_ec_p1, #calib_ec_p2'
    ).forEach(btn => {
        btn.disabled = !enabled;
        btn.style.opacity = enabled ? '1.0' : '0.4';
    });

    // Show live values while EC calibration is enabled
    ['ec_p1', 'ec_p2'].forEach(id => {

        const live = document.getElementById('live_' + id);
        const stored = document.getElementById('stored_' + id);

        if (live) {
            live.style.display = enabled ? 'block' : 'none';
        }

        if (stored) {
            stored.style.display = enabled ? 'none' : 'block';
        }
    });

    // Update lock button
    const lockBtn = document.getElementById('calibEcLockBtn');

    if (lockBtn) {

        if (enabled) {
            lockBtn.innerHTML = 'EC ENABLED';
            lockBtn.style.background = '#4CAF50';
        }
        else {
            lockBtn.innerHTML = 'EC LOCKED';
            lockBtn.style.background = '#d9534f';
        }
    }
}

function updatePhCalibLock()
{
    const enabled = phCalibEnabled;

    // Show / hide PH calibration section
    const section = document.getElementById('phCalibSection');

    if (section) {
        section.style.display = enabled ? 'flex' : 'none';
    }

    // Enable / disable PH calibration buttons
    document.querySelectorAll(
        '#calib_ph_p1, #calib_ph_p2'
    ).forEach(btn => {
        btn.disabled = !enabled;
        btn.style.opacity = enabled ? '1.0' : '0.4';
    });

    // Show live values while PH calibration is enabled
    ['ph_p1', 'ph_p2'].forEach(id => {

        const live = document.getElementById('live_' + id);
        const stored = document.getElementById('stored_' + id);

        if (live) {
            live.style.display = enabled ? 'block' : 'none';
        }

        if (stored) {
            stored.style.display = enabled ? 'none' : 'block';
        }
    });

    // Update lock button
    const lockBtn = document.getElementById('calibPhLockBtn');

    if (lockBtn) {

        if (enabled) {
            lockBtn.innerHTML = 'PH ENABLED';
            lockBtn.style.background = '#4CAF50';
        }
        else {
            lockBtn.innerHTML = 'PH LOCKED';
            lockBtn.style.background = '#d9534f';
        }
    }
}

function toggleEcCalibLock()
{
  ecCalibEnabled = !ecCalibEnabled;

  if(ecCalibEnabled)
  {
    // Disable PH calibration first
    if(phCalibEnabled)
    {
      phCalibEnabled = false;
      fetch('/stop_measure_ph');
      updatePhCalibLock();
    }

    // Start EC measurement
    fetch('/start_measure_ec');
  }
  else
  {
    fetch('/stop_measure_ec');
  }

  updateEcCalibLock();
}


function togglePhCalibLock()
{
  phCalibEnabled = !phCalibEnabled;

  if(phCalibEnabled)
  {
    // Disable EC calibration first
    if(ecCalibEnabled)
    {
      ecCalibEnabled = false;
      fetch('/stop_measure_ec');
      updateEcCalibLock();
    }

    // Start PH measurement
    fetch('/start_measure_ph');
  }
  else
  {
    fetch('/stop_measure_ph');
  }

  updatePhCalibLock();
}

function setEcCal1Value() {
    ec_cal_1.value = Number(
        document.getElementById('ecCal1Value').value
    );
}

function setEcCal2Value() {
    ec_cal_2.value = Number(
        document.getElementById('ecCal2Value').value
    );
}

function setPhCal1Value() {
    ph_cal_1.value = Number(
        document.getElementById('phCal1Value').value
    );
}

function setPhCal2Value() {
    ph_cal_2.value = Number(
        document.getElementById('phCal2Value').value
    );
}


function createMiniChart(id, label, color, min, max)
{
  return new Chart(document.getElementById(id), {

    type: 'line',

    data: {
      labels: [],
      datasets: [{
        label: label,
        data: [],
        borderColor: color,
        borderWidth: 1,
        pointRadius: 0,
        tension: 0.3
      }]
    },

    options: {

      responsive: true,
      maintainAspectRatio: false,
      animation: false,

      plugins: {
        legend: {
          display: true,
          labels: {
            boxWidth: 8,
            font: {
              size: 8
            }
          }
        }
      },

      scales: {

        x: {
          display: false
        },

        y: {
          min: min,
          max: max,

          ticks: {
            font: {
              size: 8
            }
          }
        }
      }
    }
  });
}

const pumpList = [
  {name:"MAIN", route:"MAIN_PUMP", reverse:false},
  {name:"FERT A", route:"FERTILIZER_A", reverse:true},
  {name:"FERT B", route:"FERTILIZER_B", reverse:true},
  {name:"PH+", route:"PH_PLUS", reverse:true},
  {name:"PH-", route:"PH_MINUS", reverse:true}
];

function createPumpControls()
{
    const root = document.getElementById("pumpControls");

    pumpList.forEach(p => {

        let row = document.createElement("div");

        row.style.display = "grid";

        row.style.gridTemplateColumns =
            p.reverse
                ? "65px 65px 65px 65px"
                : "65px 65px 65px";

        row.style.gap = "3px";
        row.style.alignItems = "center";

        row.innerHTML =
        `<div style="
            font-weight:bold;
            font-size:11px;
            align-self:center;
         ">
            ${p.name}
         </div>

         <button
            style="
              height:28px;
              padding:2px 5px;
              margin:0;
              font-size:10px;
            "
            onclick="pumpCmd('${p.route}','pump')">
            PUMP
         </button>

         <button
            style="
              height:28px;
              padding:2px 5px;
              margin:0;
              font-size:10px;
            "
            onclick="pumpCmd('${p.route}','stop')">
            STOP
         </button>` +

        (p.reverse ?
        `<button
            style="
              height:28px;
              padding:2px 5px;
              margin:0;
              font-size:10px;
            "
            onclick="pumpCmd('${p.route}','reverse')">
            REV
         </button>`
        : "");

        root.appendChild(row);
    });
}

function pumpCmd(pump, dir)
{
    fetch(`/pump?motor=${pump}&dir=${dir}`);
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
  let fertilize = document.getElementById('fertilize').value;
  let phSeconds = document.getElementById('phSeconds').value;
  let sh = document.getElementById('sleepStartHour').value;
  let sm = document.getElementById('sleepStartMinute').value;
  let eh = document.getElementById('sleepEndHour').value;
  let em = document.getElementById('sleepEndMinute').value;
  let ecReg = document.getElementById('ecReg').value;
  let phReg = document.getElementById('phReg').value;
  let ecTol = document.getElementById('ecTol').value;
  let phTol = document.getElementById('phTol').value;

  await fetch(
    `/set?idle=${idle}` +
    `&water=${water}` +
    `&flush=${flush}` +
    `&fertilize=${fertilize}` +
    `&phSeconds=${phSeconds}` +
    `&sh=${sh}` +
    `&sm=${sm}` +
    `&eh=${eh}` +
    `&em=${em}` +
    `&ec=${ecReg}` +
    `&ph=${phReg}` +
    `&ecTol=${ecTol}` +
    `&phTol=${phTol}`
  );
 
}

Chart.defaults.animation = false;
Chart.defaults.font.weight = 'bold';
Chart.defaults.font.size = 12;
Chart.defaults.color = '#222';

let chartEC = new Chart(document.getElementById('chartEC'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [

      {
        label: 'EC',
        data: [],
        borderColor: '#2196F3',
        backgroundColor: 'rgba(0, 0, 255, 0.2)',
        borderWidth: 1,
        tension: 0.3,
        pointRadius: 0,
        pointHoverRadius: 0
      },

      {
        label: 'Target',
        data: [],
        borderColor: '#FF0000',
        borderWidth: 1,
        pointRadius: 0,
        tension: 0,
        borderDash: [8, 4]
      },

      {
        label: 'Regulate',
        data: [],
        borderColor: '#000000',
        borderWidth: 0.5,
        pointRadius: 0,
        tension: 0,
        borderDash: [3, 3]
      }

    ]
  },

  options: {
    responsive: false,
    maintainAspectRatio: false,

    plugins: {
      legend: {
        display: true,
        labels: {
          filter: function(item) {
            return item.text !== '';
          }
        }
      }
    },

    scales: {
      y: {
        min: 0,
        max: 3.0,

        title: {
          display: true,
          text: 'EC (mS/cm)',
          font: {
            size: 14,
            weight: 'bold'
          }
        }
      },

      x: {
        ticks: {
          callback: function(value, index) {

            let cycleMin = parseInt(document.getElementById('idle').value) || 1;
            return ((index * cycleMin) / 60.0).toFixed(1) + "h";
          },

          maxRotation: 0,
          autoSkip: true,
          maxTicksLimit: 8,

          font: {
            size: 12
          }
        },

        title: {
          display: true,
          text: 'Time (hours)',

          font: {
            size: 14,
            weight: 'bold'
          }
        }
      }
    }
  }
});

let chartPH = new Chart(document.getElementById('chartPH'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [

      {
        label: 'PH',
        data: [],
        borderColor: '#4CAF50',
        backgroundColor: 'rgba(0, 255, 0, 0.2)',
        borderWidth: 1,
        tension: 0.3,
        pointRadius: 0,
        pointHoverRadius: 0
      },

      {
        label: 'Target',
        data: [],
        borderColor: '#FF0000',
        borderWidth: 1,
        pointRadius: 0,
        tension: 0,
        borderDash: [8, 4]
      },

      {
        label: 'Regulate',
        data: [],
        borderColor: '#000000',
        borderWidth: 0.5,
        pointRadius: 0,
        tension: 0,
        borderDash: [3, 3]
      },

      {
        label: '',
        data: [],
        borderColor: '#000000',
        borderWidth: 0.5,
        pointRadius: 0,
        tension: 0,
        borderDash: [3, 3]
      }

    ]
  },

  options: {
    responsive: false,
    maintainAspectRatio: false,

    plugins: {
      legend: {
        display: true,
        labels: {
          filter: function(item) {
            return item.text !== '';
          }
        }
      }
    },

    scales: {
      y: {
        min: 4,
        max: 9,

        title: {
          display: true,
          text: 'pH',
          font: {
            size: 14,
            weight: 'bold'
          }
        }
      },

      x: {
        ticks: {
          callback: function(value, index) {

            let cycleMin = parseInt(document.getElementById('idle').value) || 1;
            return ((index * cycleMin) / 60.0).toFixed(1) + "h";
          },

          maxRotation: 0,
          autoSkip: true,
          maxTicksLimit: 8,

          font: {
            size: 12
          }
        },

        title: {
          display: true,
          text: 'Time (hours)',

          font: {
            size: 14,
            weight: 'bold'
          }
        }
      }
    }
  }
});

let chartTEMP = new Chart(document.getElementById('chartTEMP'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [

      {
        label: 'Temperature',
        data: [],
        borderColor: '#FF9800',
        backgroundColor: 'rgba(255, 152, 0, 0.2)',
        borderWidth: 1,
        tension: 0.3,
        pointRadius: 0,
        pointHoverRadius: 0
      }

    ]
  },

  options: {
    responsive: false,
    maintainAspectRatio: false,

    plugins: {
      legend: {
        display: true,
        labels: {
          filter: function(item) {
            return item.text !== '';
          }
        }
      }
    },

    scales: {
      y: {
        min: 4,
        max: 30,

        title: {
          display: true,
          text: '°C',
          font: {
            size: 14,
            weight: 'bold'
          }
        }
      },

      x: {
        ticks: {
          callback: function(value, index) {

            let cycleMin = parseInt(document.getElementById('idle').value) || 1;
            return ((index * cycleMin) / 60.0).toFixed(1) + "h";
          },

          maxRotation: 0,
          autoSkip: true,
          maxTicksLimit: 8,

          font: {
            size: 12
          }
        },

        title: {
          display: true,
          text: 'Time (hours)',

          font: {
            size: 14,
            weight: 'bold'
          }
        }
      }
    }
  }
});

let dirtyInputs = new Set();

function markDirty(id)
{
  dirtyInputs.add(id);
}

function clearDirty(id)
{
  dirtyInputs.delete(id);
}

function setIfNotDirty(id, value)
{
  const el = document.getElementById(id);
  if (!el) return;

  if (document.activeElement === el) return;
  
  if (dirtyInputs.has(id)) return;

  el.value = value;
}

async function update()
{
  try
  {
    const response = await fetch('/data');

    if(!response.ok)
      return;

    const d = await response.json();

    if(!d)
      return;

    // ================= PUMPS =================

    if(Array.isArray(d.pumps))
    {
      for(let i = 0; i < 5; i++)
      {
        const lamp = document.getElementById("lamp" + i);

        if(!lamp)
          continue;

        lamp.classList.remove("pump");
        lamp.classList.remove("reverse");

        switch(d.pumps[i])
        {
          case 1: // PUMP
            lamp.classList.add("pump");
            break;

          case 2: // REVERSE
            lamp.classList.add("reverse");
            break;

          default: // STOP
            break;
        }
      }
    }

    // ================= TOP BAR =================

    document.getElementById('time').innerText = d.time || "--";
    document.getElementById('stateControl').innerText = d.state || "---";

    const timerEl = document.getElementById('timerControl');
    const unitEl = document.getElementById('timerUnit');

    if (d.state === "STOP") {
      timerEl.innerText = "";
      unitEl.style.display = "none";
    } else {
      timerEl.innerText = (d.timer != null ? d.timer : 0);
      unitEl.style.display = "inline";
    }

    document.getElementById('ecControlValue').innerText =
        Number(d.ec || 0).toFixed(2);

    document.getElementById('ecControlCalStatus').innerText =
        (d.calib?.ec_p1?.valid && d.calib?.ec_p2?.valid)
          ? ''
          : ' (not calibrated)';

    document.getElementById('phControlValue').innerText =
        Number(d.ph || 0).toFixed(2);

    document.getElementById('phControlCalStatus').innerText =
        (d.calib?.ph_p1?.valid && d.calib?.ph_p2?.valid)
          ? ''
          : ' (not calibrated)';

    document.getElementById('tempControlValue').innerText =
      (d.temp != null ? d.temp : 0);

    // ================= CALIBRATION LIVE VALUES =================

    const ecVoltage =
      Number(d.vEcMeasure || 0).toFixed(3);

    const phVoltage =
      Number(d.vPhMeasure || 0).toFixed(3);

    const calibrationTemp =
      Number(d.tempMeasure || 0).toFixed(1);


    // ================= EC LIVE VALUES =================

    document.getElementById('live_ec_voltage_p1').innerText =
      ecVoltage;

    document.getElementById('live_ec_temp_p1').innerText =
      calibrationTemp;

    document.getElementById('live_ec_voltage_p2').innerText =
      ecVoltage;

    document.getElementById('live_ec_temp_p2').innerText =
      calibrationTemp;


    // ================= PH LIVE VALUES =================

    document.getElementById('live_ph_voltage_p1').innerText =
      phVoltage;

    document.getElementById('live_ph_temp_p1').innerText =
      calibrationTemp;

    document.getElementById('live_ph_voltage_p2').innerText =
      phVoltage;

    document.getElementById('live_ph_temp_p2').innerText =
      calibrationTemp;

    if(d.calib)
    {
      document.getElementById('cal_ec_p1_v').innerText =
        Number(d.calib.ec_p1.voltage).toFixed(3) + " V";

      document.getElementById('cal_ec_p1_temp').innerText =
        Number(d.calib.ec_p1.temp).toFixed(1) + " °C";

      document.getElementById('cal_ec_p2_v').innerText =
        Number(d.calib.ec_p2.voltage).toFixed(3) + " V";

      document.getElementById('cal_ec_p2_temp').innerText =
        Number(d.calib.ec_p2.temp).toFixed(1) + " °C";

      document.getElementById('cal_ph_p1_v').innerText =
        Number(d.calib.ph_p1.voltage).toFixed(3) + " V";
        
      document.getElementById('cal_ph_p1_temp').innerText =
        Number(d.calib.ph_p1.temp).toFixed(1) + " °C";

      document.getElementById('cal_ph_p2_v').innerText =
        Number(d.calib.ph_p2.voltage).toFixed(3) + " V";

      document.getElementById('cal_ph_p2_temp').innerText =
        Number(d.calib.ph_p2.temp).toFixed(1) + " °C";
    }

    calEcHist.push(Number(d.ec || 0));
    calPhHist.push(Number(d.ph || 0));
    calTempHist.push(Number(d.temp || 0));

    if(calEcHist.length > CAL_POINTS)
    {
      calEcHist.shift();
      calPhHist.shift();
      calTempHist.shift();
    }

    const labels =
      calEcHist.map((_, i) => i);

    // ================= CHARTS =================

    if(d.labels && d.ec_hist)
    {
      let cycleMin = parseInt(document.getElementById('idle').value) || 1;
      let maxPoints = Math.floor((72 * 60) / cycleMin);

      let labels = d.labels.slice(-maxPoints);
      let ec = d.ec_hist.slice(-maxPoints);

      chartEC.data.labels = labels;
      chartEC.data.datasets[0].data = ec;

      chartEC.data.datasets[1].data =
        labels.map(() => (d.ecReg ?? 0));

      chartEC.data.datasets[2].data =
        labels.map(() => (d.ecReg ?? 0) - (d.ecTol ?? 0));

      chartEC.update();
    }

    if(d.labels && d.ph_hist)
    {
      let cycleMin = parseInt(document.getElementById('idle').value) || 1;
      let maxPoints = Math.floor((72 * 60) / cycleMin);

      let labels = d.labels.slice(-maxPoints);
      let ph = d.ph_hist.slice(-maxPoints);

      chartPH.data.labels = labels;
      chartPH.data.datasets[0].data = ph;

      chartPH.data.datasets[1].data =
        labels.map(() => (d.phReg ?? 0));

      chartPH.data.datasets[2].data =
        labels.map(() => (d.phReg ?? 0) + (d.phTol ?? 0));

      chartPH.data.datasets[3].data =
        labels.map(() => (d.phReg ?? 0) - (d.phTol ?? 0));

      chartPH.update();
    }

    if(d.labels && d.temp_hist)
    {
      let cycleMin = parseInt(document.getElementById('idle').value) || 1;
      let maxPoints = Math.floor((72 * 60) / cycleMin);

      chartTEMP.data.labels = d.labels.slice(-maxPoints);
      chartTEMP.data.datasets[0].data = d.temp_hist.slice(-maxPoints);

      chartTEMP.update();
    }

    // ================= PARAMETERS =================

    setIfNotDirty('idle', d.idle != null ? d.idle : 0);
    setIfNotDirty('water', d.water != null ? d.water : 0);
    setIfNotDirty('flush', d.flush != null ? d.flush : 0);
    setIfNotDirty('fertilize', d.fertilize != null ? d.fertilize : 0);
    setIfNotDirty('phSeconds', d.phSeconds != null ? d.phSeconds : 0);

    setIfNotDirty('sleepStartHour', d.sh != null ? d.sh : 0);
    setIfNotDirty('sleepStartMinute', d.sm != null ? d.sm : 0);
    setIfNotDirty('sleepEndHour', d.eh != null ? d.eh : 0);
    setIfNotDirty('sleepEndMinute', d.em != null ? d.em : 0);

    setIfNotDirty('ecReg', d.ecReg != null ? d.ecReg : 0);
    setIfNotDirty('phReg', d.phReg != null ? d.phReg : 0);
    setIfNotDirty('ecTol', d.ecTol != null ? d.ecTol : 0);
    setIfNotDirty('phTol', d.phTol != null ? d.phTol : 0);

    const calibMap = {
      ecCal1Value: d.calib?.ec_p1?.value,
      ecCal2Value: d.calib?.ec_p2?.value,
      phCal1Value: d.calib?.ph_p1?.value,
      phCal2Value: d.calib?.ph_p2?.value,
    };

    for (const id in calibMap)
    {
      setIfNotDirty(id, calibMap[id] ?? 0);
    }

  }
  catch(err)
  {
    console.log("UPDATE ERROR:", err);
  }
}

async function updateLog(){
  let r = await fetch('/log');
  document.getElementById('log').innerText = await r.text();
}

setInterval(update, 500);
setInterval(updateLog, 1000);

update();
updateLog();

[
  'idle','water','flush','fertilize','phSeconds',
  'sleepStartHour','sleepStartMinute','sleepEndHour','sleepEndMinute',
  'ecReg','phReg','ecTol','phTol',
  'ecCal1Value','ecCal2Value','phCal1Value','phCal2Value'
].forEach(id => {

  const el = document.getElementById(id);
  if (!el) return;

  el.addEventListener('input', () => markDirty(id));
});

showTab('main');

createPumpControls();

// calibration starts LOCKED
updateEcCalibLock();
updatePhCalibLock();
updatePumpLock();

</script>

</body>

</html>
)rawliteral";

// ==================================================
// WIFI + AP MODE
// ==================================================

bool startMDNS()
{
    if (MDNS.begin("hydrotower1"))
    {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS started");
        return true;
    }
    return false;
}

void startAP()
{
  apMode = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup");

  IPAddress ip = WiFi.softAPIP();
  Serial.println(ip);

  startMDNS();

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
  // -------- WIFI --------
  prefs.begin("wifi", true);
  ssid               = prefs.getString("ssid", "");
  password           = prefs.getString("pass", "");
  prefs.end();

  // -------- CONFIG --------
  prefs.begin("config", true);
  cycle_time_minutes = prefs.getInt("idle", 10);
  watering_minutes   = prefs.getInt("water", 5);
  flush_minutes      = prefs.getInt("flush", 3);
  fertilize_seconds  = prefs.getInt("fertilize", 10);
  ph_seconds         = prefs.getInt("ph_seconds", 10);
  sleep_start_hour   = prefs.getInt("sh", 22);
  sleep_start_minute = prefs.getInt("sm", 0);
  sleep_end_hour     = prefs.getInt("eh", 6);
  sleep_end_minute   = prefs.getInt("em", 0);
  ec_regulator       = prefs.getFloat("ec", 1.80);
  ec_tolerance       = prefs.getFloat("ec_tol", 0.20f);
  ph_regulator       = prefs.getFloat("ph", 6.00);
  ph_tolerance       = prefs.getFloat("ph_tol", 0.30f);
  prefs.end();

  web_log(
    "Load Parameter\n\tidle=" + String(cycle_time_minutes) +
    "\n\twater=" + String(watering_minutes) +
    "\n\tflush=" + String(flush_minutes) +
    "\n\tfertilize=" + String(fertilize_seconds) +
    "\n\tph_seconds=" + String(ph_seconds) +
    "\n\tsleep=" + String(sleep_start_hour) + ":" + String(sleep_start_minute) + "-" + String(sleep_end_hour) + ":" + String(sleep_end_minute) +
    "\n\tec=" + String(ec_regulator, 2) +
    "\n\tph=" + String(ph_regulator, 2) +
    "\n\tec_tol=" + String(ec_tolerance, 2) +
    "\n\tph_tol=" + String(ph_tolerance, 2)
  );

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
  
    startMDNS();

    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
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
  json.reserve(12000);

  // Basic values
  String stateName = fsm.stateName();
  json += "\"state\":\"" + stateName + "\",";
  json += "\"timer\":" + String(remaining_seconds) + ",";
 
  json += "\"ec\":" + String(ecMeasure, 2) + ",";
  json += "\"ph\":" + String(phMeasure, 2) + ",";
  json += "\"ecTol\":" + String(ec_tolerance, 2) + ",";
  json += "\"phTol\":" + String(ph_tolerance, 2) + ",";
  json += "\"temp\":" + String(tempMeasure, 2) + ",";
  json += "\"tempMeasure\":" + String(tempMeasure, 2) + ",";
  json += "\"vTempMeasure\":" + String(vTempMeasure, 3) + ",";
  json += "\"vEcMeasure\":" + String(vEcMeasure, 3) + ",";
  json += "\"vPhMeasure\":" + String(vPhMeasure, 3) + ",";

  json += "\"time\":\"" + getTimeString() + "\",";
  json += "\"idle\":" + String(cycle_time_minutes) + ",";
  json += "\"water\":" + String(watering_minutes) + ",";
  json += "\"flush\":" + String(flush_minutes) + ",";
  json += "\"fertilize\":" + String(fertilize_seconds) + ",";
  json += "\"phSeconds\":" + String(ph_seconds) + ",";
  json += "\"sh\":" + String(sleep_start_hour) + ",";
  json += "\"sm\":" + String(sleep_start_minute) + ",";
  json += "\"eh\":" + String(sleep_end_hour) + ",";
  json += "\"em\":" + String(sleep_end_minute) + ",";
  json += "\"ecReg\":" + String(ec_regulator, 2) + ",";
  json += "\"phReg\":" + String(ph_regulator, 2) + ",";


  json += "\"calib\":{";
  json += "\"ec_p1\":{";
  json += "\"valid\":" + String(ec_cal_1.valid ? "true" : "false") + ",";
  json += "\"value\":" + String(ec_cal_1.value, 2) + ",";
  json += "\"temp\":" + String(ec_cal_1.temp, 2) + ",";
  json += "\"voltage\":" + String(ec_cal_1.voltage, 3) + "},";
  json += "\"ec_p2\":{";
  json += "\"valid\":" + String(ec_cal_2.valid ? "true" : "false") + ",";
  json += "\"value\":" + String(ec_cal_2.value, 2) + ",";
  json += "\"temp\":" + String(ec_cal_2.temp, 2) + ",";
  json += "\"voltage\":" + String(ec_cal_2.voltage, 3) + "},";
  json += "\"ph_p1\":{";
  json += "\"valid\":" + String(ph_cal_1.valid ? "true" : "false") + ",";
  json += "\"value\":" + String(ph_cal_1.value, 2) + ",";
  json += "\"temp\":" + String(ph_cal_1.temp, 2) + ",";
  json += "\"voltage\":" + String(ph_cal_1.voltage, 3) + "},";
  json += "\"ph_p2\":{";
  json += "\"valid\":" + String(ph_cal_2.valid ? "true" : "false") + ",";
  json += "\"value\":" + String(ph_cal_2.value, 2) + ",";
  json += "\"temp\":" + String(ph_cal_2.temp, 2) + ",";
  json += "\"voltage\":" + String(ph_cal_2.voltage, 3) + "}";
  json += "},";


  json += "\"pumps\":[";
  for(int i = 0; i < 5; i++)
  {
      json += String(static_cast<int>(pumps[i]));

      if(i < 4)
          json += ",";
  }
  json += "],";

  json += "\"img\":\"\",";

  // History count
  int count = hist_full ? MAX_POINTS : hist_index;

  // Labels
  json += "\"labels\":[";

  for(int i = 0; i < count; i++)
  {
    int start = hist_full ? hist_index : 0;
    int idx = (start + i) % MAX_POINTS;

    json += "\"" + String(idx) + "\"";

    if(i < count - 1)
      json += ",";
  }

  json += "],";

  // EC history
  json += "\"ec_hist\":[";

  for(int i = 0; i < count; i++)
  {
    int start = hist_full ? hist_index : 0;
    int idx = (start + i) % MAX_POINTS;

    json += String(ec_hist[idx], 2);

    if(i < count - 1)
      json += ",";
  }

  json += "],";

  // PH history
  json += "\"ph_hist\":[";

  for(int i = 0; i < count; i++)
  {
    int start = hist_full ? hist_index : 0;
    int idx = (start + i) % MAX_POINTS;

    json += String(ph_hist[idx], 2);

    if(i < count - 1)
      json += ",";
  }

  json += "],";

  // Temperature history
  json += "\"temp_hist\":[";
  for(int i = 0; i < count; i++)
  {
    int start = hist_full ? hist_index : 0;
    int idx = (start + i) % MAX_POINTS;

    json += String(temp_hist[idx], 2);

    if(i < count - 1)
      json += ",";
  }

  json += "]";

  // CLOSE JSON OBJECT
  json += "}";

  return json;
}


// ==================================================
// CALIBRATION LOAD
// ==================================================
void loadCalibration()
{
  prefs.begin("calibration", true);

  // ---------- EC ----------
  ec_cal_1.valid = prefs.getBool("ec1_valid", false);
  ec_cal_2.valid = prefs.getBool("ec2_valid", false);

  ec_cal_1.value   = prefs.getFloat("ec1_value", 0);
  ec_cal_1.temp    = prefs.getFloat("ec1_temp", 25);
  ec_cal_1.voltage = prefs.getFloat("ec1_voltage", 0);

  ec_cal_2.value   = prefs.getFloat("ec2_value", 0);
  ec_cal_2.temp    = prefs.getFloat("ec2_temp", 25);
  ec_cal_2.voltage = prefs.getFloat("ec2_voltage", 0);

  // ---------- PH ----------
  ph_cal_1.valid = prefs.getBool("ph1_valid", false);
  ph_cal_2.valid = prefs.getBool("ph2_valid", false);

  ph_cal_1.value   = prefs.getFloat("ph1_value", 0);
  ph_cal_1.temp    = prefs.getFloat("ph1_temp", 25);
  ph_cal_1.voltage = prefs.getFloat("ph1_voltage", 0);

  ph_cal_2.value   = prefs.getFloat("ph2_value", 0);
  ph_cal_2.temp    = prefs.getFloat("ph2_temp", 25);
  ph_cal_2.voltage = prefs.getFloat("ph2_voltage", 0);

  prefs.end();

  // ---------- VALIDATION ----------
  if(ec_cal_1.valid && ec_cal_2.valid)
  {
      web_log(
        "Load EC calibration\n\tP1=" +
        String(ec_cal_2.value, 2) +
        "\n\tV1=" +
        String(ec_cal_1.value, 2) +
        "\n\tP2=" +
        String(ec_cal_1.voltage, 3) +
        "\n\tV2=" +
        String(ec_cal_2.voltage, 3)
      );
  }
  else{
      web_log("EC calibration not found!");
  }

  if(ph_cal_1.valid && ph_cal_2.valid)
  {
      web_log(
        "Load PH calibration\n\tP1=" + String(ph_cal_1.value, 2) +
        "\tV1=" + String(ph_cal_1.voltage, 3) +
        "\n\tP2=" + String(ph_cal_2.value, 2) +
        "\tV2=" + String(ph_cal_2.voltage, 3)
      );
  }
  else{
      web_log("PH calibration not found!");
  }
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

  // build data object for the web page

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    req->send(200, "application/json", buildJson());
  });

  // start calibrations

  server.on("/calibrate_ec_p1", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (request->hasParam("value"))
      {
          float value = request->getParam("value")->value().toFloat();
          ec_cal_1.value = value;
      }

      setCommand(CMD_CALIBRATE_EC1, "CALIBRATE EC P1");

      request->send(200, "text/plain", "OK");
  });

  server.on("/calibrate_ec_p2", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (request->hasParam("value"))
      {
          float value = request->getParam("value")->value().toFloat();
          ec_cal_2.value = value;
      }

      setCommand(CMD_CALIBRATE_EC2, "CALIBRATE EC P2");
      request->send(200, "text/plain", "OK");
  });

  server.on("/calibrate_ph_p1", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (request->hasParam("value"))
      {
          float value = request->getParam("value")->value().toFloat();
          ph_cal_1.value = value;
      }

      setCommand(CMD_CALIBRATE_PH1, "CALIBRATE PH P1");
      request->send(200, "text/plain", "OK");
  });

  server.on("/calibrate_ph_p2", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (request->hasParam("value"))
      {
          float value = request->getParam("value")->value().toFloat();
          ph_cal_2.value = value;
      }

      setCommand(CMD_CALIBRATE_PH2, "CALIBRATE PH P2");
      request->send(200, "text/plain", "OK");
  });


  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    String out;

    for(int i = 0; i < LOG_SIZE; i++)
    {
      int idx = (logIndex - 1 - i + LOG_SIZE) % LOG_SIZE;

      if(logBuffer[idx].length())
      {
        out += logBuffer[idx] + "\n";
      }
    }

    req->send(200, "text/plain", out);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    prefs.begin("config", false);

    if(req->hasParam("idle"))
    {
      cycle_time_minutes = req->getParam("idle")->value().toInt();
      prefs.putInt("idle", cycle_time_minutes);
    }

    if(req->hasParam("water"))
    {
      watering_minutes = req->getParam("water")->value().toInt();
      prefs.putInt("water", watering_minutes);
    }

    if(req->hasParam("flush"))
    {
      flush_minutes = req->getParam("flush")->value().toInt();
      prefs.putInt("flush", flush_minutes);
    }

    if(req->hasParam("fertilize"))
    {
      fertilize_seconds = req->getParam("fertilize")->value().toInt();
      prefs.putInt("fertilize", fertilize_seconds);
    }

    if(req->hasParam("phSeconds"))
    {
      ph_seconds = req->getParam("phSeconds")->value().toInt();
      prefs.putInt("ph_seconds", ph_seconds);
    }

    if(req->hasParam("sh"))
    {
      sleep_start_hour = req->getParam("sh")->value().toInt();
      prefs.putInt("sh", sleep_start_hour);
    }

    if(req->hasParam("sm"))
    {
      sleep_start_minute = req->getParam("sm")->value().toInt();
      prefs.putInt("sm", sleep_start_minute);
    }

    if(req->hasParam("eh"))
    {
      sleep_end_hour = req->getParam("eh")->value().toInt();
      prefs.putInt("eh", sleep_end_hour);
    }

    if(req->hasParam("em"))
    {
      sleep_end_minute = req->getParam("em")->value().toInt();
      prefs.putInt("em", sleep_end_minute);
    }

    if(req->hasParam("ec"))
    {
      ec_regulator = req->getParam("ec")->value().toFloat();
      prefs.putFloat("ec", ec_regulator);
    }

    if(req->hasParam("ph"))
    {
      ph_regulator = req->getParam("ph")->value().toFloat();
      prefs.putFloat("ph", ph_regulator);
    }

    if(req->hasParam("ecTol"))
    {
      ec_tolerance = req->getParam("ecTol")->value().toFloat();
      prefs.putFloat("ec_tol", ec_tolerance);
    }

    if(req->hasParam("phTol"))
    {
      ph_tolerance = req->getParam("phTol")->value().toFloat();
      prefs.putFloat("ph_tol", ph_tolerance);
    }

    prefs.end();

    web_log(
      "Saved\n\tidle=" + String(cycle_time_minutes) +
      "\n\twater=" + String(watering_minutes) +
      "\n\tflush=" + String(flush_minutes) +
      "\n\tfertilize=" + String(fertilize_seconds) +
      "\n\tph_seconds=" + String(ph_seconds) +
      "\n\tsleep=" + String(sleep_start_hour) + ":" + String(sleep_start_minute) + "-" + String(sleep_end_hour) + ":" + String(sleep_end_minute) +
      "\n\tec=" + String(ec_regulator, 2) +
      "\n\tph=" + String(ph_regulator, 2) +
      "\n\tec_tol=" + String(ec_tolerance, 2) +
      "\n\tph_tol=" + String(ph_tolerance, 2)
    );

    req->send(200, "text/plain", "Saved");
  });

  server.on("/start_measure_ec", HTTP_GET,
  [](AsyncWebServerRequest *req)
  {
    setStopLock(true);
    fsm.transitionTo(&STATE_STOP);

    enableEc();
    fsm.scheduler().startTask(sensor_proc_task);

    req->send(200, "text/plain", "EC measurement started");
  });


  server.on("/start_measure_ph", HTTP_GET,
  [](AsyncWebServerRequest *req)
  {
    setStopLock(true);
    fsm.transitionTo(&STATE_STOP);

    enablePh();
    fsm.scheduler().startTask(sensor_proc_task);

    req->send(200, "text/plain", "PH measurement started");
  });

  server.on("/stop_measure_ec", HTTP_GET,
  [](AsyncWebServerRequest *req)
  {
    disableEc();
    fsm.scheduler().stopTask(sensor_proc_task);

    setStopLock(false);

    req->send(200, "text/plain", "EC measurement stopped");
  });

  server.on("/stop_measure_ph", HTTP_GET,
  [](AsyncWebServerRequest *req)
  {
    disablePh();
    fsm.scheduler().stopTask(sensor_proc_task);

    setStopLock(false);

    req->send(200, "text/plain", "PH measurement stopped");
  });

  server.on("/pump", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    if(!req->hasParam("motor") || !req->hasParam("dir"))
    {
      req->send(400, "text/plain", "missing params");
      return;
    }

    String motor = req->getParam("motor")->value();
    String dir   = req->getParam("dir")->value();

    pumps_t pump;
    pump_dir direction;

    // ---- map motor string → enum ----
    if(motor == "MAIN_PUMP") pump = pumps_t::MAIN_PUMP;
    else if(motor == "PH_PLUS") pump = pumps_t::PH_PLUS;
    else if(motor == "PH_MINUS") pump = pumps_t::PH_MINUS;
    else if(motor == "FERTILIZER_A") pump = pumps_t::FERTILIZER_A;
    else if(motor == "FERTILIZER_B") pump = pumps_t::FERTILIZER_B;
    else
    {
      req->send(400, "text/plain", "bad motor");
      return;
    }

    // ---- map dir string → enum ----
    if(dir == "stop") direction = pump_dir::STOP;
    else if(dir == "pump") direction = pump_dir::PUMP;
    else if(dir == "reverse") direction = pump_dir::REVERSE;
    else
    {
      req->send(400, "text/plain", "bad dir");
      return;
    }

    setPump(pump, direction);

    req->send(200, "text/plain", "OK");
  });

  server.on("/idle", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_IDLE)
      {
          setCommand(CMD_IDLE, "IDLE");
      }

      request->send(200, "text/plain", "OK");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_STOP)
      {
          setCommand(CMD_STOP, "STOP");
      }

      request->send(200, "text/plain", "OK");
  });

  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_WATERING)
      {
          setCommand(CMD_WATER, "WATER");
      }

      request->send(200, "text/plain", "OK");
  });

  server.on("/measure", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_MEASURE)
      {
          setCommand(CMD_MEASURE, "MEASURE");
      }

      request->send(200, "text/plain", "OK");
  });

  server.on("/regulate", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_REGULATE)
      {
          setCommand(CMD_REGULATE, "REGULATE");
      }

      request->send(200, "text/plain", "OK");
  });

  server.on("/flush", HTTP_GET, [](AsyncWebServerRequest *request)
  {
      if (fsm.current != &STATE_FLUSH)
      {
          setCommand(CMD_FLUSH, "FLUSH");
      }

      request->send(200, "text/plain", "OK");
  });
}

// ==================================================
void web_init()
{
  connectWiFi();
  loadHistory();
  loadCalibration();
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