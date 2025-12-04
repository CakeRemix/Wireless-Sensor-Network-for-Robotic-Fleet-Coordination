#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define TRIG_PIN 5
#define ECHO_PIN 18

#define BASE_MAC {0x24, 0x6F, 0x28, 0x12, 0x34, 0x56}   // Replace with your Base ESP32's MAC
#define PEER_MAC {0x24, 0x6F, 0x28, 0x65, 0x43, 0x21}   // Replace with the other robot's MAC

const char* ROBOT_ID = "R1";   // Use "R2" for second robot
int posX = 0;  // Set this manually
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

  esp_now_peer_info_t peer;
  memcpy(peer.peer_addr, BASE_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  memcpy(peer.peer_addr, PEER_MAC, 6);
  esp_now_add_peer(&peer);

  Serial.println("Robot ready.");
}

void loop() {
  // Send to base
  uint8_t base_mac[] = BASE_MAC;
  sendDataTo(base_mac);

  // Send to peer robot
  uint8_t peer_mac[] = PEER_MAC;
  sendDataTo(peer_mac);

  delay(3000); // every 3 seconds
}
