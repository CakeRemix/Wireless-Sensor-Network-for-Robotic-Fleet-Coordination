/*#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define TRIG_PIN 5
#define ECHO_PIN 18

// ⚠️ Replace with your actual MAC addresses
#define BASE_MAC {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C}   // Base ESP32
#define PEER_MAC {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58}   // Robot 1 red

const char* ROBOT_ID = "R1";   // This is Robot 1
int posX = 0;  // Set starting position in the grid
int posY = 0;

typedef struct message_t {
  char robot[3];
  int x;
  int y;
  bool obstacle;
} message_t;

message_t data;

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  return distance;
}

void sendDataTo(uint8_t* peer_addr) {
  float distance = readDistanceCM();
  data.x = posX;
  data.y = posY;
  data.obstacle = (distance < 20);  // <20cm = obstacle
  strncpy(data.robot, ROBOT_ID, 3);

  esp_err_t result = esp_now_send(peer_addr, (uint8_t*)&data, sizeof(data));
  if (result == ESP_OK) {
    Serial.println("Data sent!");
  } else {
    Serial.println("Send error");
  }

  Serial.printf("Sent: %s at (%d,%d) | Obstacle: %s\n", data.robot, data.x, data.y, data.obstacle ? "Yes" : "No");
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  // Add BASE as peer
  esp_now_peer_info_t base_peer;
  memcpy(base_peer.peer_addr, BASE_MAC, 6);
  base_peer.channel = 0;
  base_peer.encrypt = false;
  esp_now_add_peer(&base_peer);

  // Add Robot 2 as peer
  esp_now_peer_info_t robot2_peer;
  memcpy(robot2_peer.peer_addr, PEER_MAC, 6);
  robot2_peer.channel = 0;
  robot2_peer.encrypt = false;
  esp_now_add_peer(&robot2_peer);

  Serial.println("Robot 1 ready.");
}

void loop() {
  // Send to base
  uint8_t base_mac[] = BASE_MAC;
  sendDataTo(base_mac);

  // Send to Robot 2
  uint8_t peer_mac[] = PEER_MAC;
  sendDataTo(peer_mac);

  delay(3000); // every 3 seconds
}*/
#include <esp_now.h>
#include <WiFi.h>

bool receivedValue = false;

// NEW ESP32 Core 3.x receive callback
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len == sizeof(bool)) {
    memcpy(&receivedValue, data, sizeof(bool));

    Serial.print("Received: ");
    Serial.println(receivedValue ? "BLACK detected (true)" :
                                   "WHITE detected (false)");
  } else {
    Serial.println("Received unknown data");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  Serial.println("Robot 2 Ready.");
}

void loop() {
  // nothing needed here
}





