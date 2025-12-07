#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

// Thresholds
const int FORWARD_SPIKE = 2500;
const int BACKWARD_SPIKE = -2500;
const int MOTION_VARIANCE_THRESHOLD = 300;   // Movement vibrations
const int STOP_VARIANCE_THRESHOLD = 80;      // Stillness
const int GYRO_LEFT = 150;
const int GYRO_RIGHT = -150;

int16_t ax, ay, az;
int16_t gx, gy, gz;

// For motion detection
const int WINDOW = 20;
int axHistory[WINDOW];
int indexPos = 0;
bool bufferFilled = false;

String state = "STOP";

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  mpu.initialize();
  Serial.println("Calibrating...");
  mpu.CalibrateAccel(30);
  mpu.CalibrateGyro(30);
  Serial.println("Ready.");
}

int computeVariance() {
  long sum = 0;
  long sumSq = 0;
  int count = bufferFilled ? WINDOW : indexPos;

  for (int i = 0; i < count; i++) {
    sum += axHistory[i];
    sumSq += (long)axHistory[i] * axHistory[i];
  }

  long mean = sum / count;
  long var = (sumSq / count) - (mean * mean);
  return abs(var);
}

void loop() {
  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);

  // Store AX samples for variance
  axHistory[indexPos] = ax;
  indexPos++;
  if (indexPos >= WINDOW) {
    indexPos = 0;
    bufferFilled = true;
  }

  int variance = computeVariance();

  // Detect start of motion
  if (ax > FORWARD_SPIKE) {
    state = "FORWARD (start)";
  }
  if (ax < BACKWARD_SPIKE) {
    state = "BACKWARD (start)";
  }

  // Detect ongoing forward movement 
  if (variance > MOTION_VARIANCE_THRESHOLD && state.startsWith("FORWARD")) {
    state = "FORWARD (moving)";
  }

  // Detect continuous backward movement
  if (variance > MOTION_VARIANCE_THRESHOLD && state.startsWith("BACKWARD")) {
    state = "BACKWARD (moving)";
  }

  // Detect STOP
  if (variance < STOP_VARIANCE_THRESHOLD && abs(gz) < 50) {
    state = "STOP";
  }

  // Turning detection
  if (gz > GYRO_LEFT) {
    Serial.println("TURN LEFT");
  } 
  else if (gz < GYRO_RIGHT) {
    Serial.println("TURN RIGHT");
  }

  Serial.print("Motion state: ");
  Serial.print(state);
  Serial.print(" | AX: ");
  Serial.print(ax);
  Serial.print(" | Var: ");
  Serial.println(variance);

  delay(50);
}
