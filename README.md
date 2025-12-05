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

**Project Description:**

The Useful Box project started a few years ago when my friend Trevor Johns gifted me a "Useless Box" for my birthday. A "Useless Box" is simply a small electro-mechanical device that sits on your desk and has a single switch on top. Upon flicking the switch, a small arm rises from the box to de-activate the switch, and then the cycle repeats.

This gift inspired the idea for a small IoT project where I could build Trevor and I each a similarly "Useless Box" for our desks except with the main change being, my switch would activate his arm to flick his switch off, and him turning his switch on would activate the arm on my box to turn my switch off.

Of course, I also took the opportunity to improve on the design of the box, adding LEDs, buzzers, settings buttons, and a sleeker look to make us the perfect desktop interactive toys. So while we're busy working on our other projects be it for work or personal interest, Trevor and I will be able to maintain our connection across the internet and still get on each others nerves even though we don't live in the same city anymore.

This project has been in the works for quite a while now and it is now in the late stages of assembly as I prepare to have it ready for his birthday this year (End of December).

I would like to see this project grow after the development of this first prototype so Trevor and I may collaborate on more iterations and maybe even add some of our other friends and create a larger interconnected network of slightly less useless "Useless Boxes" to all bother each other at once!

Let me know what you think and stay tuned for more updates!

---

## 🛠 Step 1: Installation

Currently using **Arduino Cloud** to set up the device as a "Thing" and a dashboard to control and monitor the variables.

### For Development:

I am currently developing in VSCode using the PlatformIO extension. I started in the ArduinoCloud Editor, and then copied UselessBoxes.ino, thingProperties.h, and arduino_secrets.h from there into my local ArduinoIDE. Then I installed all the required libraries from the ArduinoIDE (ArduinoIoTCloud, etc...), then I was able to make a project in PlatformIO in VS code and copy over the files. The modifications you must make to get it building in PlatformIO are:

1. Add these lines to platformio.ini
   ```
   lib_extra_dirs = ~/Documents/Arduino/libraries
   lib_ignore = WiFiNINA
   ```
2. Change Useless_Boxes.ino to Useless_Boxes.cpp
3. Write a "Useless_Boxes.h" with variable and function definitions (Ask an LLM for the header from the .cpp file)
3. Include `<Arduino>` and "Useless_Boxes.h" in Useless_Boxes.cpp
4. Include "arduino_secrets.h" in thingProperties.h

When running the code from this repository, all you should need to do is clone the repo, and then ensure you have Arduino ESP32 Boards by Arduino (2.0.18-arduino.5) installed in the ArduinoIDE, and the ArduinoIoTCloud by Arduino (2.8.0) library installed. Once these (And all dependencies) are installed in the ArduinoIDE, the project should build with PlatformIO in VSCode (Knock on Wood)

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

## Useless Box Wiring Connections

## 🧠 Arduino Nano ESP32

| Pin           | Connected To                                             | Purpose / Notes                           |
| ------------- | -------------------------------------------------------- | ----------------------------------------- |
| **D2**  | L293D**Pin 2 (IN1)**                               | Motor direction control A (digital OUT).  |
| **D3**  | L293D**Pin 7 (IN2)**                               | Motor direction control B (digital OUT).  |
| **D4**  | L293D**Pin 1 (EN1)**                               | Motor enable / PWM speed control.         |
| **D5**  | RGB LED**Blue** → 220Ω → LED **B Pin**    | Blue channel (PWM).                       |
| **D6**  | RGB LED**Green** → 220Ω → LED **G Pin**   | Green channel (PWM).                      |
| **D7**  | RGB LED**Red** → 220Ω → LED **R Pin**     | Red channel (PWM).                        |
| **D8**  | SPDT Switch**Throw A**                             | Reads switch position (`INPUT_PULLUP`). |
| **D9**  | Limit Switch**NC Terminal**                        | Reads limit state (`INPUT_PULLUP`).     |
| **D10** | Button**NC Terminal**                              | Reads button press (`INPUT_PULLUP`).    |
| **D11** | Buzzer**+ Pin**                                          | Buzzer output (tone / PWM).               |
| **3V3** | —                                                       | Not used.                                 |
| **GND** | All grounds (switches, LED common, buzzer −, L293D GND) | Shared ground.                            |

