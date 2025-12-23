/****************************************************
   ROBOT 1 (ID = 1)

   - Position is in CELL coordinates (1..4, 1..4)
     (col, row)  => (xCell, yCell)
   - Robot 1 starts at (1,4)
   - IR FLOOR is ACTIVE LOW:
        0 = WHITE  block
        1 = BLACK  block
   - IR FRONT is ACTIVE LOW (obstacle sensor):
        0 = OBSTACLE detected
        1 = NO obstacle
   - IMU gives direction:
        0 = UP, 1 = RIGHT, 2 = DOWN, 3 = LEFT
   - grid[4][4] stores black/white pattern:
        0 = WHITE, 1 = BLACK
   - On FLOOR IR change -> robot moved to next cell
   - FRONT IR detects obstacle in front
   - Sends to Base (TCP):
        POS,x,y,1,dirCode
        OBS,ox,oy,1
****************************************************/

#include <WiFi.h>
#include <Wire.h>
#include <MPU6050_6Axis_MotionApps20.h>

#define RAD_TO_DEG 57.2957795f

// ====== WiFi Settings ======
const char* ssid     = "RobotNet";
const char* password = "12345678";
const char* host     = "192.168.4.1";
const int   port     = 3333;

WiFiClient client;

#define ROBOT_ID 2

// ====== GRID (4x4) 0=WHITE, 1=BLACK ======
int grid[4][4] = {
  {0, 1, 0, 1},
  {1, 0, 1, 0},
  {0, 1, 0, 1},
  {1, 0, 1, 0}
};

// ====== Robot cell position (1..4, 1..4) ======
int xCell = 4;    // starting column  1
int yCell = 4;    // starting row     4

// ====== IR Pins ======
const int IR_FLOOR_PIN = 14;   // floor IR (movement)  ACTIVE LOW
const int IR_FRONT_PIN = 25;   // front IR (obstacle)  ACTIVE LOW

// ====== IMU Object ======
MPU6050 mpu;
bool dmpReady = false;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];

String currentDir = "FORWARD";
int currentDirCode = 0;         // 0=UP,1=RIGHT,2=DOWN,3=LEFT
int lastSentDirCode = -1;

// IR state (floor)
int lastFloorIRState = 1;

// obstacle state (front)
int lastFrontIRState = 1;

// ===== Map yaw → text direction =====
String getDirection(float yaw) {
  if (yaw < 45 || yaw >= 315) return "FORWARD";
  if (yaw >= 45 && yaw < 135) return "RIGHT";
  if (yaw >= 135 && yaw < 225) return "BACKWARD";
  return "LEFT";
}

// ===== Map text direction → numeric code =====
int directionToCode(String d) {
  if (d == "FORWARD")  return 0;
  if (d == "RIGHT")    return 1;
  if (d == "BACKWARD") return 2;
  if (d == "LEFT")     return 3;
  return 0;
}

// ===== WiFi connection checker (debug) =====
void checkWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Status: CONNECTED");
  } else {
    Serial.println("WiFi Status: NOT CONNECTED — Reconnecting...");
    WiFi.begin(ssid, password);
  }
}

// ===== Ensure WiFi + TCP connection =====
void ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected — reconnecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      Serial.print(".");
    }
    Serial.println("\nWiFi connected");
  }

  if (!client.connected()) {
    Serial.println("Reconnecting to Base...");
    while (!client.connect(host, port)) {
      Serial.println("Waiting for Base...");
      delay(1000);
    }
    Serial.println("TCP Connected to Base");
  }
}

// ===== Send Position to Base =====
void sendPosition() {
  ensureConnected();
  String msg = "POS," + String(xCell) + "," + String(yCell) + "," +
               String(ROBOT_ID) + "," + String(currentDirCode) + "\n";
  client.print(msg);
  Serial.println("Sent: " + msg);
}

// ===== Send Obstacle to Base =====
void sendObstacle(int ox, int oy) {
  ensureConnected();
  String msg = "OBS," + String(ox) + "," + String(oy) + "," +
               String(ROBOT_ID) + "\n";
  client.print(msg);
  Serial.println("Sent: " + msg);
}

