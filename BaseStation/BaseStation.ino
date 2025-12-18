/****************************************************
   BASE ESP32

   - WiFi AP: "RobotNet" / "12345678"
   - TCP server on port 3333:
        Receives:
          POS,x,y,id,dir
          OBS,x,y,id
   - Tracks:
        Robot 1 & Robot 2 position + direction
        Obstacles on 4x4 grid (x,y = 1..4)
   - Web server:
        GET /      -> serves HTML map UI
        GET /map   -> JSON with robots + obstacles

   - Prints on Serial when Robot1/Robot2 connects/disconnects (by MAC)
****************************************************/

#include <WiFi.h>
#include <WebServer.h>

// ========== WiFi AP SETTINGS ==========
const char* ap_ssid = "RobotNet";
const char* ap_pass = "12345678";
const int TCP_PORT  = 3333;

// ========== ROBOT MAC ADDRESSES ==========
static const uint8_t ROBOT1_MAC[6] = {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58};
static const uint8_t ROBOT2_MAC[6] = {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C};

volatile bool robot1Connected = false;
volatile bool robot2Connected = false;

bool macEquals(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
  return true;
}

void printMac(const uint8_t* mac) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    const uint8_t* mac = info.wifi_ap_staconnected.mac;

    Serial.print("Device connected: ");
    printMac(mac);
    Serial.println();

    if (macEquals(mac, ROBOT1_MAC)) { robot1Connected = true; Serial.println("Robot 1 CONNECTED"); }
    else if (macEquals(mac, ROBOT2_MAC)) { robot2Connected = true; Serial.println("Robot 2 CONNECTED"); }
    else { Serial.println("Unknown device connected"); }
  }

  if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    const uint8_t* mac = info.wifi_ap_stadisconnected.mac;

    Serial.print("Device disconnected: ");
    printMac(mac);
    Serial.println();

    if (macEquals(mac, ROBOT1_MAC)) { robot1Connected = false; Serial.println("Robot 1 DISCONNECTED"); }
    else if (macEquals(mac, ROBOT2_MAC)) { robot2Connected = false; Serial.println("Robot 2 DISCONNECTED"); }
    else { Serial.println("Unknown device disconnected"); }
  }
}

// ========== STRUCTS & GLOBAL STATE ==========
struct RobotState {
  int x;        // 1..4
  int y;        // 1..4
  int dir;      // 0=UP,1=RIGHT,2=DOWN,3=LEFT
  bool active;  // true after first POS message
};

RobotState robots[3];   // index 1 and 2 used
bool obstacles[5][5];   // obstacles[y][x] (1..4)

// ========== SERVERS ==========
WebServer webServer(80);
WiFiServer tcpServer(TCP_PORT);

