# 🟢 Project: UselessBoxes

A design for paired **UselessBoxes** that can be owned separately and used to interact between friends.

---

![Arduino](https://img.shields.io/badge/Platform-Arduino-blue)
![ESP32](https://img.shields.io/badge/Board-Nano%20ESP32-green)
![License](https://img.shields.io/badge/License-Proprietary-lightgrey)

---

## 📅 Project Info

- **Author:** Michael Marsland  
- **Email:** mmarsland@mac.com  
- **Date:** 10/10/2025  
- **Revision:** 10/10/2025 - Alpha Dev  
- **License:** Proprietary

---

## 🛠 Step 1: Installation

Currently using **Arduino Cloud** to set up the device as a "Thing" and a dashboard to control and monitor the variables.  

---

## ⚡ Step 2: Assemble the Circuit

### 🧩 Components

- Arduino Nano ESP32 (3.3V logic)
- L293DNE H-Bridge motor driver
- DC motor
- SPDT switch (for direction)
- Momentary push button (for stop)
- 6V DC input
- Jumper wires
- Common GND for all components

### ⚙️ Wiring Connections

#### 🧠 Arduino Nano ESP32

| Pin  | Connected To                                        | Purpose |
|------|-----------------------------------------------------|---------|
| D2   | H-Bridge IN1                                        | Motor direction control A |
| D3   | H-Bridge IN2                                        | Motor direction control B |
| D4   | H-Bridge EN1                                        | Motor enable / PWM speed control |
| D5   | —                                                   | Unused (kept as a spacer between outputs and inputs) |
| D6   | Switch Throw A & 10kΩ Pull-down resistor to GND     | Reads switch position (forward/reverse) |
| D7   | Button NO terminal & 10kΩ Pull-down resistor to GND | Reads button press (stop signal) |
| 3V3  | —                                                   | Not used for inputs (external pull-down resistors used) |
| GND  | Common Ground                                       | Common ground for all components |

#### 🐜 L293DNE (H-Bridge)

| L293DNE Pin | Connected To | Description |
|--------------|--------------|-------------|
| Pin 1 (EN1)  | Arduino D4   | Motor enable (PWM capable) |
| Pin 2 (IN1)  | Arduino D2   | Input 1 for motor direction |
| Pin 7 (IN2)  | Arduino D3   | Input 2 for motor direction |
| Pin 3 (OUT1) | Motor Terminal 1 | Motor output A |
| Pin 6 (OUT2) | Motor Terminal 2 | Motor output B |
| Pin 4, 5, 12, 13 (GND) | Arduino GND / Common Ground | Common ground |
| Pin 8 (VCC2) | +6V external motor power | Motor supply voltage |
| Pin 16 (VCC1)| +6V external motor power | Logic power supply for L293DNE |

#### 🔀 SPDT Switch (2-position)

| Switch Terminal | Connected To | Purpose |
|-----------------|--------------|---------|
| Common (C)      | GND          | Pulls the input LOW when connected |
| Throw A         | Arduino D6   | Reads switch position (forward) |
| Throw B         | — (leave unconnected) | Arduino infers reverse when D6 reads LOW due to GND connection |

**Logic (with `INPUT_PULLUP`):**  
- **HIGH** → Forward (not connected)  
- **LOW** → Reverse (connected to GND)  

#### 🔘 Button (Momentary, C / NC / NO)

| Button Terminal | Connected To | Purpose |
|-----------------|--------------|---------|
| Common (C)      | GND          | Pulls input LOW when pressed |
| Normally Open (NO) | —         | Not used |
| Normally Closed (NC) | Arduino D7 | Reads button press |
| —               | (Internal pull-up active on D7) | Pull-up used instead of external resistor |

**Logic (with `INPUT_PULLUP`):**  
- **HIGH** → Button pressed (stop motor)  
- **LOW** → Button released  

---

## 💻 Step 3: Load the Code

Upload the code contained in this sketch onto your board via Arduino Cloud or Arduino IDE.

### 📂 Folder Structure

- [README.md](README.md) — Project README and Instructions (This File)  
- [.gitignore](.gitignore) — Git Ignore  
- [sketch.json](sketch.json) — Something to do with Arduino Cloud  

- [docs/](docs/) — Documentation Folder (Schematics)  
  - [UselessBoxes.fzz](docs/UselessBoxes.fzz) — Fritzing Circuit Diagram  

- [Useless_Boxes/](Useless_Boxes/) — Arduino sketch folder  
  - [Useless_Boxes.ino](Useless_Boxes/Useless_Boxes.ino) — Main Arduino file  
  - [arduino_secrets.h](Useless_Boxes/arduino_secrets.h) — Secrets for the Devices and Arduino Cloud  
  - [thingProperties.h](Useless_Boxes/thingProperties.h) — Properties for Arduino Cloud

---

## 🤝 Contributing
To contribute to this project, please contact:  

**Michael Marsland**  
Email: mmarsland@mac.com  

---

## 📝 Bill of Materials (BOM) (TODO)

| ID  | Part name      | Part number | Quantity |
|-----|----------------|-------=-----|----------|
|     |                |             |          |