// ===== Read IMU + Update currentDirCode =====
void updateIMUDirection() {
  if (!mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) return;

  mpu.dmpGetQuaternion(&q, fifoBuffer);
  mpu.dmpGetGravity(&gravity, &q);
  mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

  float yaw = ypr[0] * RAD_TO_DEG;
  if (yaw < 0) yaw += 360;

  String newDir = getDirection(yaw);
  currentDir     = newDir;
  currentDirCode = directionToCode(newDir);

  Serial.print("Yaw: "); Serial.print(yaw);
  Serial.print("  Dir: "); Serial.print(currentDir);
  Serial.print("  Code: "); Serial.println(currentDirCode);
}

// ===== Check FRONT IR for obstacle and send OBS =====
// Active low: 0 = obstacle detected
void checkObstacleFrontIR() {
  int frontNow = digitalRead(IR_FRONT_PIN);

  // optional: only act on change (reduces spam)
  if (frontNow != lastFrontIRState) {
    Serial.print("Front IR changed: ");
    Serial.print(lastFrontIRState);
    Serial.print(" -> ");
    Serial.println(frontNow);
  }

  // obstacle detected
  if (frontNow == LOW) {
    int ox = xCell;
    int oy = yCell;

    if (currentDirCode == 0) oy -= 1;       // UP
    else if (currentDirCode == 1) ox += 1;  // RIGHT
    else if (currentDirCode == 2) oy += 1;  // DOWN
    else if (currentDirCode == 3) ox -= 1;  // LEFT

    if (ox >= 1 && ox <= 4 && oy >= 1 && oy <= 4) {
      sendObstacle(ox, oy);
    } else {
      Serial.println("Obstacle cell outside grid – ignored");
    }
  }

  lastFrontIRState = frontNow;
}

// ===== Update grid position when FLOOR IR changes =====
// Active low means output toggles depending on black/white
void updatePositionFromFloorIR() {
  int floorNow = digitalRead(IR_FLOOR_PIN);

  if (floorNow != lastFloorIRState) {
    Serial.print("Floor IR changed: ");
    Serial.print(lastFloorIRState);
    Serial.print(" -> ");
    Serial.println(floorNow);

    int newX = xCell;
    int newY = yCell;

    if (currentDirCode == 0) newY -= 1;        // UP
    else if (currentDirCode == 1) newX += 1;   // RIGHT
    else if (currentDirCode == 2) newY += 1;   // DOWN
    else if (currentDirCode == 3) newX -= 1;   // LEFT

    if (newX >= 1 && newX <= 4 && newY >= 1 && newY <= 4) {
      xCell = newX;
      yCell = newY;

      int gridColor = grid[yCell - 1][xCell - 1]; // 0:white,1:black
      Serial.print("New cell: (");
      Serial.print(xCell);
      Serial.print(",");
      Serial.print(yCell);
      Serial.print(")  Color: ");
      Serial.println(gridColor == 0 ? "WHITE" : "BLACK");

      sendPosition();
    } else {
      Serial.println("Move ignored: would leave the 4x4 grid");
    }
  }

  lastFloorIRState = floorNow;
}

void setup() {
  Serial.begin(115200);

  // Start WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  delay(2000);

  Wire.begin(26, 27);

  pinMode(IR_FLOOR_PIN, INPUT);
  pinMode(IR_FRONT_PIN, INPUT);

  // Read initial IR states
  lastFloorIRState = digitalRead(IR_FLOOR_PIN);
  lastFrontIRState = digitalRead(IR_FRONT_PIN);

  Serial.print("Initial Floor IR state: ");
  Serial.println(lastFloorIRState);

  Serial.print("Initial Front IR state: ");
  Serial.println(lastFrontIRState);

  // ==== INIT IMU ====
  mpu.initialize();
  uint16_t status = mpu.dmpInitialize();

  if (status == 0) {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.setDMPEnabled(true);
    dmpReady = true;
    Serial.println("IMU READY");
  } else {
    Serial.println("IMU INIT FAILED!");
    while (1);
  }

  // Send initial position
  sendPosition();
  lastSentDirCode = currentDirCode;
}

void loop() {
  if (!dmpReady) return;

  checkWiFiStatus();
  updateIMUDirection();

  // send POS if direction changed even without movement
  if (currentDirCode != lastSentDirCode) {
    sendPosition();
    lastSentDirCode = currentDirCode;
  }

  updatePositionFromFloorIR();   // movement
  checkObstacleFrontIR();        // obstacle 

  delay(200);
}
