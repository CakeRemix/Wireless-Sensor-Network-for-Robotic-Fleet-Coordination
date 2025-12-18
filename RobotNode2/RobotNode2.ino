/****************************************************
   ROBOT 2 (ID = 2)

   - Position is in CELL coordinates (1..4, 1..4)
   - Robot 2 starts at (4,4)
   - IR is ACTIVE LOW:
        0 = WHITE block
        1 = BLACK block
   - IMU gives direction:
        0 = UP, 1 = RIGHT, 2 = DOWN, 3 = LEFT
   - grid[4][4] stores black/white pattern:
        0 = WHITE, 1 = BLACK
   - On IR change -> robot moved one block
   - Ultrasonic detects obstacle in front
   - Sends to Base (TCP):
        POS,x,y,2,dirCode
        OBS,ox,oy,2
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

// ====== Robot Start Position ======
int xCell = 4;    // starting column
int yCell = 4;    // starting row

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

String currentDir = "FORWARD";  
int currentDirCode = 0;   
int lastSentDirCode = -1;       

int lastIRState = 1;            

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

// ===== WiFi connection checker =====
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

// ===== Read IMU + Update direction =====
void updateIMUDirection() {
  if (!mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) return;

  mpu.dmpGetQuaternion(&q, fifoBuffer);
  mpu.dmpGetGravity(&gravity, &q);
  mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

  float yaw = ypr[0] * RAD_TO_DEG;
  if (yaw < 0) yaw += 360;

  String newDir = getDirection(yaw);
  currentDir = newDir;
  currentDirCode = directionToCode(newDir);

  Serial.print("Yaw: "); Serial.print(yaw);
  Serial.print("  Dir: "); Serial.print(currentDir);
  Serial.print("  Code: "); Serial.println(currentDirCode);
}

// ===== Ultrasonic distance (cm) =====
float getDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 9999;

  return duration * 0.0343f / 2.0f;
}

// ===== Check for obstacle =====
void checkObstacle() {
  float d = getDistanceCm();
  Serial.print("Distance: "); Serial.println(d);

  if (d < 15.0f) {
    int ox = xCell;
    int oy = yCell;

    if (currentDirCode == 0) oy -= 1;
    else if (currentDirCode == 1) ox += 1;
    else if (currentDirCode == 2) oy += 1;
    else if (currentDirCode == 3) ox -= 1;

    if (ox >= 1 && ox <= 4 && oy >= 1 && oy <= 4)
      sendObstacle(ox, oy);
  }
}

// ===== IR movement detection =====
void updatePositionFromIR() {
  int irNow = digitalRead(IR_PIN);

  if (irNow != lastIRState) {
    Serial.print("IR changed: ");
    Serial.print(lastIRState);
    Serial.print(" -> ");
    Serial.println(irNow);

    int newX = xCell;
    int newY = yCell;

    if (currentDirCode == 0) newY -= 1;
    else if (currentDirCode == 1) newX += 1;
    else if (currentDirCode == 2) newY += 1;
    else if (currentDirCode == 3) newX -= 1;

    if (newX >= 1 && newX <= 4 && newY >= 1 && newY <= 4) {
      xCell = newX;
      yCell = newY;

      Serial.print("New cell: (");
      Serial.print(xCell);
      Serial.print(",");
      Serial.print(yCell);
      Serial.println(")");

      sendPosition();
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

  Wire.begin(12, 13);

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