## 🐜 L293DNE (H-Bridge)

| L293D Pin                  | Function    | Connected To         | Notes              |
| -------------------------- | ----------- | -------------------- | ------------------ |
| **Pin 1 (EN1)**      | Enable 1    | Arduino**D4**  | PWM motor control  |
| **Pin 2 (IN1)**      | Input 1     | Arduino**D2**  | Direction A        |
| **Pin 7 (IN2)**      | Input 2     | Arduino**D3**  | Direction B        |
| **Pin 3 (OUT1)**     | Motor A     | Motor Terminal 1     | —                 |
| **Pin 6 (OUT2)**     | Motor B     | Motor Terminal 2     | —                 |
| **Pin 4, 5, 12, 13** | GND         | Arduino**GND** | Common ground      |
| **Pin 8 (VCC2)**     | Motor Power | +6V external         | Motor supply       |
| **Pin 16 (VCC1)**    | Logic Power | +5V supply           | L293D logic supply |

## 🔀 SPDT Switch (2-position)

| Switch Terminal      | Connected To        | Purpose                    |
| -------------------- | ------------------- | -------------------------- |
| **Common (C)** | GND                 | Provides LOW when selected |
| **Throw A**    | Arduino**D8** | Reads switch state         |
| **Throw B**    | —                  | Unused                     |

## 🧱 Limit Switch (Momentary)

| Terminal     | Connected To                       | Purpose                                     |
| ------------ | ---------------------------------- | ------------------------------------------- |
| **C**  | GND                                | LOW when triggered                          |
| **NC** | Arduino**D9** (INPUT_PULLUP) | Default connected → LOW when limit reached |
| **NO** | —                                 | Not used                                    |

## 🔘 Button (Momentary)

| Terminal     | Connected To                        | Purpose                |
| ------------ | ----------------------------------- | ---------------------- |
| **C**  | GND                                 | Pulls LOW when pressed |
| **NC** | Arduino**D10** (INPUT_PULLUP) | Reads button press     |
| **NO** | —                                  | Not used               |

## 🔊 Buzzer

| Buzzer Pin   | Connected To         | Purpose             |
| ------------ | -------------------- | ------------------- |
| **+**  | Arduino**D11** | Output for tone/PWM |
| **–** | GND                  | —                  |

## 🌈 RGB LED (Common Cathode recommended)

Each color pin requires a **220Ω resistor**.

| LED Pin                | Connected To                 | Notes         |
| ---------------------- | ---------------------------- | ------------- |
| **R (Red)**      | 220Ω → Arduino**D7** | PWM capable   |
| **G (Green)**    | 220Ω → Arduino**D6** | PWM capable   |
| **B (Blue)**     | 220Ω → Arduino**D5** | PWM capable   |
| **Common Anode** | GND                          | Shared ground |

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

| ID | Part name                                      | Part number | Quantity |
| -- | ---------------------------------------------- | ----------- | -------- |
|    | Arduino Nano ESP32                             |             | 1        |
|    | RGB Led                                        |             | 1        |
|    | 220 Ohm Resistors                              |             | 3        |
|    | Piezo Buzzer                                   | XYDZ        | 1        |
|    | Button                                         |             | 1        |
|    | Limit Switch                                   |             | 1        |
|    | SPDT Switch                                    |             | 1        |
|    | H-Bridge )                                     | L293DE      | 1        |
|    | Barrel Jack Connector                          |             | 1        |
|    | Motor, Gear Box, and Armature from Useless Box |             | 1        |
|    | 6V/0.83A Power Supply                          |             | 1        |
|    | Wires (Approx 25)                              |             | ~25      |
|    | M2x6 Bolts & M2 Nuts                           |             | 6        |
|    |                                                |             |          |
|    |                                                |             |          |
|    |                                                |             |          |

