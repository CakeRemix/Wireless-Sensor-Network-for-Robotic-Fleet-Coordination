# Wireless Sensor Network for Robotic Fleet Coordination

[![ESP32](https://img.shields.io/badge/ESP32-Microcontroller-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![WiFi](https://img.shields.io/badge/WiFi-TCP-green.svg)](https://en.wikipedia.org/wiki/Transmission_Control_Protocol)
[![MPU6050](https://img.shields.io/badge/Sensor-MPU6050-orange.svg)](https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/)
[![IR Sensor](https://img.shields.io/badge/Sensor-IR-red.svg)](https://en.wikipedia.org/wiki/Infrared_sensor)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A distributed robotic fleet coordination system using ESP32 microcontrollers, WiFi TCP communication, and IR sensors for position tracking and obstacle detection. This project demonstrates real-time data collection, processing, and visualization of multiple robots navigating in a coordinated environment.

## 🎯 Overview

This project implements a wireless sensor network for coordinating multiple autonomous robots. Each robot is equipped with:
- **IR sensors** for obstacle detection and position tracking
- **MPU6050 IMU** for orientation sensing
- **WiFi TCP connectivity** for real-time communication with a base station

The system enables robots to:
- Navigate autonomously while avoiding obstacles
- Track their position on a black-and-white grid
- Share sensor data with a central base station
- Coordinate movements to prevent collisions

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Base Station (PC)                        │
│  ┌────────────────────────────────────────────────────┐     │
│  │        Python Processing Application                │     │
│  │  - TCP Server (192.168.4.1:3333)                   │     │
│  │  - Data aggregation & processing                    │     │
│  │  - Real-time visualization                          │     │
│  │  - Collision detection                              │     │
│  └────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ WiFi TCP
                            │ (RobotNet)
          ┌─────────────────┼─────────────────┐
          │                 │                 │
    ┌─────▼─────┐     ┌─────▼─────┐     ┌────▼──────┐
    │  Robot 1  │     │  Robot 2  │     │  Robot N  │
    │           │     │           │     │           │
    │  ESP32    │     │  ESP32    │     │  ESP32    │
    │  + IR     │     │  + IR     │     │  + IR     │
    │  + MPU6050│     │  + MPU6050│     │  + MPU6050│
    └───────────┘     └───────────┘     └───────────┘
```

### Communication Flow

1. **Robot Sensors** → Collect data (position, obstacles, orientation)
2. **WiFi TCP** → Transmit CSV formatted messages to base station
3. **Base Station** → Process and aggregate data from all robots
4. **Visualization** → Real-time display of fleet status and positions

## 🔧 Hardware Requirements

### Per Robot:
- **ESP32 Development Board** (e.g., ESP32-WROOM-32)
- **MPU6050 IMU Module** - 6-axis accelerometer/gyroscope for orientation
- **IR Sensor (Obstacle Detection)** - Active low, detects obstacles ahead
- **IR Sensor (Position Tracking)** - Active low, detects black/white surfaces
- **Motor Driver** (e.g., L298N or similar)
- **DC Motors** with wheels
- **Power Supply** (battery pack, 7.4V recommended)
- **Chassis** and mounting hardware

### Base Station:
- **PC or Laptop** with WiFi capability
- **Python 3.7+** installed
- Network configured to communicate with robot network

## 📡 Communication Protocol

### Network Configuration
- **SSID**: RobotNet
- **Base Station IP**: 192.168.4.1
- **TCP Port**: 3333
- **Protocol**: WiFi TCP with CSV message format

### Message Format

The robots communicate using CSV-formatted strings over TCP:

#### Position Update Message
```
POS,x,y,robotID,dirCode
```
- `x`: X-coordinate on grid
- `y`: Y-coordinate on grid
- `robotID`: Unique robot identifier (1-255)
- `dirCode`: Direction code (0=North, 1=East, 2=South, 3=West)

**Example**: `POS,5,3,1,0` - Robot 1 at position (5,3) facing North

#### Obstacle Detection Message
```
OBS,x,y,robotID
```
- `x`: X-coordinate where obstacle detected
- `y`: Y-coordinate where obstacle detected
- `robotID`: Robot identifier that detected the obstacle

**Example**: `OBS,6,3,1` - Robot 1 detected obstacle at (6,3)

### TCP Communication Details

- **Connection**: Persistent TCP connection to base station
- **Reconnection**: Automatic reconnection on connection loss
- **Message Frequency**: Position updates every 500ms, obstacle reports as detected
- **Encoding**: ASCII/UTF-8 text with newline terminators

## 🔌 Pin Configuration

### ESP32 GPIO Assignments

```cpp
// IR Sensors
#define IR_FLOOR_PIN 14      // Position tracking (black/white detection) - Active LOW
#define IR_OBSTACLE_PIN 25   // Obstacle detection (front sensor) - Active LOW

// MPU6050 IMU (I2C)
#define MPU6050_SDA 26       // I2C Data line
#define MPU6050_SCL 27       // I2C Clock line

// Motor Control (example - adjust based on your motor driver)
#define MOTOR_L_FWD 12       // Left motor forward
#define MOTOR_L_BWD 13       // Left motor backward
#define MOTOR_R_FWD 32       // Right motor forward
#define MOTOR_R_BWD 33       // Right motor backward
```

### Sensor Details

#### IR Floor Sensor (GPIO 14)
- **Purpose**: Position tracking on black/white grid
- **Logic**: Active LOW (LOW = black detected, HIGH = white detected)
- **Mounting**: Facing downward, ~1cm from floor

#### IR Obstacle Sensor (GPIO 25)
- **Purpose**: Front obstacle detection
- **Logic**: Active LOW (LOW = obstacle detected, HIGH = clear)
- **Mounting**: Facing forward, ~5-10cm from ground
- **Range**: Typically 2-30cm (model dependent)

#### MPU6050 IMU (I2C)
- **Purpose**: Orientation and motion sensing
- **Interface**: I2C (address 0x68 or 0x69)
- **Sensors**: 3-axis accelerometer + 3-axis gyroscope
- **Usage**: Calculate heading/direction, detect turns

## 💻 Software Setup

### Robot Firmware (ESP32)

1. **Install Arduino IDE** with ESP32 board support:
   ```bash
   # Add ESP32 Board Manager URL in Arduino IDE preferences:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. **Install Required Libraries**:
   - WiFi (built-in with ESP32)
   - Wire (built-in)
   - MPU6050 (by Electronic Cats or Jeff Rowberg)

3. **Configure Network Settings**:
   ```cpp
   const char* ssid = "RobotNet";
   const char* password = "your_password";
   const char* server_ip = "192.168.4.1";
   const int server_port = 3333;
   ```

4. **Upload Firmware** to each ESP32 with unique robot ID

### Base Station Software

1. **Clone Repository**:
   ```bash
   git clone https://github.com/CakeRemix/Wireless-Sensor-Network-for-Robotic-Fleet-Coordination.git
   cd Wireless-Sensor-Network-for-Robotic-Fleet-Coordination
   ```

2. **Install Python Dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

3. **Configure Network**:
   - Set PC WiFi to connect to RobotNet or create RobotNet access point
   - Ensure PC has IP 192.168.4.1 on the network

4. **Run Base Station**:
   ```bash
   python base_station.py
   ```

## 🚀 Usage

### Starting the System

1. **Power on all robots** - They will automatically connect to RobotNet and establish TCP connections

2. **Start base station software** on PC:
   ```bash
   python base_station.py
   ```

3. **Monitor the visualization** - Real-time display shows:
   - Robot positions and orientations
   - Detected obstacles
   - Movement paths
   - Connection status

### Robot Operation

- Robots autonomously navigate the environment
- IR floor sensor tracks position on grid
- IR obstacle sensor triggers avoidance maneuvers
- MPU6050 maintains orientation awareness
- Position and obstacle data sent to base station via WiFi TCP

## 📊 Data Visualization

The base station provides:
- **Real-time grid map** showing robot positions
- **Obstacle overlay** with detected obstacles
- **Status panel** with connection info and sensor readings
- **Path history** showing robot trajectories
- **Alert system** for potential collisions

## 🔬 Technical Details

### Position Tracking
- Black/white grid pattern on floor
- IR sensor detects transitions
- Dead reckoning between grid lines
- MPU6050 gyroscope for heading correction

### Obstacle Detection
- IR sensor continuously monitors front area
- Active LOW signal indicates obstacle presence
- Triggers avoidance algorithm
- Position reported to base station for fleet coordination

### Coordination Algorithm
- Base station maintains global map
- Collision prediction based on positions and velocities
- Path planning considers known obstacles
- Priority-based movement resolution

## 🛠️ Troubleshooting

### Connectivity Issues
- **Robots won't connect**: Check WiFi credentials and base station IP
- **Connection drops**: Verify WiFi signal strength, reduce distance
- **No TCP connection**: Ensure base station server is running on port 3333

### Sensor Issues
- **IR sensors not responding**: Check wiring, verify active LOW logic
- **Position tracking errors**: Adjust sensor height, ensure good contrast on grid
- **MPU6050 not found**: Check I2C connections, verify address (0x68/0x69)

### Performance Issues
- **Laggy updates**: Reduce message frequency, check network bandwidth
- **Poor obstacle detection**: Adjust IR sensor sensitivity or positioning

## 📈 Future Enhancements

- [ ] Multi-hop mesh networking for extended range
- [ ] Machine learning for improved path planning
- [ ] Support for dynamic obstacle avoidance
- [ ] Mobile app for remote monitoring
- [ ] Additional sensor integration (ultrasonic, LiDAR)
- [ ] Swarm intelligence algorithms
- [ ] Battery monitoring and charging station navigation

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- ESP32 community for excellent documentation and libraries
- MPU6050 library contributors
- Open-source robotics community

## 📧 Contact

Project Link: [https://github.com/CakeRemix/Wireless-Sensor-Network-for-Robotic-Fleet-Coordination](https://github.com/CakeRemix/Wireless-Sensor-Network-for-Robotic-Fleet-Coordination)

---

**Note**: This project is for educational purposes and demonstrates concepts in distributed systems, sensor networks, and robotic coordination.
