// ================= ROBOT 1 NODE =================
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <math.h>

// ------------ IDs & MACs -------------
#define ROBOT_ID 1

uint8_t BASE_MAC[]   = {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C};
uint8_t OTHER_ROBOT_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C}; // Robot 2

// ------------ Pins (All on D13 side) -------------
#define IR_PIN      14          // IR sensor (white/black)
#define TRIG_PIN   26          // ultrasonic trig
#define ECHO_PIN   27          // ultrasonic echo
// IMU uses I2C: SDA=12, SCL=13

// ------------ Board & Map -------------
#define BOARD_SIZE 4

#define MSG_TYPE_MAP_UPDATE  1

#define CELL_UNKNOWN   0
#define CELL_CLEAR     1
#define CELL_OBSTACLE  2

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint8_t sourceId;
  uint8_t row;
  uint8_t col;
  uint8_t value;
  uint8_t hop;
} MapMessage;

int8_t mapGrid[BOARD_SIZE + 1][BOARD_SIZE + 1];  // 1..4 used
int currentRow = 4;   // Robot1 starts at (4,1)
int currentCol = 1;

// For IR movement detection
bool lastIRKnown = false;
bool lastIsWhite = false;

// Ultrasonic timing
const float OBSTACLE_DISTANCE_CM = 15.0;
unsigned long lastSonarMs = 0;
const unsigned long SONAR_INTERVAL_MS = 200;

// ------------ IMU (your code) -------------
MPU6050 mpu;

bool dmpReady = false;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];

// Robot direction (relative, not compass)
String currentDirection = "FORWARD";
const float ROTATION_THRESHOLD = 70.0;

// Map yaw → direction
String getDirection(float yaw) {
  if (yaw < 45 || yaw >= 315) return "FORWARD";
  if (yaw >= 45 && yaw < 135) return "RIGHT";
  if (yaw >= 135 && yaw < 225) return "BACKWARD";
  return "LEFT";  // 225–315
}

// Map direction → center angle
float directionAngle(String dir) {
  if (dir == "FORWARD") return 0;
  if (dir == "RIGHT") return 90;
  if (dir == "BACKWARD") return 180;
  if (dir == "LEFT") return 270;
  return 0;
}

// ---- This is exactly your IMU loop body, wrapped in a function ----
void updateIMU() {
  if (!dmpReady) return;

  if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {

    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    float yaw = ypr[0] * 180 / M_PI;
    if (yaw < 0) yaw += 360;

    // direction from yaw
    String newDir = getDirection(yaw);

    // angle difference
    float diff = fabs(yaw - directionAngle(currentDirection));
    if (diff > 180) diff = 360 - diff;

    // change only after large rotation
    if (newDir != currentDirection && diff >= ROTATION_THRESHOLD) {
      currentDirection = newDir;
      Serial.print("Direction: ");
      Serial.println(currentDirection);  // PRINT THE NEW DIRECTION
    }
  }
}

// ------------ ESP-NOW callbacks -------------
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional debug
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ESP-NOW send OK" : "ESP-NOW send fail");
}

void forwardMapMessage(const MapMessage &msg);

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(MapMessage)) {
    return;
  }
  MapMessage msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type != MSG_TYPE_MAP_UPDATE) return;
  if (msg.row < 1 || msg.row > BOARD_SIZE ||
      msg.col < 1 || msg.col > BOARD_SIZE) return;

  // Update local map
  mapGrid[msg.row][msg.col] = msg.value;

  Serial.print("[Robot1] Map update from Robot ");
  Serial.print(msg.sourceId);
  Serial.print(" -> cell(");
  Serial.print(msg.row);
  Serial.print(",");
  Serial.print(msg.col);
  Serial.print(") = ");
  Serial.println(msg.value == CELL_OBSTACLE ? "OBSTACLE" : "CLEAR");

  // Forward only once, and never forward our own original messages
  if (msg.hop == 0 && msg.sourceId != ROBOT_ID) {
    forwardMapMessage(msg);
  }
}

// ------------ ESP-NOW send helpers -------------
void sendMapUpdate(uint8_t row, uint8_t col, uint8_t value) {
  MapMessage msg;
  msg.type     = MSG_TYPE_MAP_UPDATE;
  msg.sourceId = ROBOT_ID;
  msg.row      = row;
  msg.col      = col;
  msg.value    = value;
  msg.hop      = 0;   // original info

  // Send to BOTH base station and other robot
  esp_now_send(BASE_MAC, (uint8_t*)&msg, sizeof(msg));
  esp_now_send(OTHER_ROBOT_MAC, (uint8_t*)&msg, sizeof(msg));
  
  Serial.print("[Robot");
  Serial.print(ROBOT_ID);
  Serial.print("] Sent update: (");
  Serial.print(row);
  Serial.print(",");
  Serial.print(col);
  Serial.print(") = ");
  Serial.println(value);
}