---

## 🛣️ Project Roadmap

A step-by-step roadmap to finish the UselessBoxes project, from platform selection to final hardware.

### 🖥️ SOFTWARE

#### Decide on the Interconnection Platform

- Evaluate options:
  - Arduino Cloud
  - Thinger.io
  - Your own website with custom backend
- Consider:
  - Free tier limitations
  - Data synchronization capabilities
  - Ease of integration with Arduino Nano ESP32

**Decided to stick with Arduino Cloud for V1**
It will highly simplify the steps required to get a functioning system running.
And it will be free for up to two devices and 5 variables per device.
This should be enough to control the motors and even a few settings.
I can also determine a way to adjust settings on the device (A button perhaps and detailed procedures).
I can upgrade from Arduino Cloud to a different free option in the future such as using my website in V2.

~~#### Choose a Frontend Dashboard Library~~
~~- If using your own website, select a library for real-time dashboards:~~
~~- Examples: Chart.js, D3.js, React + Recharts, or Vue + ECharts~~
~~- Ensure compatibility with live updates and WebSocket or REST API communication~~

~~#### Plan Website Architecture (Assuming Website)~~
~~- Define:~~
~~- Backend (Node.js / Express / Firebase Functions)~~
~~- Database (Cloud Firestore, MySQL, or PostgreSQL)~~
~~- Frontend (Dashboard, user interface)~~
~~- API endpoints for Arduino devices~~
~~- Authentication flow (Firebase Auth, JWT tokens)~~

~~#### Set Up Device Communication~~
~~- Establish communication between Arduino Nano ESP32 and website backend~~
~~- Options: HTTP POST, WebSocket, MQTT (via your own broker)~~
~~- Test sending simple sensor or motor state data~~
~~- Test sending commands / setting updates to the devices~~

~~#### Ensure Proper Device Authentication~~
~~- Assign unique device IDs or secrets~~
~~- Implement secure authentication when Arduino connects to your backend~~
~~- Prevent unauthorized devices from sending or receiving data~~

~~#### Set Up Database on Website~~
~~- Store device states, logs, and user data~~
~~- Recommended options:~~
~~- Firebase Firestore for simplicity~~
~~- MySQL/PostgreSQL if using Node.js backend~~
~~- Define schema for:~~
~~- Devices~~
~~- Motor state~~
~~- Dashboard data~~
~~- Users~~

~~#### Build the Intercommunication Dashboard~~
~~- Display real-time motor status and device states~~
~~- Allow manual control of motors (optional)~~
~~- Include live logs for troubleshooting~~

~~#### Add Firebase Authentication for Dashboard Access~~
~~- Restrict dashboard access to yourself and your friend~~
~~- Options:~~
~~- Email/password login~~
~~- Google Sign-In~~
~~- Integrate auth into your frontend dashboard~~

#### Ensure Interdevice Communication and Functionality

- Test interactions between paired UselessBoxes
- Verify:
  - State changes propagate correctly
  - Motors respond as expected
  - Dashboard reflects real-time updates

### ⚙️ HARDWARE

#### Test NeoPixels

- Just ordered a logic level shifter and I will re-test
- They seem like they will work once I have the shifter

#### Design Hardware

- Will need to decide on final wiring, how will the wires connect, where/how will the eletronics be seated
- Prototype and design 3D printed housing
- Ensure components can fit and function
- Make design simple, and practical for desk use

#### Finalize Hardware

- Secure all components in the box
- Ensure wiring is neat and safe
- Test full functionality before duplicating

#### Build the Second Box

- Replicate hardware and software setup
- Connect to the same platform
- Test intercommunication between the two boxes

#### Deployment Features

- Setup Over-The-Air updates so the software can be modified
- Setup wifi connection process so that devices can be easily set up

##### ✅ Notes

- Iteratively test hardware and software at each stage
- Prioritize secure authentication and device identification
- Document wiring, code, and backend API endpoints for reproducibility
