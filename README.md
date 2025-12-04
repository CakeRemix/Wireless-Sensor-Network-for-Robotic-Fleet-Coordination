# Wireless Sensor Network for Robotic Fleet Coordination

![Project status: WIP](https://img.shields.io/badge/status-WIP-orange) ![License: MIT](https://img.shields.io/badge/license-MIT-green)

## TL;DR
A decentralized, ESP32-based wireless sensor network enabling a small fleet of mobile robots to share obstacle and localization data (via ESP-NOW) for cooperative mapping and formation control. This repository contains the project report, documentation, and code placeholders for the course "Introduction to Computer Networks" at the German International University.

## Why this project
- Demonstrates low-latency, connectionless peer-to-peer communication using ESP-NOW.
- Explores distributed mapping and simple formation algorithms on low-cost hardware.
- Useful as a hands-on introduction to mesh-style coordination for mobile agents.

## Key Technologies
- ESP32 (ESP-NOW)
- Arduino IDE (ESP32 board support)
- HC-SR04 ultrasonic sensor
- TCS3200 / TCS34725 color sensor
- Simple 2-wheel remote-controlled toy platforms

## Project Team
- Hassan Yousef — 13006567
- Pierre George Boshra — 13007351
- Mohamed Walid — 13006513
- Abdelhamid ElSharnouby — 13006294
- Khaled Khaled — 14001048
- Mahmoud Nasser — 13006342

Supervision & Support:
- Dr. Yasmine Zaghloul (Instructor)
- Eng. Omar Hemeda (TA)
- Eng. Amr Khaled (Lab Engineer)

---

## Repository Structure
```
README.md
Docs/
	└── report.tex
	└── (other documentation and diagrams)
hardware/
	└── schematics/ (sensor wiring and diagrams)
software/
	└── master/ (master node sketches)
	└── slave/  (slave node sketches)
```

> Note: The `software/` folder currently contains placeholders and code templates. See `Docs/report.tex` for timeline and design decisions.

## Architecture Overview
The system adopts a lightweight mesh-like topology using ESP-NOW so each ESP32 can broadcast and receive short packets without a TCP/IP connection. The high-level roles are:

- Master Node (USB-connected ESP32): aggregates map updates, provides visualization and logging.
- Slave Nodes (vehicle-mounted ESP32s): sense environment (ultrasonic + color), broadcast obstacle/location messages, receive peer updates and update local map.

Message design principles:
- Small fixed-size messages (robot ID, position/color ID, obstacle flag, distance, timestamp, TTL)
- Duplicate detection and simple TTL to limit rebroadcast storms

## Week-by-week Timeline (Summary)
- Week 1 (Research & Setup): environment setup, hardware selection, Arduino IDE configuration, and ESP-NOW primer.
- Week 2 (Core Module): ultrasonic sensing and ESP-NOW broadcast (code placeholder).
- Week 3 (Integration): multi-node reception, local map updates and re-broadcast logic (placeholder).
- Week 4 (Enhancement): simple leader-follower or swarm algorithm using color-based localization (placeholder).
- Week 5 (Final): testing, documentation, and demonstration (placeholder).

Full timeline and extended notes are available in `Docs/report.tex`.

## Quick Start (Hardware & Software)
Prerequisites:

- Arduino IDE 2.x
- ESP32 board support added via Boards Manager (Espressif)
- USB cable and three ESP32 development boards
- 2 remote-controlled toy cars (for vehicle platforms)
- HC-SR04 ultrasonic sensors (x2)
- Color sensors TCS3200 / TCS34725 (x2)

Setup steps (high level):

1. Install Arduino IDE and add ESP32 board support (Boards Manager). Refer to the official Espressif documentation.
2. Install libraries: `esp_now`, `WiFi`, and sensor-specific libraries (`Adafruit_TCS34725`, or TCS3200 libraries), if needed.
3. Wire sensors to ESP32s according to schematics in `hardware/schematics` (create if missing).
4. Flash the master and slave sketches from `software/master` and `software/slave` when available.

Example Arduino board manager URL (if needed):
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

## Development Notes
- Use the Serial Monitor for debugging and verifying ESP-NOW packet exchange.
- Start with one transmitter and one receiver to validate message formats before scaling to three nodes.
- Keep messages concise (esp. when broadcasting) to reduce collisions and latency.

## Contribution Guidelines
- This project is primarily an academic submission. If you want to contribute:
	1. Open an issue describing the change.
	2. Create feature branches and submit PRs with clear descriptions and tests/code comments.

## Testing
- Unit testing is informal: use serial logs and controlled grid tests with colored squares for localization.
- Record test videos for demonstrations; keep logs from the master node for post-analysis.

## Known Limitations
- ESP-NOW is connectionless and has basic reliability; implement simple ACKs or TTLs to improve robustness.
- Localization is simplistic (color-grid) and assumes controlled lighting conditions.

## Roadmap / Next Steps
- Implement and test full code for slave/master nodes under `software/`.
- Add schematic diagrams to `hardware/schematics` with clear pinouts.
- Implement visualization tools (Python script) to render aggregated maps from master logs.

## License
This repository uses the MIT license. See `LICENSE` (add file) for details.

---

_Prepared for the Introduction to Computer Networks course — German International University_
