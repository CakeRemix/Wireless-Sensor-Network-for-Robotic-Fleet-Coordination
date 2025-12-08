// ================= ROBOT 2 NODE =================
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <math.h>

#define ROBOT_ID 2

uint8_t BASE_MAC[]        = {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C};
uint8_t OTHER_ROBOT_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58}; // Robot 1

// Pins
#define IR_PIN    14      // IR sensor (white/black)
#define TRIG_PIN  26      // ultrasonic trig
#define ECHO_PIN  27      // ultrasonic echo

// Map / messages
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

int8_t mapGrid[BOARD_SIZE + 1][BOARD_SIZE + 1];
int currentRow = 4;   // Robot2 starts at (4,4)
int currentCol = 4;

// IR state
bool lastIRKnown = false;
bool lastIsWhite = false;

// Ultrasonic
const float OBSTACLE_DISTANCE_CM = 15.0;
unsigned long lastSonarMs = 0;
const unsigned long SONAR_INTERVAL_MS = 200;

// ---------- Direction handling (no String) ----------
enum Direction {
  DIR_FORWARD = 0,
  DIR_RIGHT,
  DIR_BACKWARD,
  DIR_LEFT
};

Direction currentDirection = DIR_FORWARD;
const float ROTATION_THRESHOLD = 70.0;

const char* directionToText(Direction d) {
  switch (d) {
    case DIR_FORWARD:  return "FORWARD";
    case DIR_RIGHT:    return "RIGHT";
    case DIR_BACKWARD: return "BACKWARD";
    case DIR_LEFT:     return "LEFT";
    default:           return "UNKNOWN";
  }
}

Direction getDirectionFromYaw(float yaw) {
  if (yaw < 45 || yaw >= 315) return DIR_FORWARD;
  if (yaw >= 45 && yaw < 135) return DIR_RIGHT;
  if (yaw >= 135 && yaw < 225) return DIR_BACKWARD;
  return DIR_LEFT; // 225–315
}

float directionAngle(Direction dir) {
  switch (dir) {
    case DIR_FORWARD:  return 0;
    case DIR_RIGHT:    return 90;
    case DIR_BACKWARD: return 180;
    case DIR_LEFT:     return 270;
    default:           return 0;
  }
}

// ---------- IMU ----------
MPU6050 mpu;
bool dmpReady = false;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];

void updateIMU() {
  if (!dmpReady) return;

  if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    float yaw = ypr[0] * 180.0f / M_PI;
    if (yaw < 0) yaw += 360.0f;

    Direction newDir = getDirectionFromYaw(yaw);

    float diff = fabsf(yaw - directionAngle(currentDirection));
    if (diff > 180.0f) diff = 360.0f - diff;

    if (newDir != currentDirection && diff >= ROTATION_THRESHOLD) {
      currentDirection = newDir;
      Serial.print("Direction: ");
      Serial.println(directionToText(currentDirection));
    }
  }
}

// ---------- ESP-NOW ----------
void forwardMapMessage(const MapMessage &msg);

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional debug:
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send OK" : "Send FAIL");
}

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(MapMessage)) return;

  MapMessage msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type != MSG_TYPE_MAP_UPDATE) return;
  if (msg.row < 1 || msg.row > BOARD_SIZE ||
      msg.col < 1 || msg.col > BOARD_SIZE) return;

  mapGrid[msg.row][msg.col] = msg.value;

  Serial.print("[Robot2] Map update from Robot ");
  Serial.print(msg.sourceId);
  Serial.print(" -> cell(");
  Serial.print(msg.row);
  Serial.print(",");
  Serial.print(msg.col);
  Serial.print(") = ");
  Serial.println(msg.value == CELL_OBSTACLE ? "OBSTACLE" : "CLEAR");

  if (msg.hop == 0 && msg.sourceId != ROBOT_ID) {
    forwardMapMessage(msg);
  }
}

void sendMapUpdate(uint8_t row, uint8_t col, uint8_t value) {
  MapMessage msg;
  msg.type     = MSG_TYPE_MAP_UPDATE;
  msg.sourceId = ROBOT_ID;
  msg.row      = row;
  msg.col      = col;
  msg.value    = value;
  msg.hop      = 0;

  esp_err_t res = esp_now_send(OTHER_ROBOT_MAC, (uint8_t*)&msg, sizeof(msg));
  // Optional debug: check res
}

