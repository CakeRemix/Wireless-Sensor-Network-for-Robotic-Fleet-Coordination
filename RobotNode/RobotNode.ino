#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Ultrasonic Sensor Pins
#define TRIG_PIN 5
#define ECHO_PIN 18

// MAC Addresses
#define BASE_MAC {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C}   // Base Station MAC
#define PEER_MAC {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C}   // Robot 2 MAC

// Robot Configuration
const char* ROBOT_ID = "R1";   // Change to "R2" for second robot

// Robot Position and Orientation
int posX = 0;  // Current X position
int posY = 0;  // Current Y position
char orientation = 'F';  // Current orientation: 'L'=Left, 'R'=Right, 'F'=Forward, 'B'=Backward

// Distance threshold for obstacle detection (cm)
const float OBSTACLE_THRESHOLD = 20.0;

// Message Structure
typedef struct message_t {
  char robot[3];
  int x;
  int y;
  bool obstacle;
} message_t;

message_t data;

// Read distance from ultrasonic sensor
float readUltraSonicSensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) {
    return 999.0; // No echo received
  }
  
  float distance = duration * 0.034 / 2;
  return distance;
}

// Check if obstacle is detected
bool isObstacleDetected(float distance) {
  return (distance < OBSTACLE_THRESHOLD && distance > 0);
}

// Get obstacle position based on robot orientation
void getObstaclePosition(int &obstacleX, int &obstacleY) {
  obstacleX = posX;
  obstacleY = posY;
  
  // Calculate obstacle position based on orientation
  switch(orientation) {
    case 'L':  // Left
      obstacleX = posX - 1;
      obstacleY = posY;
      break;
    case 'R':  // Right
      obstacleX = posX + 1;
      obstacleY = posY;
      break;
    case 'F':  // Forward
      obstacleX = posX;
      obstacleY = posY - 1;
      break;
    case 'B':  // Backward
      obstacleX = posX;
      obstacleY = posY + 1;
      break;
  }
}

// Update robot position based on movement
void updatePosition() {
  // This would be called when robot moves
  // For now, position is updated manually or via serial commands
  // W -> B, B -> W logic would be implemented here
}

// Send data to specified peer
void sendDataTo(uint8_t* peer_addr) {
  // Prepare message with current position
  data.x = posX;
  data.y = posY;
  data.obstacle = false;
  strncpy(data.robot, ROBOT_ID, 3);

  // Send current position (scanned and clear)
  esp_err_t result = esp_now_send(peer_addr, (uint8_t*)&data, sizeof(data));
  
  Serial.printf("Sent position: %s at (%d,%d) | Status: %s\n", 
                data.robot, data.x, data.y, 
                result == ESP_OK ? "OK" : "FAIL");
}

// Send obstacle data to specified peer
void sendObstacleDataTo(uint8_t* peer_addr, int obstacleX, int obstacleY) {
  // Prepare message with obstacle position
  data.x = obstacleX;
  data.y = obstacleY;
  data.obstacle = true;
  strncpy(data.robot, ROBOT_ID, 3);

  // Send obstacle position
  esp_err_t result = esp_now_send(peer_addr, (uint8_t*)&data, sizeof(data));
  
  Serial.printf("Sent obstacle: %s at (%d,%d) | Status: %s\n", 
                data.robot, data.x, data.y, 
                result == ESP_OK ? "OK" : "FAIL");
}

// Callback for sent data
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: handle send status
}

void setup() {
  Serial.begin(115200);
  
  // Setup ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  // Register send callback
  esp_now_register_send_cb(onSent);

  // Add base station peer
  esp_now_peer_info_t peer;
  uint8_t base_mac[] = BASE_MAC;
  memcpy(peer.peer_addr, base_mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  
  if (esp_now_add_peer(&peer) == ESP_OK) {
    Serial.println("Base Station Peer Added.");
  } else {
    Serial.println("Failed to add Base Station peer!");
  }

  // Add peer robot
  uint8_t peer_mac[] = PEER_MAC;
  memcpy(peer.peer_addr, peer_mac, 6);
  
  if (esp_now_add_peer(&peer) == ESP_OK) {
    Serial.println("Robot Peer Added.");
  } else {
    Serial.println("Failed to add Robot peer!");
  }

  Serial.println("Robot Node Ready.");
  Serial.printf("Robot ID: %s | Starting Position: (%d,%d) | Orientation: %c\n", 
                ROBOT_ID, posX, posY, orientation);
}

void loop() {
  // Step 1: Read Ultra Sonic Sensor
  float distance = readUltraSonicSensor();
  Serial.printf("Distance: %.2f cm\n", distance);

  uint8_t base_mac[] = BASE_MAC;
  uint8_t peer_mac[] = PEER_MAC;

  // Step 2: Check if obstacle is detected
  if (isObstacleDetected(distance)) {
    Serial.println("Obstacle Detected!");
    
    // Step 3: What is the orientation of the robot?
    // Calculate obstacle position based on orientation
    int obstacleX, obstacleY;
    getObstaclePosition(obstacleX, obstacleY);
    
    Serial.printf("Obstacle at (%d,%d) relative to orientation '%c'\n", 
                  obstacleX, obstacleY, orientation);
    
    // Step 4: Add the position of the obstacle and update the map
    sendObstacleDataTo(base_mac, obstacleX, obstacleY);
    delay(100);
    sendObstacleDataTo(peer_mac, obstacleX, obstacleY);
    
  } else {
    // No obstacle detected - W -> B or B -> W logic
    Serial.println("No Obstacle - Path Clear");
    
    // Step 5: Update position
    updatePosition();
    
    // Step 6: Add the current position as scanned and clear, and update the map
    sendDataTo(base_mac);
    delay(100);
    sendDataTo(peer_mac);
  }

  delay(2000); // Check every 2 seconds
}

// Serial commands to control robot (optional)
// Send commands via Serial Monitor:
// M:x,y - Move to position (x,y)
// O:L/R/F/B - Set orientation
void serialEvent() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("M:")) {
      // Move command: M:x,y
      int commaIdx = cmd.indexOf(',');
      if (commaIdx > 0) {
        posX = cmd.substring(2, commaIdx).toInt();
        posY = cmd.substring(commaIdx + 1).toInt();
        Serial.printf("Position updated to (%d,%d)\n", posX, posY);
      }
    } else if (cmd.startsWith("O:")) {
      // Orientation command: O:L/R/F/B
      orientation = cmd.charAt(2);
      Serial.printf("Orientation set to '%c'\n", orientation);
    }
  }
}
