# 🤖 Wireless Sensor Network for Robotic Fleet Coordination

<div align="center">

![ESP32](https://img.shields.io/badge/ESP32-Dual_Core-blue?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-IDE_2.x-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![ESP-NOW](https://img.shields.io/badge/Protocol-ESP--NOW-green?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Complete-success?style=for-the-badge)

**A distributed wireless sensor network enabling real-time coordination and mapping between autonomous robotic agents using ESP-NOW mesh communication.**

[Features](#-features) • [Architecture](#-architecture) • [Hardware](#-hardware-requirements) • [Setup](#-quick-start) • [Documentation](#-documentation)

</div>

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Architecture](#-architecture)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Repository Structure](#-repository-structure)
- [Quick Start](#-quick-start)
- [Communication Protocol](#-communication-protocol)
- [Web Interface](#-web-interface)
- [Pin Configuration](#-pin-configuration)
- [Project Timeline](#-project-timeline)
- [Team](#-team)
- [License](#-license)

---

## 🎯 Overview

This project implements a **wireless sensor network** for coordinating a fleet of mobile robots exploring a 4×4 grid environment. Each robot is equipped with sensors (ultrasonic, IR, IMU) and communicates discoveries to peer robots and a central base station using the **ESP-NOW protocol**.

### Key Capabilities

- 🗺️ **Collaborative Mapping** — Robots share obstacle discoveries in real-time
- 📡 **Mesh Communication** — Peer-to-peer ESP-NOW with automatic re-broadcasting
- 🌐 **Web Visualization** — Live grid map viewable from any browser
- ⚡ **Low Latency** — Sub-10ms message delivery between nodes

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| **Multi-Robot Coordination** | Two mobile robots explore and share map data simultaneously |
| **Obstacle Detection** | HC-SR04 ultrasonic sensors detect obstacles within 15cm |
| **Position Tracking** | IR sensors detect white/black square transitions on checkerboard grid |
| **Orientation Sensing** | MPU6050 IMU with DMP provides heading (Forward/Right/Backward/Left) |
| **Mesh Re-broadcasting** | Messages forwarded with hop count to prevent broadcast storms |
| **Real-time Web UI** | Base station serves live map visualization via WiFi AP |
| **SPIFFS Web Files** | HTML/CSS/JS served from ESP32 flash storage |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SYSTEM ARCHITECTURE                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│    ┌──────────────┐    ESP-NOW    ┌──────────────┐                 │
│    │   ROBOT 1    │◄─────────────►│   ROBOT 2    │                 │
│    │  Start(4,1)  │               │  Start(4,4)  │                 │
│    │              │               │              │                 │
│    │ • Ultrasonic │               │ • Ultrasonic │                 │
│    │ • IR Sensor  │               │ • IR Sensor  │                 │
│    │ • MPU6050    │               │ • MPU6050    │                 │
│    └──────┬───────┘               └──────┬───────┘                 │
│           │                              │                          │
│           │         ESP-NOW              │                          │
│           └───────────┬──────────────────┘                          │
│                       ▼                                             │
│              ┌────────────────┐                                     │
│              │  BASE STATION  │                                     │
│              │                │                                     │
│              │ • ESP-NOW Recv │         ┌──────────────┐           │
│              │ • WiFi AP      │────────►│   PC/Phone   │           │
│              │ • Web Server   │  HTTP   │  Web Browser │           │
│              │ • SPIFFS       │         └──────────────┘           │
│              └────────────────┘                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Communication Flow

1. **Robot 1** detects obstacle → Sends `MapMessage` to **Robot 2**
2. **Robot 2** updates internal map → Re-broadcasts to **Robot 1** + **Base Station**
3. **Base Station** updates grid → Serves to web clients
4. Process is bidirectional: Robot 2 discoveries flow similarly

---

## 🔧 Hardware Requirements

### Components List

| Qty | Component | Purpose |
|:---:|-----------|---------|
| 3 | ESP32 DevKit | Main microcontrollers |
| 2 | RC Toy Vehicles | Mobile robot platforms |
| 2 | HC-SR04 Ultrasonic | Obstacle detection (15cm range) |
| 2 | IR Sensor Module | Black/white square detection |
| 2 | MPU6050 IMU | Orientation tracking (yaw) |
| 1 | 4×4 Checkerboard Grid | Navigation area |

### Wiring Diagram (Per Robot)

```
ESP32 GPIO Layout:
┌────────────────────────┐
│        ESP32           │
│                        │
│  GPIO 26 ──► TRIG      │ ─┐
│  GPIO 27 ◄── ECHO      │  │ Ultrasonic
│                        │ ─┘
│  GPIO 14 ◄── IR_OUT    │ ── IR Sensor
│                        │
│  GPIO 12 ─── SDA       │ ─┐
│  GPIO 13 ─── SCL       │  │ MPU6050 I2C
│                        │ ─┘
└────────────────────────┘
```

---

## 💻 Software Requirements

| Software | Version | Purpose |
|----------|---------|---------|
| Arduino IDE | 2.x+ | Development environment |
| ESP32 Board Support | Latest | ESP32 toolchain |
| MPU6050_6Axis_MotionApps20 | - | DMP-based orientation |
| WebServer (ESP32) | Built-in | HTTP server |
| SPIFFS | Built-in | Flash file system |
| ESP-NOW | Built-in | Peer-to-peer protocol |

---

## 📁 Repository Structure

```
Wireless-Sensor-Network-for-Robotic-Fleet-Coordination/
│
├── 📄 README.md                 # This file
├── 📄 LICENSE                   # MIT License
│
├── 📁 RobotNode/
│   └── RobotNode.ino            # Robot 1 firmware (starts at 4,1)
│
├── 📁 RobotNode2/
│   └── RobotNode2.ino           # Robot 2 firmware (starts at 4,4)
│
├── 📁 BaseStation2/
│   ├── BaseStation2.ino         # Base station firmware
│   └── data/
│       └── index.html           # Web UI for grid visualization
│
└── 📁 Docs/
    └── report.tex               # LaTeX project report
```

---

## 🚀 Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/your-username/Wireless-Sensor-Network-for-Robotic-Fleet-Coordination.git
cd Wireless-Sensor-Network-for-Robotic-Fleet-Coordination
```

### 2. Configure MAC Addresses

Each ESP32 has a unique MAC address. Update the following in all `.ino` files:

```cpp
// Find your MAC with: WiFi.macAddress()
uint8_t ROBOT1_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x88, 0x58};
uint8_t ROBOT2_MAC[] = {0xCC, 0xDB, 0xA7, 0x97, 0x76, 0x9C};
uint8_t BASE_MAC[]   = {0xCC, 0xDB, 0xA7, 0x97, 0x7B, 0x6C};
```

### 3. Upload SPIFFS Data

For the base station web interface:

1. Install **ESP32 Sketch Data Upload** tool
2. Place `index.html` in `BaseStation2/data/`
3. Run **Tools → ESP32 Sketch Data Upload**

### 4. Flash Firmware

| Board | Sketch to Upload |
|-------|------------------|
| Robot 1 ESP32 | `RobotNode/RobotNode.ino` |
| Robot 2 ESP32 | `RobotNode2/RobotNode2.ino` |
| Base Station | `BaseStation2/BaseStation2.ino` |

### 5. Connect to Web Interface

1. Power on the base station
2. Connect your PC/phone to WiFi: **`RobotBase`** (password: `12345678`)
3. Open browser: **`http://192.168.4.1`**
4. Watch the grid update in real-time!

---

## 📡 Communication Protocol

### Message Structure

```cpp
typedef struct __attribute__((packed)) {
  uint8_t type;      // MSG_TYPE_MAP_UPDATE = 1
  uint8_t sourceId;  // Robot ID (1 or 2)
  uint8_t row;       // Grid row (1-4)
  uint8_t col;       // Grid column (1-4)
  uint8_t value;     // CELL_UNKNOWN/CLEAR/OBSTACLE
  uint8_t hop;       // 0=original, 1=forwarded
} MapMessage;        // Total: 6 bytes
```

### Cell Types

| Value | Constant | Meaning |
|:-----:|----------|---------|
| 0 | `CELL_UNKNOWN` | Unexplored cell |
| 1 | `CELL_CLEAR` | Robot has visited |
| 2 | `CELL_OBSTACLE` | Obstacle detected |

### Re-broadcast Logic

```cpp
// Only forward original messages from other robots
if (msg.hop == 0 && msg.sourceId != ROBOT_ID) {
  msg.hop = 1;  // Prevent infinite forwarding
  esp_now_send(OTHER_ROBOT_MAC, &msg, sizeof(msg));
  esp_now_send(BASE_MAC, &msg, sizeof(msg));
}
```

---

## 🌐 Web Interface

The base station hosts a real-time visualization at `http://192.168.4.1`:

- **4×4 Grid Display** — Shows all cells with color coding
- **Auto-refresh** — Updates every 2 seconds via `/map` API
- **Cell States**:
  - ❓ Gray = Unknown
  - ✅ Green = Clear
  - ❌ Red = Obstacle

### API Endpoint

```
GET /map
Response: {"grid":[[0,1,2,0],[1,1,0,2],[0,0,1,1],[1,1,1,1]]}
```

---

## 🔌 Pin Configuration

| Component | GPIO | Direction | Notes |
|-----------|:----:|:---------:|-------|
| IR Sensor | 14 | INPUT | HIGH=White, LOW=Black |
| Ultrasonic Trigger | 26 | OUTPUT | 10μs pulse |
| Ultrasonic Echo | 27 | INPUT | Duration measurement |
| MPU6050 SDA | 12 | I2C | Data line |
| MPU6050 SCL | 13 | I2C | Clock line |

---

## 📅 Project Timeline

| Week | Phase | Deliverables |
|:----:|-------|--------------|
| **1** | Research & Setup | ESP-NOW learning, hardware assembly, architecture design |
| **2** | Core Modules | Sensor drivers, message structure, ESP-NOW init |
| **3** | Integration | Dual-robot communication, re-broadcast logic, map sync |
| **4** | Base Station | Web server, SPIFFS, real-time visualization |
| **5** | Testing & Docs | System demo, performance metrics, final report |

---

## 📊 Performance Metrics

| Metric | Value |
|--------|-------|
| ESP-NOW Range | >50m (line of sight) |
| Message Latency | <10ms |
| Obstacle Detection | 15cm range |
| IMU Update Rate | 100Hz (DMP) |
| Web Refresh Rate | 2 seconds |
| Grid Size | 4×4 (16 cells) |

---

## 👥 Team

| Name | Student ID |
|------|:----------:|
| Hassan Yousef | 13006567 |
| Pierre George Boshra | 13007351 |
| Mohamed Walid | 13006513 |
| Abdelhamid ElSharnouby | 13006294 |
| Khaled Khaled | 14001048 |
| Mahmoud Nasser | 13006342 |

**Supervised by:** Dr. Yasmine Zaghloul  
**Teaching Assistant:** Eng. Omar Hemeda

---

## 📚 References

1. [ESP-NOW Documentation](https://www.espressif.com/) — Espressif Systems
2. [ESP-NOW Tutorial](https://www.youtube.com/watch?v=Ydi0M3Xd_vs) — YouTube
3. [MPU6050 Library](https://github.com/jrowberg/i2cdevlib) — I2Cdevlib
4. [Arduino ESP32](https://docs.arduino.cc/) — Arduino Documentation

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

<div align="center">

**German International University**  
Faculty of Engineering • Automation and Control  
Winter Semester 2025

</div>