void forwardMapMessage(const MapMessage &msgIn) {
  MapMessage out = msgIn;
  out.hop = 1;

  esp_now_send(OTHER_ROBOT_MAC, (uint8_t*)&out, sizeof(out));
  esp_now_send(BASE_MAC,        (uint8_t*)&out, sizeof(out));
}

// ---------- Ultrasonic ----------
float readUltrasonicCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30 ms timeout
  if (duration <= 0) return -1;
  return duration * 0.0343f / 2.0f;
}

// ---------- Position ----------
void updatePositionFromDirection() {
  int newRow = currentRow;
  int newCol = currentCol;

  if (currentDirection == DIR_FORWARD)      newRow--;
  else if (currentDirection == DIR_BACKWARD) newRow++;
  else if (currentDirection == DIR_RIGHT)   newCol++;
  else if (currentDirection == DIR_LEFT)    newCol--;

  if (newRow < 1 || newRow > BOARD_SIZE || newCol < 1 || newCol > BOARD_SIZE) {
    Serial.println("[Robot2] Move would leave board, ignored");
    return;
  }

  currentRow = newRow;
  currentCol = newCol;

  if (mapGrid[currentRow][currentCol] != CELL_CLEAR) {
    mapGrid[currentRow][currentCol] = CELL_CLEAR;
    Serial.print("[Robot2] Moved to cell (");
    Serial.print(currentRow); Serial.print(",");
    Serial.print(currentCol); Serial.println(") CLEAR");
    sendMapUpdate(currentRow, currentCol, CELL_CLEAR);
  }
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(IR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Init map
  for (int r = 1; r <= BOARD_SIZE; r++)
    for (int c = 1; c <= BOARD_SIZE; c++)
      mapGrid[r][c] = CELL_UNKNOWN;

  mapGrid[currentRow][currentCol] = CELL_CLEAR;

  // IMU init
  Wire.begin(12, 13);   // SDA = 12, SCL = 13
  delay(1000);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 NOT CONNECTED!");
    while (1) { delay(1000); }
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
    while (1) { delay(1000); }
  }

  Serial.println(directionToText(currentDirection));  // "FORWARD"

  // ESP-NOW init
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) { delay(1000); }
  }

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  peer.channel = 0;
  peer.encrypt = false;

  memcpy(peer.peer_addr, OTHER_ROBOT_MAC, 6);
  esp_now_add_peer(&peer);

  memcpy(peer.peer_addr, BASE_MAC, 6);
  esp_now_add_peer(&peer);

  Serial.println("[Robot2] Ready");
}

void loop() {
  updateIMU();

  // IR-based movement detection
  bool isWhite = digitalRead(IR_PIN);
  if (!lastIRKnown) {
    lastIsWhite = isWhite;
    lastIRKnown = true;
  } else if (isWhite != lastIsWhite) {
    lastIsWhite = isWhite;
    updatePositionFromDirection();
  }

  // Obstacle detection
  unsigned long now = millis();
  if (now - lastSonarMs >= SONAR_INTERVAL_MS) {
    lastSonarMs = now;
    float dist = readUltrasonicCm();
    if (dist > 0 && dist < OBSTACLE_DISTANCE_CM) {
      int r = currentRow;
      int c = currentCol;

      if (currentDirection == DIR_FORWARD)      r--;
      else if (currentDirection == DIR_BACKWARD) r++;
      else if (currentDirection == DIR_RIGHT)   c++;
      else if (currentDirection == DIR_LEFT)    c--;

      if (r >= 1 && r <= BOARD_SIZE && c >= 1 && c <= BOARD_SIZE) {
        if (mapGrid[r][c] != CELL_OBSTACLE) {
          mapGrid[r][c] = CELL_OBSTACLE;
          Serial.print("[Robot2] Obstacle at (");
          Serial.print(r); Serial.print(",");
          Serial.print(c); Serial.println(")");
          sendMapUpdate(r, c, CELL_OBSTACLE);
        }
      }
    }
  }

  delay(20);
}
