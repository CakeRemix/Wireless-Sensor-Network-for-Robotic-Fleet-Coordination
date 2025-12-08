#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <SPIFFS.h>

// ---------- MAC ADDRESSES OF ROBOTS ----------
uint8_t ROBOT1_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58}; // Robot 1
uint8_t ROBOT2_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C}; // Robot 2

// ---------- MAP DEFINITIONS ----------
enum CellType : uint8_t {
  CELL_UNKNOWN  = 0,
  CELL_CLEAR    = 1,
  CELL_OBSTACLE = 2,
  CELL_WHITE    = 3,
  CELL_BLACK    = 4
};

CellType grid[4][4];  // 4x4 board

char cellToChar(CellType c) {
  switch (c) {
    case CELL_CLEAR:    return '.';
    case CELL_OBSTACLE: return 'X';
    case CELL_WHITE:    return 'W';
    case CELL_BLACK:    return 'B';
    case CELL_UNKNOWN:
    default:            return '?';
  }
}

// ---------- MESSAGE FORMAT (MUST MATCH ROBOTS) ----------
typedef struct __attribute__((packed)) {
  uint8_t type;      // MSG_TYPE_MAP_UPDATE = 1
  uint8_t sourceId;  // 1 = Robot1, 2 = Robot2
  uint8_t row;       // 1..4
  uint8_t col;       // 1..4
  uint8_t value;     // CellType value
  uint8_t hop;       // 0 = original, 1 = forwarded
} MapMessage;

#define MSG_TYPE_MAP_UPDATE 1

// ---------- WEB SERVER ----------
WebServer server(80);

// Serve the index.html file from SPIFFS
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// API endpoint to get current map data as JSON
void handleMapData() {
  String json = "{\"grid\":[";
  
  for (int r = 0; r < 4; r++) {
    json += "[";
    for (int c = 0; c < 4; c++) {
      json += String((int)grid[r][c]);
      if (c < 3) json += ",";
    }
    json += "]";
    if (r < 3) json += ",";
  }
  
  json += "]}";
  
  // Add headers to prevent caching and allow CORS
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  server.send(200, "application/json", json);
  
  // Debug: print what we're sending
  Serial.print("Sending map data: ");
  Serial.println(json);
}

// ---------- ESP-NOW RECEIVE CALLBACK (ESP32 core 3.x) ----------
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(MapMessage)) {
    Serial.print("Received packet with unexpected size: ");
    Serial.print(len);
    Serial.print(" expected: ");
    Serial.println(sizeof(MapMessage));
    return;
  }

  MapMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // Check if it's a map update message
  if (msg.type != MSG_TYPE_MAP_UPDATE) {
    Serial.println("Received non-map-update message");
    return;
  }

  // RobotNode uses 1-based indexing (1..4), convert to 0-based (0..3)
  int r = msg.row - 1;
  int c = msg.col - 1;

  if (r < 0 || r >= 4 || c < 0 || c >= 4) {
    Serial.print("Received invalid coordinates: (");
    Serial.print(msg.row);
    Serial.print(",");
    Serial.print(msg.col);
    Serial.println(")");
    return;
  }

  grid[r][c] = static_cast<CellType>(msg.value);

  Serial.print("Update from Robot ");
  Serial.print(msg.sourceId);
  Serial.print(" -> cell (");
  Serial.print(msg.row);
  Serial.print(",");
  Serial.print(msg.col);
  Serial.print(") = ");
  Serial.print(msg.value);
  Serial.print(" (");
  Serial.print(cellToChar(static_cast<CellType>(msg.value)));
  Serial.println(")");

  // Re-broadcast the same update to both robots (only if not already forwarded)
  if (msg.hop == 0) {
    MapMessage fwd = msg;
    fwd.hop = 1;
    esp_now_send(ROBOT1_MAC, (uint8_t*)&fwd, sizeof(fwd));
    esp_now_send(ROBOT2_MAC, (uint8_t*)&fwd, sizeof(fwd));
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS mounted successfully");

  // Init map as unknown
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      grid[r][c] = CELL_UNKNOWN;
    }
  }

  // --- Wi-Fi AP for PC connection ---
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP("RobotBase", "12345678")) {
    Serial.println("Failed to start AP!");
  } else {
    Serial.print("AP started. Connect PC to SSID 'RobotBase' password '12345678'\nIP: ");
    Serial.println(WiFi.softAPIP());
  }

  // --- ESP-NOW setup ---
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  peer.channel = 0;     // same channel as AP
  peer.encrypt = false;

  // Add Robot 1
  memcpy(peer.peer_addr, ROBOT1_MAC, 6);
  esp_now_add_peer(&peer);

  // Add Robot 2
  memcpy(peer.peer_addr, ROBOT2_MAC, 6);
  esp_now_add_peer(&peer);

  // --- Web server routes ---
  server.on("/", handleRoot);
  server.on("/map", handleMapData);  // API endpoint for map data
  server.begin();
  Serial.println("HTTP server started on port 80");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();   // Handle browser requests
}
