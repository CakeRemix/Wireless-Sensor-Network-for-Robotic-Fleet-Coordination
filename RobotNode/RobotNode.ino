/****************************************************
   ROBOT 1 (ID = 1)

   - Position is in CELL coordinates (1..4, 1..4)
     (col, row)  => (xCell, yCell)
   - Robot 1 starts at (4,1)
   - IR is ACTIVE LOW:
        0 = WHITE  block
        1 = BLACK  block
   - IMU gives direction:
        0 = UP, 1 = RIGHT, 2 = DOWN, 3 = LEFT
   - grid[4][4] stores black/white pattern:
        0 = WHITE, 1 = BLACK
   - On IR change -> robot moved to next cell
   - Ultrasonic detects obstacle in front
   - Sends to Base (TCP):
        POS,x,y,1,dirCode
        OBS,ox,oy,1
****************************************************/

#include <WiFi.h>
#include <Wire.h>
#include <MPU6050_6Axis_MotionApps20.h>

#define RAD_TO_DEG 57.2957795f

// ====== WiFi Settings ======
const char* ssid     = "RobotNet";       // Base ESP AP name
const char* password = "12345678";       // Base ESP AP password
const char* host     = "192.168.4.1";    // Base ESP IP
const int   port     = 3333;             // TCP port

WiFiClient client;

#define ROBOT_ID 1

// ====== GRID (4x4) 0=WHITE, 1=BLACK ======
// Coordinates are (x=1..4, y=1..4)
// We store as grid[y-1][x-1]
int grid[4][4] = {
  // x: 1  2  3  4
  {0, 1, 0, 1},   // y=1 : W B W B
  {1, 0, 1, 0},   // y=2 : B W B W
  {0, 1, 0, 1},   // y=3 : W B W B
  {1, 0, 1, 0}    // y=4 : B W B W
};

// ====== Robot cell position (1..4, 1..4) ======
int xCell = 1;    // starting column  1
int yCell = 4;    // starting row     4

// ====== Pins (change to your wiring) ======
const int IR_PIN   = 14;   // digital input from IR (active low)
const int TRIG_PIN = 26;    // ultrasonic trigger
const int ECHO_PIN = 27;   // ultrasonic echo

// ====== IMU Object ======
MPU6050 mpu;
bool dmpReady = false;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];

String currentDir = "FORWARD";  // FORWARD / RIGHT / BACKWARD / LEFT
int currentDirCode = 0;         // 0=UP,1=RIGHT,2=DOWN,3=LEFT
int lastSentDirCode = -1; 

// IR state
int lastIRState = 1;            // will set correctly in setup()

// ===== Map yaw → text direction =====
String getDirection(float yaw) {
  if (yaw < 45 || yaw >= 315) return "FORWARD";
  if (yaw >= 45 && yaw < 135) return "RIGHT";
  if (yaw >= 135 && yaw < 225) return "BACKWARD";
  return "LEFT";
}

// ===== Map text direction → numeric code =====
int directionToCode(String d) {
  if (d == "FORWARD")  return 0; // UP
  if (d == "RIGHT")    return 1; // RIGHT
  if (d == "BACKWARD") return 2; // DOWN
  if (d == "LEFT")     return 3; // LEFT
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

// ===== Send Position to Base (in cell coords 1..4) =====
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
  currentDir    = newDir;
  currentDirCode = directionToCode(newDir);

  Serial.print("Yaw: "); Serial.print(yaw);
  Serial.print("  Dir: "); Serial.print(currentDir);
  Serial.print("  Code: "); Serial.println(currentDirCode);
}

// ===== Measure distance with ultrasonic (cm) =====
float getDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) return 9999; // no echo

  float distance = duration * 0.0343f / 2.0f;
  return distance;
}

// ===== Check for obstacle in front and send OBS,ox,oy =====
void checkObstacle() {
  float d = getDistanceCm();
  Serial.print("Distance: ");
  Serial.println(d);

  if (d < 15.0f) { // obstacle closer than 15 cm
    int ox = xCell;
    int oy = yCell;

    if (currentDirCode == 0) oy -= 1; // UP
    else if (currentDirCode == 1) ox += 1; // RIGHT
    else if (currentDirCode == 2) oy += 1; // DOWN
    else if (currentDirCode == 3) ox -= 1; // LEFT

    // inside grid 1..4
    if (ox >= 1 && ox <= 4 && oy >= 1 && oy <= 4) {
      sendObstacle(ox, oy);
    } else {
      Serial.println("Obstacle cell outside grid – ignored");
    }
  }
}

// ===== Update grid position when IR changes (moved one block) =====
void updatePositionFromIR() {
  int irNow = digitalRead(IR_PIN);  // 0 = white, 1 = black (active low)

  if (irNow != lastIRState) {
    // IR changed → robot crossed into a new block
    Serial.print("IR changed: ");
    Serial.print(lastIRState);
    Serial.print(" -> ");
    Serial.println(irNow);

    int newX = xCell;
    int newY = yCell;

    if (currentDirCode == 0) newY -= 1;   // UP
    else if (currentDirCode == 1) newX += 1; // RIGHT
    else if (currentDirCode == 2) newY += 1; // DOWN
    else if (currentDirCode == 3) newX -= 1; // LEFT

    // Check inside grid
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

      sendPosition();   // inform Base about new position
    } else {
      Serial.println("Move ignored: would leave the 4x4 grid");
    }
  }

  lastIRState = irNow;
}

void setup() {
  Serial.begin(115200);

    // Start WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  delay(2000);

  Wire.begin(32, 33);

  pinMode(IR_PIN,   INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Read initial IR state at starting cell (4,1)
  lastIRState = digitalRead(IR_PIN);
  Serial.print("Initial IR state: ");
  Serial.println(lastIRState);

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
    while (1);  // stop here
  }

  // Send initial position
  sendPosition();
  lastSentDirCode = currentDirCode;
}

void loop() {
  if (!dmpReady) return;

 checkWiFiStatus();
 updateIMUDirection();

// ✅ if direction changed, send POS even if robot didn't move
if (currentDirCode != lastSentDirCode) {
  sendPosition();
  lastSentDirCode = currentDirCode;
}

updatePositionFromIR();
checkObstacle();

delay(200);

}