void forwardMapMessage(const MapMessage &msgIn) {
  MapMessage out = msgIn;
  out.hop = 1; // forwarded once

  // Rebroadcast to other robot and to base
  esp_now_send(OTHER_ROBOT_MAC, (uint8_t*)&out, sizeof(out));
  esp_now_send(BASE_MAC,        (uint8_t*)&out, sizeof(out));
}

// ------------ Ultrasonic -------------
float readUltrasonicCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) return -1;                   // no echo
  float distance = duration * 0.0343 / 2.0;       // speed of sound
  return distance;
}

// ------------ Position helpers -------------
void updatePositionFromDirection() {
  int newRow = currentRow;
  int newCol = currentCol;

  if (currentDirection == "FORWARD")      newRow--;
  else if (currentDirection == "BACKWARD") newRow++;
  else if (currentDirection == "RIGHT")   newCol++;
  else if (currentDirection == "LEFT")    newCol--;

  // Keep inside 4x4 board
  if (newRow < 1 || newRow > BOARD_SIZE || newCol < 1 || newCol > BOARD_SIZE) {
    Serial.println("[Robot1] Move would leave board, ignored");
    return;
  }

  currentRow = newRow;
  currentCol = newCol;

  if (mapGrid[currentRow][currentCol] != CELL_CLEAR) {
    mapGrid[currentRow][currentCol] = CELL_CLEAR;
    Serial.print("[Robot1] Moved to cell (");
    Serial.print(currentRow); Serial.print(",");
    Serial.print(currentCol); Serial.println(") CLEAR");
    sendMapUpdate(currentRow, currentCol, CELL_CLEAR);
  }
}

// ------------ Setup & Loop -------------
void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(IR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Init map
  for (int r = 1; r <= BOARD_SIZE; r++)
    for (int c = 1; c <= BOARD_SIZE; c++)
      mapGrid[r][c] = CELL_UNKNOWN;

  mapGrid[currentRow][currentCol] = CELL_CLEAR;

  // ---- IMU init (your setup code) ----
  Wire.begin(12, 13);  // SDA=12, SCL=13
  delay(1000);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 NOT CONNECTED!");
    while (1);
  }

  uint16_t status = mpu.dmpInitialize();
  if (status == 0) {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.setDMPEnabled(true);
    dmpReady = true;
    delay(2000);
  } else {
    Serial.println("DMP init failed!");
    while (1);
  }

  Serial.println("FORWARD");  // Initial direction

  // ---- ESP-NOW init ----
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  // Add peers: other robot + base
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, OTHER_ROBOT_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  memcpy(peer.peer_addr, BASE_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("[Robot1] Ready");
}

void loop() {
  // 1) Update orientation from IMU
  updateIMU();

  // 2) Detect motion by IR color changes (W <-> B)
  bool isWhite = digitalRead(IR_PIN);   // adjust if logic inverted
  if (!lastIRKnown) {
    lastIsWhite = isWhite;
    lastIRKnown = true;
  } else if (isWhite != lastIsWhite) {
    // crossed into next square
    lastIsWhite = isWhite;
    updatePositionFromDirection();
  }

  // 3) Periodically check ultrasonic for obstacle
  unsigned long now = millis();
  if (now - lastSonarMs >= SONAR_INTERVAL_MS) {
    lastSonarMs = now;
    float dist = readUltrasonicCm();
    if (dist > 0 && dist < OBSTACLE_DISTANCE_CM) {
      // obstacle in front of robot (one cell ahead)
      int r = currentRow;
      int c = currentCol;

      if (currentDirection == "FORWARD")      r--;
      else if (currentDirection == "BACKWARD") r++;
      else if (currentDirection == "RIGHT")   c++;
      else if (currentDirection == "LEFT")    c--;

      if (r >= 1 && r <= BOARD_SIZE && c >= 1 && c <= BOARD_SIZE) {
        if (mapGrid[r][c] != CELL_OBSTACLE) {
          mapGrid[r][c] = CELL_OBSTACLE;
          Serial.print("[Robot1] Obstacle at (");
          Serial.print(r); Serial.print(",");
          Serial.print(c); Serial.println(")");
          sendMapUpdate(r, c, CELL_OBSTACLE);
        }
      }
    }
  }

  delay(20);
}
