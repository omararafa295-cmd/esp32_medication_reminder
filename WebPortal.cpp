#include "WebPortal.h"
#include "AppConfig.h"
#include "DoseRegistry.h"
#include "SoundAlerter.h"
#include "PowerGuard.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer s_server(80);
static volatile bool s_portalRunning = false;
static uint32_t s_portalStartTime = 0;

// ============================================================================
// EMBEDDED RESPONSIVE HTML/CSS/JS DASHBOARD (Stored in Flash PROGMEM)
// ============================================================================
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MedReminder Dashboard</title>
<style>
:root{--bg:#0f172a;--card:#1e293b;--text:#f8fafc;--text-muted:#94a3b8;--accent:#38bdf8;--green:#22c55e;--red:#ef4444;--border:#334155;}
*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;}
body{background:var(--bg);color:var(--text);padding:16px;max-width:600px;margin:0 auto;}
header{display:flex;justify-content:space-between;align-items:center;padding:12px 0 20px;}
h1{font-size:1.25rem;font-weight:700;color:var(--accent);display:flex;align-items:center;gap:8px;}
.badge{background:#0284c7;color:#fff;font-size:0.75rem;padding:4px 8px;border-radius:12px;font-weight:600;}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:16px;margin-bottom:16px;box-shadow:0 4px 6px -1px rgba(0,0,0,0.2);}
.card-title{font-size:0.9rem;text-transform:uppercase;color:var(--text-muted);font-weight:600;margin-bottom:12px;letter-spacing:0.5px;}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}
.stat-box{background:#0f172a;border:1px solid var(--border);border-radius:12px;padding:12px;text-align:center;}
.stat-val{font-size:1.6rem;font-weight:800;color:var(--text);margin-top:4px;}
.stat-label{font-size:0.75rem;color:var(--text-muted);}
.val-green{color:var(--green);}
.val-red{color:var(--red);}
.progress-bar{background:#334155;border-radius:8px;height:10px;overflow:hidden;margin-top:10px;}
.progress-fill{background:var(--green);height:100%;width:0%;transition:width 0.4s ease;}
label{font-size:0.85rem;color:var(--text-muted);display:block;margin-bottom:6px;}
select,input{width:100%;padding:10px;background:#0f172a;border:1px solid var(--border);border-radius:8px;color:#fff;font-size:1rem;margin-bottom:12px;}
.btn{width:100%;padding:12px;border:none;border-radius:10px;font-size:0.95rem;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:10px;transition:0.2s;}
.btn-primary{background:#0284c7;color:#fff;}
.btn-primary:active{background:#0369a1;}
.btn-success{background:#16a34a;color:#fff;}
.btn-success:active{background:#15803d;}
.btn-danger{background:#dc2626;color:#fff;}
.btn-danger:active{background:#b91c1c;}
.btn-secondary{background:#475569;color:#fff;}
.btn-secondary:active{background:#334155;}
.status-msg{text-align:center;font-size:0.85rem;color:var(--accent);margin-top:8px;min-height:1.2rem;}
footer{text-align:center;font-size:0.75rem;color:var(--text-muted);margin-top:20px;}
</style>
</head>
<body>
<header>
  <h1>💊 MedReminder</h1>
  <span class="badge" id="connBadge">AP Mode (192.168.4.1)</span>
</header>

<div class="card">
  <div class="card-title">Medication Adherence Metrics</div>
  <div class="grid">
    <div class="stat-box">
      <div class="stat-label">Total Scheduled</div>
      <div class="stat-val" id="valTotal">--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Confirmed Taken</div>
      <div class="stat-val val-green" id="valTaken">--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Unanswered / Missed</div>
      <div class="stat-val val-red" id="valMissed">--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Adherence Rate</div>
      <div class="stat-val" id="valAdherence">--%</div>
    </div>
  </div>
  <div class="progress-bar"><div class="progress-fill" id="progFill"></div></div>
</div>

<div class="card">
  <div class="card-title">Medication Interval Configuration</div>
  <label for="intervalSelect">Select Reminder Schedule:</label>
  <select id="intervalSelect" onchange="onIntervalPresetChange()">
    <option value="15">15 Seconds (Lab Testing / Demo)</option>
    <option value="60">1 Minute</option>
    <option value="3600">1 Hour</option>
    <option value="14400">4 Hours (6 times daily)</option>
    <option value="21600">6 Hours (4 times daily)</option>
    <option value="28800" selected>8 Hours (3 times daily)</option>
    <option value="43200">12 Hours (Twice daily)</option>
    <option value="86400">24 Hours (Once daily)</option>
    <option value="custom">Custom Interval (Seconds)...</option>
  </select>

  <div id="customDiv" style="display:none;">
    <label for="customSec">Custom Interval in Seconds:</label>
    <input type="number" id="customSec" min="5" max="86400" value="15">
  </div>

  <button class="btn btn-success" onclick="saveSettings()">💾 Save Schedule & Deep Sleep</button>
</div>

<div class="card">
  <div class="card-title">Diagnostic & Hardware Controls</div>
  <button class="btn btn-primary" onclick="testBuzzer()">🔔 Test Buzzer & Status LED</button>
  <button class="btn btn-danger" onclick="resetStats()">🔄 Reset Adherence Counters</button>
  <button class="btn btn-secondary" onclick="enterSleep()">💤 Exit Setup & Return to Sleep</button>
  <div class="status-msg" id="statusMsg">Connected to ESP32 Web Portal</div>
</div>

<footer>ESP32 Medication Reminder • Open Source by Omar Arafa</footer>

<script>
function showMsg(txt, color="#38bdf8"){const el=document.getElementById("statusMsg");el.textContent=txt;el.style.color=color;}
function fetchStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById("valTotal").textContent = d.sequence;
    document.getElementById("valTaken").textContent = d.taken;
    document.getElementById("valMissed").textContent = d.missed;
    const adh = parseFloat(d.adherence).toFixed(1);
    document.getElementById("valAdherence").textContent = adh + "%";
    document.getElementById("progFill").style.width = Math.min(100, Math.max(0, adh)) + "%";
    
    // Set interval dropdown
    const sel = document.getElementById("intervalSelect");
    let matched = false;
    for(let i=0; i<sel.options.length; i++){
      if(parseInt(sel.options[i].value) === d.interval){
        sel.selectedIndex = i;
        matched = true;
        break;
      }
    }
    if(!matched && sel.value !== "custom"){
      sel.value = "custom";
      document.getElementById("customDiv").style.display = "block";
      document.getElementById("customSec").value = d.interval;
    }
  }).catch(e=>showMsg("Error syncing status", "#ef4444"));
}

function onIntervalPresetChange(){
  const val = document.getElementById("intervalSelect").value;
  document.getElementById("customDiv").style.display = (val === "custom") ? "block" : "none";
}

function saveSettings(){
  let sec = document.getElementById("intervalSelect").value;
  if(sec === "custom"){
    sec = parseInt(document.getElementById("customSec").value);
  } else {
    sec = parseInt(sec);
  }
  if(isNaN(sec) || sec < 5){
    showMsg("Minimum interval is 5 seconds", "#ef4444");
    return;
  }
  showMsg("Saving new interval...", "#38bdf8");
  fetch('/api/settings?interval=' + sec, {method:'POST'}).then(r=>r.json()).then(d=>{
    showMsg("Saved! Entering Deep Sleep in 2s...", "#22c55e");
    setTimeout(()=>{
      fetch('/api/sleep', {method:'POST'}).then(()=>{
        showMsg("ESP32 in Deep Sleep. You can disconnect Wi-Fi.", "#22c55e");
      });
    }, 1500);
  });
}

function testBuzzer(){
  showMsg("Testing buzzer & LED...", "#38bdf8");
  fetch('/api/test-buzzer', {method:'POST'}).then(r=>r.json()).then(()=>{
    showMsg("Hardware test executed successfully!", "#22c55e");
  });
}

function resetStats(){
  if(!confirm("Reset all medication counters to zero?")) return;
  fetch('/api/reset', {method:'POST'}).then(r=>r.json()).then(()=>{
    showMsg("All statistics reset to zero.", "#22c55e");
    fetchStatus();
  });
}

function enterSleep(){
  showMsg("Entering Deep Sleep...", "#38bdf8");
  fetch('/api/sleep', {method:'POST'}).then(()=>{
    showMsg("ESP32 in Deep Sleep. Disconnect Wi-Fi.", "#22c55e");
  });
}

fetchStatus();
setInterval(fetchStatus, 3000);
</script>
</body>
</html>
)rawliteral";

// ============================================================================
// HTTP ROUTE HANDLERS
// ============================================================================
static void handleRoot() {
    s_server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handleStatus() {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"sequence\":%u,\"taken\":%u,\"missed\":%u,\"adherence\":%.1f,\"interval\":%u}",
        DoseRegistry::getDoseSequence(),
        DoseRegistry::getTakenCount(),
        DoseRegistry::getMissedCount(),
        DoseRegistry::getAdherenceRate(),
        DoseRegistry::getSleepIntervalSec()
    );
    s_server.send(200, "application/json", json);
}

static void handleSettings() {
    if (s_server.hasArg("interval")) {
        uint32_t newInterval = s_server.arg("interval").toInt();
        DoseRegistry::setSleepIntervalSec(newInterval);
        s_server.send(200, "application/json", "{\"status\":\"saved\"}");
    } else {
        s_server.send(400, "application/json", "{\"error\":\"missing interval\"}");
    }
}

static void handleTestBuzzer() {
    SoundAlerter::playTestBuzzerChirp();
    s_server.send(200, "application/json", "{\"status\":\"tested\"}");
}

static void handleReset() {
    DoseRegistry::resetStatistics();
    s_server.send(200, "application/json", "{\"status\":\"reset\"}");
}

static void handleSleep() {
    s_server.send(200, "application/json", "{\"status\":\"sleeping\"}");
    s_portalRunning = false;
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================
bool WebPortal::isActive() {
    return s_portalRunning;
}

void WebPortal::stopPortalAndSleep() {
    s_portalRunning = false;
}

void WebPortal::startPortal() {
    Serial.println("\n==================================================");
    Serial.println("  STARTING LOCAL CONFIGURATION WEB PORTAL (AP)    ");
    Serial.println("==================================================");

    // 1. Configure and start Access Point
    WiFi.mode(WIFI_AP);
    bool apOk = WiFi.softAP(AP_WIFI_SSID, AP_WIFI_PASSWORD, AP_WIFI_CHANNEL, 0, AP_MAX_CLIENTS);

    if (!apOk) {
        Serial.println("[AP] ERROR: softAP failed to start.");
        return;
    }

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[AP] Access Point Started: \"%s\"\n", AP_WIFI_SSID);
    Serial.printf("[AP] Connect Wi-Fi and open browser at: http://%s/\n", apIP.toString().c_str());

    // 2. Register HTTP endpoints
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/api/status", HTTP_GET, handleStatus);
    s_server.on("/api/settings", HTTP_POST, handleSettings);
    s_server.on("/api/test-buzzer", HTTP_POST, handleTestBuzzer);
    s_server.on("/api/reset", HTTP_POST, handleReset);
    s_server.on("/api/sleep", HTTP_POST, handleSleep);

    s_server.begin();
    Serial.println("[HTTP] Embedded Web Server started on port 80.");

    s_portalRunning   = true;
    s_portalStartTime = millis();

    uint32_t lastLedBlink = millis();
    bool ledState = false;

    // 3. Process web client requests until sleep is requested or timeout expires
    while (s_portalRunning) {
        s_server.handleClient();

        // Heartbeat LED flash (toggles every 1 second)
        if (millis() - lastLedBlink >= 1000) {
            lastLedBlink = millis();
            ledState = !ledState;
            digitalWrite(PIN_STATUS_LED, ledState ? HIGH : LOW);
        }

        // Automatic timeout guard (3 minutes) to avoid battery drain
        if ((millis() - s_portalStartTime) >= AP_PORTAL_TIMEOUT_MS) {
            Serial.println("\n[AP] Portal safety timeout expired (3 min inactivity).");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // Feed FreeRTOS watchdog
    }

    // 4. Teardown Wi-Fi & WebServer before entering Deep Sleep
    Serial.println("[AP] Shutting down Web Server and Wi-Fi Access Point...");
    digitalWrite(PIN_STATUS_LED, LOW);
    s_server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // 5. Enter Deep Sleep using the configured interval
    uint32_t sleepInterval = DoseRegistry::getSleepIntervalSec();
    Serial.printf("[AP] Returning to Deep Sleep with schedule interval: %u seconds.\n", sleepInterval);
    PowerGuard::enterDeepSleep((uint64_t)sleepInterval * 1000000ULL);
}