// ========== EMBEDDED HTML PAGE ==========
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Robot Fleet Map</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    .container {
      background:#fff;
      border-radius:20px;
      box-shadow:0 20px 60px rgba(0,0,0,0.3);
      padding:30px;
      max-width:700px;
      width:100%;
    }
    h1 { text-align:center; color:#667eea; margin-bottom:10px; }
    .subtitle { text-align:center; color:#666; margin-bottom:20px; }

    .grid {
      display:grid;
      grid-template-columns: repeat(4,80px);
      grid-template-rows: repeat(4,80px);
      gap:8px;
      margin:0 auto 15px auto;
      padding:15px;
      background:#f8f9fa;
      border-radius:15px;
      box-shadow: inset 0 2px 8px rgba(0,0,0,0.1);
      width: fit-content;
    }

    .cell {
      width:80px; height:80px;
      background:linear-gradient(135deg,#ffffff 0%,#f0f0f0 100%);
      border-radius:10px;
      border:2px solid #ddd;
      display:flex;
      align-items:center;
      justify-content:center;
      position:relative;
      font-size:1.3rem;
      font-weight:bold;
      transition:all .2s ease;
    }

    .cellCoords {
      position:absolute;
      top:4px;
      left:6px;
      font-size:0.6rem;
      color:#aaa;
    }

    .obstacle { background:linear-gradient(135deg,#ff6b6b 0%,#ee5a6f 100%); color:#fff; border-color:#ff5252; }
    .robot1   { background:linear-gradient(135deg,#51cf66 0%,#37b24d 100%); color:#fff; border-color:#2fb344; }
    .robot2   { background:linear-gradient(135deg,#339af0 0%,#1c7ed6 100%); color:#fff; border-color:#1c7ed6; }

    .legend { display:flex; justify-content:center; gap:20px; margin-top:10px; }
    .legend-item { display:flex; align-items:center; gap:8px; font-size:0.9rem; color:#555; }
    .legend-box { width:18px; height:18px; border-radius:4px; border:2px solid #ddd; }
    .lg-empty { background:linear-gradient(135deg,#ffffff 0%,#f0f0f0 100%); }
    .lg-r1    { background:linear-gradient(135deg,#51cf66 0%,#37b24d 100%); }
    .lg-r2    { background:linear-gradient(135deg,#339af0 0%,#1c7ed6 100%); }
    .lg-obs   { background:linear-gradient(135deg,#ff6b6b 0%,#ee5a6f 100%); }

    .footer { margin-top:15px; text-align:center; font-size:0.85rem; color:#888; }
    .status-bar { display:flex; justify-content:space-between; margin-bottom:10px; font-size:0.9rem; color:#555; }
    .dot { display:inline-block; width:10px; height:10px; border-radius:50%; background:#51cf66; margin-right:6px; animation:blink 1.8s infinite; }
    @keyframes blink { 0%,100% { opacity:1;} 50% { opacity:0.3;} }
  </style>
</head>
<body>
  <div class="container">
    <h1>Robot Fleet Map</h1>
    <p class="subtitle">Real-time positions, directions and obstacles</p>

    <div class="status-bar">
      <div><span class="dot"></span><span id="connStatus">Updating...</span></div>
      <div>Grid: 4 × 4</div>
    </div>

    <div class="grid" id="grid"></div>

    <div class="legend">
      <div class="legend-item"><div class="legend-box lg-empty"></div>Empty</div>
      <div class="legend-item"><div class="legend-box lg-r1"></div>Robot 1</div>
      <div class="legend-item"><div class="legend-box lg-r2"></div>Robot 2</div>
      <div class="legend-item"><div class="legend-box lg-obs"></div>Obstacle</div>
    </div>

    <div class="footer">
      <div>Arrows show robot headings: ↑, →, ↓, ←</div>
    </div>
  </div>

  <script>
    const gridElem = document.getElementById('grid');
    const connStatus = document.getElementById('connStatus');

    // ✅ CORRECT: build row by row (y first, then x)
    // Row1: (1,1)(2,1)(3,1)(4,1)
    // Row4: (1,4)(2,4)(3,4)(4,4)
    for (let y = 1; y <= 4; y++) {
      for (let x = 1; x <= 4; x++) {
        const cell = document.createElement('div');
        cell.className = 'cell';
        cell.id = `cell-${x}-${y}`;

        const coordLabel = document.createElement('div');
        coordLabel.className = 'cellCoords';
        coordLabel.textContent = `(${x},${y})`;
        cell.appendChild(coordLabel);

        gridElem.appendChild(cell);
      }
    }

    function dirToArrow(dir) {
      switch (dir) {
        case 0: return '↑';
        case 1: return '→';
        case 2: return '↓';
        case 3: return '←';
        default: return '?';
      }
    }

    async function updateGrid() {
      try {
        const res = await fetch('/map?ts=' + Date.now()); // avoid caching
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();
        connStatus.textContent = 'Live / Last update OK';

        // clear
        for (let y = 1; y <= 4; y++) {
          for (let x = 1; x <= 4; x++) {
            const c = document.getElementById(`cell-${x}-${y}`);
            c.classList.remove('robot1', 'robot2', 'obstacle');
            while (c.childNodes.length > 1) c.removeChild(c.lastChild);
          }
        }

        // obstacles
        if (Array.isArray(data.obstacles)) {
          data.obstacles.forEach(o => {
            const cell = document.getElementById(`cell-${o.x}-${o.y}`);
            if (cell) cell.classList.add('obstacle');
          });
        }

        // robots
        if (Array.isArray(data.robots)) {
          data.robots.forEach(r => {
            if (!r.active) return;
            const cell = document.getElementById(`cell-${r.x}-${r.y}`);
            if (!cell) return;

            const span = document.createElement('div');
            span.textContent = r.id + dirToArrow(r.dir);
            cell.appendChild(span);

            if (r.id === 1) cell.classList.add('robot1');
            if (r.id === 2) cell.classList.add('robot2');
          });
        }
      } catch (e) {
        console.error('Update error:', e);
        connStatus.textContent = 'Error talking to base...';
      }
    }

    updateGrid();
    setInterval(updateGrid, 500);
  </script>
</body>
</html>
)rawliteral";

// ========== HELPERS ==========
void initState() {
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 5; x++)
      obstacles[y][x] = false;

  for (int i = 0; i < 3; i++) {
    robots[i].x = 1;
    robots[i].y = 1;
    robots[i].dir = 0;
    robots[i].active = false;
  }

  // Defaults (will be overwritten by first POS)
  robots[1].x = 4; robots[1].y = 1; robots[1].dir = 0; robots[1].active = false;
  robots[2].x = 4; robots[2].y = 4; robots[2].dir = 0; robots[2].active = false;
}

void handleRobotLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("Robot msg: ");
  Serial.println(line);

  if (line.startsWith("POS")) {
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return;

    int x   = line.substring(p1 + 1, p2).toInt();
    int y   = line.substring(p2 + 1, p3).toInt();
    int id  = line.substring(p3 + 1, p4).toInt();
    int dir = line.substring(p4 + 1).toInt();

    if (id >= 1 && id <= 2 && x >= 1 && x <= 4 && y >= 1 && y <= 4 && dir >= 0 && dir <= 3) {
      robots[id].x = x;
      robots[id].y = y;
      robots[id].dir = dir;
      robots[id].active = true;
      Serial.printf("Updated Robot %d -> (%d,%d) dir=%d\n", id, x, y, dir);
    }
  }
  else if (line.startsWith("OBS")) {
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) return;

    int x  = line.substring(p1 + 1, p2).toInt();
    int y  = line.substring(p2 + 1, p3).toInt();
    int id = line.substring(p3 + 1).toInt();
    (void)id;

    if (x >= 1 && x <= 4 && y >= 1 && y <= 4) {
      obstacles[y][x] = true;
      Serial.printf("Obstacle at (%d,%d)\n", x, y);
    }
  }
}

// ========== HTTP ==========
void handleRoot() { webServer.send_P(200, "text/html", MAIN_page); }

void handleMap() {
  String json = "{";

  json += "\"robots\":[";
  for (int id = 1; id <= 2; id++) {
    if (id > 1) json += ",";
    json += "{";
    json += "\"id\":" + String(id) + ",";
    json += "\"x\":" + String(robots[id].x) + ",";
    json += "\"y\":" + String(robots[id].y) + ",";
    json += "\"dir\":" + String(robots[id].dir) + ",";
    json += "\"active\":" + String(robots[id].active ? "true" : "false");
    json += "}";
  }
  json += "],";

  json += "\"obstacles\":[";
  bool first = true;
  for (int y = 1; y <= 4; y++) {
    for (int x = 1; x <= 4; x++) {
      if (obstacles[y][x]) {
        if (!first) json += ",";
        first = false;
        json += "{\"x\":" + String(x) + ",\"y\":" + String(y) + "}";
      }
    }
  }
  json += "]";

  json += "}";

  webServer.send(200, "application/json", json);
}

// ========== SETUP & LOOP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("BASE HTML VERSION: GRID_Y_FIRST_v1");

  initState();

  WiFi.onEvent(onWiFiEvent);

  Serial.println("Starting AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  webServer.on("/", handleRoot);
  webServer.on("/map", handleMap);
  webServer.begin();
  Serial.println("HTTP server started");

  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.printf("TCP server started on port %d\n", TCP_PORT);
}

void loop() {
  webServer.handleClient();

  // Multi-client support
  static WiFiClient clients[4];

  WiFiClient newC = tcpServer.available();
  if (newC) {
    for (int i = 0; i < 4; i++) {
      if (!clients[i] || !clients[i].connected()) {
        if (clients[i]) clients[i].stop();
        clients[i] = newC;
        Serial.print("New TCP client in slot ");
        Serial.println(i);
        break;
      }
    }
  }

  for (int i = 0; i < 4; i++) {
    if (clients[i] && clients[i].connected()) {
      while (clients[i].available()) {
        String line = clients[i].readStringUntil('\n');
        handleRobotLine(line);
      }
    } else {
      if (clients[i]) clients[i].stop();
    }
  }

  delay(5);
}
