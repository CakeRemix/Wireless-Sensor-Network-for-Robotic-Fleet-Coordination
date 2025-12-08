#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// ---------------- MAC ADDRESSES ----------------
// Use the real MACs of your boards (these are the ones you gave me)
uint8_t BASE_MAC[]   = {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C}; // Base
uint8_t ROBOT1_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58}; // Robot 1
uint8_t ROBOT2_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C}; // Robot 2

// ---------------- MAP / MESSAGE DEFINITIONS ----------------
enum CellType : uint8_t {
  CELL_UNKNOWN = 0,
  CELL_FREE_WHITE = 1,
  CELL_FREE_BLACK = 2,
  CELL_OBSTACLE = 3
};

enum MsgType : uint8_t {
  MSG_OBSTACLE = 0,     // obstacle at (row,col)
  MSG_CLEAR    = 1      // scanned free cell at (row,col)
};

// MUST match the struct used in Robot1 & Robot2
struct __attribute__((packed)) MapMessage {
  uint8_t msgType;   // MsgType
  uint8_t fromId;    // 1 or 2
  uint8_t row;       // 1..4
  uint8_t col;       // 1..4
  uint8_t cellType;  // CellType
  uint8_t reserved;  // not used, just padding
};

// 4x4 grid, rows 1..4, cols 1..4 (we'll store 0..3 indexes)
CellType grid[4][4];

// ---------------- WEB SERVER ----------------
WebServer server(80);

// helper: convert CellType to character (for debug)
char cellToChar(CellType c) {
  switch (c) {
    case CELL_UNKNOWN:    return '?';
    case CELL_FREE_WHITE: return 'W';
    case CELL_FREE_BLACK: return 'B';
    case CELL_OBSTACLE:   return 'X';
    default:              return '?';
  }
}

// Build HTML for the current map
String buildHtmlPage() {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<title>Robot Map</title>"
    "<style>"
    "body{font-family:Arial;background:#111;color:#eee;text-align:center;}"
    "table{margin:auto;border-collapse:collapse;}"
    "td{width:60px;height:60px;border:1px solid #555;font-size:24px;font-weight:bold;}"
    ".boardW{background:#ffffff;color:#000000;}"
    ".boardB{background:#000000;color:#ffffff;}"
    ".obs{background:#ff3333 !important;color:#000;}"
    ".unknown{background:#666 !important;color:#000;}"
    "</style>"
    "</head><body>"
    "<h1>4x4 Robot Map</h1>"
    "<p>Refresh page to see updates.</p>"
    "<table>";

  // 4x4 chessboard style + overlay map info
  for (int r = 0; r < 4; r++) {
    html += "<tr>";
    for (int c = 0; c < 4; c++) {

      // Chessboard pattern: (r+c) even = white, odd = black
      bool boardWhite = ((r + c) % 2 == 0);
      String cls = boardWhite ? "boardW" : "boardB";
      String cellContent = "";

      CellType cell = grid[r][c];

      if (cell == CELL_UNKNOWN) {
        cls = "unknown";
        cellContent = "?";
      } else if (cell == CELL_OBSTACLE) {
        cls = "obs";
        cellContent = "X";
      } else if (cell == CELL_FREE_WHITE) {
        cellContent = "W";
      } else if (cell == CELL_FREE_BLACK) {
        cellContent = "B";
      }

      html += "<td class='" + cls + "'>" + cellContent + "</td>";
    }
    html += "</tr>";
  }

  html += "</table></body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", buildHtmlPage());
}

// ---------------- ESP-NOW RECEIVE CALLBACK ----------------
// New core 3.x signature
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(MapMessage)) {
    Serial.print("[BASE] Unknown packet size: ");
    Serial.println(len);
    return;
  }

  MapMessage msg;
  memcpy(&msg, data, sizeof(MapMessage));

  // Basic range check
  if (msg.row < 1 || msg.row > 4 || msg.col < 1 || msg.col > 4) {
    Serial.println("[BASE] Invalid row/col in message");
    return;
  }

  // Update local map
  int r = msg.row - 1;
  int c = msg.col - 1;
  grid[r][c] = (CellType)msg.cellType;

  Serial.print("[BASE] From robot ");
  Serial.print(msg.fromId);
  Serial.print(" -> (");
  Serial.print(msg.row);
  Serial.print(",");
  Serial.print(msg.col);
  Serial.print(")  type=");

  char ch = cellToChar(grid[r][c]);
  Serial.println(ch);
}

// ---------------- SETUP & LOOP ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize map as UNKNOWN
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      grid[r][c] = CELL_UNKNOWN;
    }
  }

  Serial.println("\n[BASE] Starting WiFi AP + ESP-NOW base station");

  // Start WiFi in AP+STA mode so:
  //  - AP side gives WiFi to your PC
  //  - STA side can be used by ESP-NOW
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(true);

  // Start Access Point for PC
  const char* ssid     = "RobotBase";
  const char* password = "12345678";   // min 8 chars

  // Channel 1 to match typical ESP-NOW default
  bool apOk = WiFi.softAP(ssid, password, 1, 0);
  IPAddress ip = WiFi.softAPIP();

  Serial.print("[BASE] AP started: ");
  Serial.println(apOk ? "OK" : "FAIL");
  Serial.print("[BASE] AP SSID: ");
  Serial.println(ssid);
  Serial.print("[BASE] AP IP:   ");
  Serial.println(ip);

  // ---- ESP-NOW init ----
  if (esp_now_init() != ESP_OK) {
    Serial.println("[BASE] Error initializing ESP-NOW!");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  Serial.println("[BASE] ESP-NOW ready, waiting for robots...");

  // ---- HTTP SERVER ----
  server.on("/", handleRoot);
  server.begin();
  Serial.println("[BASE] HTTP server started. Open http://192.168.4.1/ from PC.");
}

void loop() {
  // Handle incoming HTTP requests
  server.handleClient();
}
