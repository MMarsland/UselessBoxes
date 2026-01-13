/* 
  Useless Boxes - Core Logic
  -------------------------
  Handles main loop, button input, motor control, RGB and buzzer management,
  and serial menu interface.
  Integrates with Arduino IoT Cloud for remote control of Active Box setting.
  -------------------------
  Created by Michael Marsland, 2024-2025

  Arduino IoT Cloud Variables description
  The following variables are automatically generated and updated when changes are made to the Thing

  String active_box; // READ/WRITE
  
  Variables which are marked as READ/WRITE in the Cloud Thing will also have functions
  which are called when their values are changed from the Dashboard.
  These functions are generated with the Thing and added at the end of this sketch.
*/
#include <Arduino.h>
#include <Preferences.h>
#include "Useless_Boxes.h"
#include "thingProperties.h"

// (Hardware pin mappings live in the header as `constexpr` values)

// ------------------------------------------------------------------
// Runtime-configurable settings (initialized from DEFAULT_* values)
// ------------------------------------------------------------------
unsigned long LONG_PRESS_TIME = DEFAULT_LONG_PRESS_TIME;   // ms  // Adjustable
unsigned long DEBOUNCE_TIME   = DEFAULT_DEBOUNCE_TIME;     // ms  // Adjustable
unsigned long MENU_TIMEOUT_MS = DEFAULT_MENU_TIMEOUT_MS;   // ms  // Adjustable
unsigned long MOTOR_UPDATE_INTERVAL = DEFAULT_MOTOR_UPDATE_INTERVAL; // ms // Adjustable


// ------------------------------------------------------------------
// RGB / animation state (shared with other helpers)
// ------------------------------------------------------------------
int currentRGBMode = RGB_RAINBOW;
unsigned long lastRGBAnimation = 0;
int rainbowPos = 0;
int breathValue = 0;
int breathDir = 1;

// Active/Inactive presets (persisted)
int activeRGBSetting = RGB_RAINBOW;
int inactiveRGBSetting = RGB_SOLID_RED;
int activeBuzzerSetting = BUZZER_CHIRP;
int inactiveBuzzerSetting = BUZZER_SINGLE;
int rgb_brightness_percentage = DEFAULT_RGB_BRIGHTNESS_PERCENTAGE; // % Setting
int motorSpeed = 100; // 0-100% - controls PWM duty cycle

// ------------------------------------------------------------------
// Buzzer state
// ------------------------------------------------------------------
int currentBuzzerPattern = BUZZER_OFF;    // active buzzer pattern playback state
bool buzzerState = false;           // on/off state for looping patterns
unsigned long buzzerLast = 0;       // last toggle time
unsigned int buzzerStep = 0;        // step in sequence
bool buzzerDemo = false;      // true when playing a demo pattern (i.e. for a second
unsigned long buzzerDemoStart = 0; // start time of demo pattern
constexpr unsigned long BUZZER_DEMO_DURATION = 5000; // ms

// ------------------------------------------------------------------
// File-local internal state (kept private to this .cpp)
// ------------------------------------------------------------------
namespace {
  int menuIndex = 0;
  bool inSubMenu = false;

  // Button tracking
  bool settingsButtonState = HIGH;
  bool lastSettingsButtonState = HIGH;
  unsigned long pressedTime = 0;
  unsigned long releasedTime = 0;
  bool longPressActive = false;
  unsigned long lastDebounceTime = 0;
  int shortPressCount = 0;
  int longPressCount  = 0;
  int lastShortPressCount = 0;
  int lastLongPressCount  = 0;
  // Inactivity timeout tracking
  unsigned long lastInteractionTime = 0;  // resets every button press

  // Motor timing / state
  unsigned long lastMotorUpdate = 0;
  bool switch_forward = false; 
  bool limit_pressed = false;
  bool stateChanged = false;
  
  // Motor soft PWM control (dynamic timing based on motorSpeed)
  int motorDirection = 0;  // 1=forward, -1=reverse, 0=stopped
  bool motorShouldRun = false;  // Motor should be running
  unsigned long lastMotorPWMUpdate = 0;
  bool motorPWMEnabled = false;  // Current state of PWM (on or off)
  const unsigned long MOTOR_PWM_CYCLE_TIME = 10;  // 20ms total cycle
}

// Preferences (non-volatile storage) instance
static Preferences prefs;

// ------------------------------------------------------------------
// Setter implementations (validate and apply side-effects)
// Persist user-adjustable settings to non-volatile storage.
// ------------------------------------------------------------------
void setLongPressTime(unsigned long ms) { LONG_PRESS_TIME = ms; }
void setDebounceTime(unsigned long ms) { DEBOUNCE_TIME = ms; }
void setMenuTimeout(unsigned long ms) { MENU_TIMEOUT_MS = ms; }
void setMotorUpdateInterval(unsigned long ms) { MOTOR_UPDATE_INTERVAL = ms; }

// ===== Active/Inactive preset setters =====
void setActiveRGBSetting(int mode) {
  activeRGBSetting = mode;
  prefs.putInt("active_rgb", activeRGBSetting);
}

void setInactiveRGBSetting(int mode) {
  inactiveRGBSetting = mode;
  prefs.putInt("inactive_rgb", inactiveRGBSetting);
}

void setRGBBrightness(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  rgb_brightness_percentage = percent;
  applyRGBMode();
  prefs.putInt("rgb_brightness", rgb_brightness_percentage);
}

void setActiveBuzzerSetting(int pattern) {
  activeBuzzerSetting = pattern;
  prefs.putInt("active_buzzer", activeBuzzerSetting);
}

void setInactiveBuzzerSetting(int pattern) {
  inactiveBuzzerSetting = pattern;
  prefs.putInt("inactive_buzzer", inactiveBuzzerSetting);
}

void setMotorSpeed(int speed) {
  if (speed <= 0) speed = 0;
  if (speed > 100) speed = 100;
  motorSpeed = speed;
  prefs.putInt("motor_speed", motorSpeed);
}


// Load persisted settings (call during setup after prefs.begin())
void loadPersistentSettings() {
  // Active/Inactive presets
  activeRGBSetting = (RGBMode)prefs.getInt("active_rgb", activeRGBSetting);
  inactiveRGBSetting = (RGBMode)prefs.getInt("inactive_rgb", inactiveRGBSetting);
  rgb_brightness_percentage = prefs.getInt("rgb_brightness", rgb_brightness_percentage);
  activeBuzzerSetting = prefs.getInt("active_buzzer", activeBuzzerSetting);
  inactiveBuzzerSetting = prefs.getInt("inactive_buzzer", inactiveBuzzerSetting);
  motorSpeed = prefs.getInt("motor_speed", motorSpeed);
  // initialize buzzer runtime state
  buzzerStep = 0;
  buzzerState = false;
  buzzerLast = millis();
  // apply loaded values
  applyRGBMode();
}


// ==================================================================
// === SETUP ========================================================
// ==================================================================
void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(200); 

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  /*
     The following function allows you to obtain more information
     related to the state of network and IoT Cloud connection and errors
     the higher number the more granular information you’ll get.
     The default is 0 (only errors).
     Maximum is 4
 */
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // Open non-volatile storage namespace and load any saved settings
  prefs.begin("useless_box", false);
  loadPersistentSettings();

  // Set the Board LED as outputs (kept OFF — not configurable)
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  analogWrite(LED_RED, 255);
  analogWrite(LED_GREEN, 255);
  analogWrite(LED_BLUE, 255);

  // Set the RGB LED as outputs
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  // Turn LED fully off at startup
  setRGB(0, 0, 0);

  // Set the Buzzer Pin as an output
  pinMode(BUZZER_PIN, OUTPUT);

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(EN1, OUTPUT);
  digitalWrite(EN1, LOW); // Disable motor at startup

  // Inputs with internal pull-ups
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  // Settings button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Reflect starting state
  onActiveBoxChange();
  updateRGBModeFromBoxState();
  Serial.println("System Initialized.");
  showMenu();
}

// ==================================================================
// === MAIN LOOP ====================================================
// ==================================================================
void loop() {
    ArduinoCloud.update();
    handleSettingsButton();
    handleSwitchDetection();

    if (millis() - lastMotorUpdate >= MOTOR_UPDATE_INTERVAL) {
      lastMotorUpdate = millis();
      updateMotorPWM();
    }

    updateAnimations();   // RGB Effects (rainbow, pulse, etc)
    updateBuzzerAlarm();  // Buzzer Patterns
    handleSerialMenu();
}


// ==================================================================
// === SETTINGS BUTTON HANDLER ======================================
// ==================================================================
void handleSettingsButton() {
  bool reading = digitalRead(BUTTON_PIN);

  // Debounce
  if (reading != lastSettingsButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
    if (reading != settingsButtonState) {
      settingsButtonState = reading;

      // Just pressed
      if (settingsButtonState == LOW) {
        pressedTime = millis();
        longPressActive = false;
      }

      // Just released
      else {
        unsigned long pressDuration = millis() - pressedTime;
        if (pressDuration < LONG_PRESS_TIME && !longPressActive) {
          shortPressCount++;
          //Serial.print("Short press #");
          //Serial.println(shortPressCount);
        }
      }
    }
  }

  // Detect long press
  if (settingsButtonState == LOW && !longPressActive && 
      (millis() - pressedTime > LONG_PRESS_TIME)) {
    longPressActive = true;
    longPressCount++;
    //Serial.print("Long press #");
    //Serial.println(longPressCount);
  }

  lastSettingsButtonState = reading;
}

// ==================================================================
// === SETTINGS MENU (Refactored / Data-Driven) ======================
// ==================================================================

/*
 This system is fully modular:
  - To add a new menu, create:
      showX(), adjustX(), enterX()
  - Then add one line inside the menuItems[] table below.

 Core logic stays unchanged forever.
*/

// Forward declarations for menu handler functions

MenuItem menuItems[] = { 
  { "Active RGB",           showActiveRGB,           adjustActiveRGB,           confirmActiveRGB },
  { "Inactive RGB",         showInactiveRGB,         adjustInactiveRGB,         confirmInactiveRGB },
  { "RGB Brightness",       showRGBBrightness,       adjustRGBBrightness,       confirmRGBBrightness },
  { "Active Buzzer",        showActiveBuzzerSetting, adjustActiveBuzzerSetting, confirmActiveBuzzerSetting },
  { "Inactive Buzzer",      showInactiveBuzzerSetting, adjustInactiveBuzzerSetting, confirmInactiveBuzzerSetting },
  { "Motor Speed",          showMotorSpeed,          adjustMotorSpeed,          confirmMotorSpeed }
};

int totalMenus = sizeof(menuItems) / sizeof(MenuItem);

// === MAIN MENU HANDLER (Button-driven navigation) ===
void handleSerialMenu() {
  unsigned long now = millis();

  // Remove Timeout Functionality
  // // Timeout → return to main screen
  // if ((now - lastInteractionTime) > MENU_TIMEOUT_MS && (menuIndex != 0 || inSubMenu)) {
  //   Serial.println("\n⏱️ Menu timed out — returning to main screen.\n");
  //   menuIndex = 0;
  //   inSubMenu = false;
  //   showMenu();
  //   beepBuzzer(1, 2000, 0); // Long beep
  // }

  // Short press: next menu item OR adjust submenu value
  if (shortPressCount > lastShortPressCount) {
    lastShortPressCount = shortPressCount;
    lastInteractionTime = now;

    if (!inSubMenu) {
      menuIndex = (menuIndex + 1) % totalMenus;
      showMenu();
      beepBuzzer(menuIndex+1, 100, 100);
    } else {
      menuItems[menuIndex].onAdjust();
    }
  }

  // Long press: enter submenu OR confirm/save
  if (longPressCount > lastLongPressCount) {
    lastLongPressCount = longPressCount;
    lastInteractionTime = now;

    if (!inSubMenu) {
      inSubMenu = true;
      Serial.print("⚙️ Editing ");
      Serial.println(menuItems[menuIndex].name);
      menuItems[menuIndex].onShow();
      beepBuzzer(1, 500, 100, 800);
    } else {
      inSubMenu = false;
      menuItems[menuIndex].onConfirm();
      Serial.println("✅ Saved and returned to main menu.");
      showMenu();
      beepBuzzer(1, 500, 100, 1200);
    }
  }
}

// === MENU DISPLAY ===
void showMenu() {
  Serial.println();
  Serial.print("> Setting ");
  Serial.print(menuIndex + 1);
  Serial.print(": ");
  Serial.println(menuItems[menuIndex].name);

  // Optional: live preview under each menu
  if (menuItems[menuIndex].onShow) {
    menuItems[menuIndex].onShow();
  }
}

// ==================================================================
// === INDIVIDUAL MENU HANDLERS =====================================
// ==================================================================

// ---------------- ACTIVE RGB PRESET ----------------
void showActiveRGB() {
  Serial.print("Active RGB Mode: ");
  switch(activeRGBSetting) {
    case RGB_OFF:        Serial.println("OFF"); break;
    case RGB_WHITE:      Serial.println("WHITE"); break;
    case RGB_RAINBOW:    Serial.println("RAINBOW"); break;
    case RGB_BREATHING:  Serial.println("BREATHING"); break;
    case RGB_SOLID_RED:  Serial.println("RED"); break;
    case RGB_SOLID_GREEN:Serial.println("GREEN"); break;
    case RGB_SOLID_BLUE: Serial.println("BLUE"); break;
    default:             Serial.println("UNKNOWN"); break;
  }
  currentRGBMode = activeRGBSetting;
  applyRGBMode();
}
void adjustActiveRGB() {
  activeRGBSetting = (activeRGBSetting + 1) % RGB_MODE_COUNT;
  setActiveRGBSetting(activeRGBSetting);
  showActiveRGB();
  beepBuzzer(1, 100, 100);
}
void confirmActiveRGB() { 
  showActiveRGB(); 
  updateRGBModeFromBoxState();
}

// ---------------- INACTIVE RGB PRESET ----------------
void showInactiveRGB() {
  Serial.print("Inactive RGB Mode: ");
  switch(inactiveRGBSetting) {
    case RGB_OFF:        Serial.println("OFF"); break;
    case RGB_WHITE:      Serial.println("WHITE"); break;
    case RGB_RAINBOW:    Serial.println("RAINBOW"); break;
    case RGB_BREATHING:  Serial.println("BREATHING"); break;
    case RGB_SOLID_RED:  Serial.println("RED"); break;
    case RGB_SOLID_GREEN:Serial.println("GREEN"); break;
    case RGB_SOLID_BLUE: Serial.println("BLUE"); break;
    default:             Serial.println("UNKNOWN"); break;
  }
  currentRGBMode = inactiveRGBSetting;
  applyRGBMode();
}
void adjustInactiveRGB() {
  inactiveRGBSetting = (inactiveRGBSetting + 1) % RGB_MODE_COUNT;
  setInactiveRGBSetting(inactiveRGBSetting);
  showInactiveRGB();

  beepBuzzer(1, 100, 100);
}
void confirmInactiveRGB() { 
  showInactiveRGB();
  updateRGBModeFromBoxState();
}

// ---------------- RGB BRIGHTNESS ----------------
void showRGBBrightness() {
  Serial.print("RGB Brightness: ");
  Serial.print(rgb_brightness_percentage);
  Serial.println("%");
}
void adjustRGBBrightness() {
  int next = rgb_brightness_percentage + 10;
  if (next > 100) next = 0;
  setRGBBrightness(next);
  showRGBBrightness();
  beepBuzzer(next/10, 100, 100);
}
void confirmRGBBrightness() {
  showRGBBrightness();
}

// ---------------- ACTIVE BUZZER PRESET ----------------
void showActiveBuzzerSetting() {
  Serial.print("Active Buzzer: ");
  switch(activeBuzzerSetting) {
    case BUZZER_OFF:    Serial.println("OFF"); break;
    case BUZZER_SINGLE: Serial.println("SINGLE"); break;
    case BUZZER_CHIRP:  Serial.println("CHIRP"); break;
    case BUZZER_LOOP:   Serial.println("LOOP"); break;
    case BUZZER_SOS:    Serial.println("SOS"); break;
    default:            Serial.println("UNKNOWN"); break;
  }
}
void adjustActiveBuzzerSetting() {
  activeBuzzerSetting = (activeBuzzerSetting + 1) % BUZZER_PATTERN_COUNT;
  setActiveBuzzerSetting(activeBuzzerSetting);
  demoBuzzerPattern(activeBuzzerSetting);
  showActiveBuzzerSetting();
}
void confirmActiveBuzzerSetting() {
  showActiveBuzzerSetting();
}

// ---------------- INACTIVE BUZZER PRESET ----------------
void showInactiveBuzzerSetting() {
  Serial.print("Inactive Buzzer: ");
  switch(inactiveBuzzerSetting) {
    case BUZZER_OFF:    Serial.println("OFF"); break;
    case BUZZER_SINGLE: Serial.println("SINGLE"); break;
    case BUZZER_CHIRP:  Serial.println("CHIRP"); break;
    case BUZZER_LOOP:   Serial.println("LOOP"); break;
    case BUZZER_SOS:    Serial.println("SOS"); break;
    default:            Serial.println("UNKNOWN"); break;
  }
}
void adjustInactiveBuzzerSetting() {
  inactiveBuzzerSetting = (inactiveBuzzerSetting + 1) % BUZZER_PATTERN_COUNT;
  setInactiveBuzzerSetting(inactiveBuzzerSetting);
  demoBuzzerPattern(inactiveBuzzerSetting);
  showInactiveBuzzerSetting();
}
void confirmInactiveBuzzerSetting() {
  showInactiveBuzzerSetting();
}

// ---------- MOTOR SPEED ----------------
void showMotorSpeed() {
  Serial.print("Motor Speed: ");
  Serial.print(motorSpeed);
  Serial.println("%");
}
void adjustMotorSpeed() {
  int next = motorSpeed + 10;
  if (next > 100) next = 40;
  setMotorSpeed(next);
  showMotorSpeed();
  beepBuzzer(next/10, 100, 100);
}
void confirmMotorSpeed() {
  showMotorSpeed();
}
// ==================================================================


// === RGB LED CONTROL ==============================================
void updateRGBModeFromBoxState() {
  // Set currentRGBMode based on whether this box is active or inactive
  if (active_box == BOX_NAME) {
    // This box is active
    currentRGBMode = activeRGBSetting;
  } else {
    // This box is inactive (either another box is active or no box is active)
    currentRGBMode = inactiveRGBSetting;
  }
  applyRGBMode();
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Apply brightness scaling
  r = (r * rgb_brightness_percentage) / 100;
  g = (g * rgb_brightness_percentage) / 100;
  b = (b * rgb_brightness_percentage) / 100;
  // Common anode inversion
  analogWrite(RGB_R, 255 - r);
  analogWrite(RGB_G, 255 - g);
  analogWrite(RGB_B, 255 - b);
}

void applyRGBMode() {
  switch (currentRGBMode) {
    case RGB_OFF:
      setRGB(0,0,0);
      break;
    case RGB_WHITE:
      setRGB(255,255,255);
      break;
    case RGB_SOLID_RED:
      setRGB(255,0,0);
      break;
    case RGB_SOLID_GREEN:
      setRGB(0,255,0);
      break;
    case RGB_SOLID_BLUE:
      setRGB(0,0,255);
      break;
    case RGB_RAINBOW:
    case RGB_BREATHING:
      // handled in updateAnimations()
      break;
  }
}

void updateAnimations() {
  unsigned long now = millis();
  
  if (currentRGBMode == RGB_RAINBOW && now - lastRGBAnimation > RGB_UPDATE_INTERVAL) {
    lastRGBAnimation = now;
    int r = (sin((rainbowPos) * 0.05) * 127) + 128;
    int g = (sin((rainbowPos*2) * 0.05) * 127) + 128;
    int b = (sin((rainbowPos*3) * 0.05) * 127) + 128;
    setRGB(r,g,b);
    rainbowPos++;
  }

  if (currentRGBMode == RGB_BREATHING && now - lastRGBAnimation > RGB_UPDATE_INTERVAL) {
    lastRGBAnimation = now;
    breathValue += breathDir * 2; // adjust speed
    if (breathValue >= 250) breathDir = -1;
    if (breathValue <= 5) breathDir = 1;
    setRGB(breathValue, breathValue, breathValue);
  }
}


// === BUZZER CONTROL ===============================================
void triggerBuzzerPattern(int pattern) {
  currentBuzzerPattern = pattern;
  buzzerStep = 0;
  buzzerState = false;
  buzzerLast = millis();
  noTone(BUZZER_PIN);
}

void demoBuzzerPattern(int pattern) {
  buzzerDemo = true;
  buzzerDemoStart = millis();
  triggerBuzzerPattern(pattern);
}

void beepBuzzer(int quantity, int duration_ms, int pause_ms, int toneFreq) {
  for (int i = 0; i < quantity; i++) {
    tone(BUZZER_PIN, toneFreq);
    delay(duration_ms);
    noTone(BUZZER_PIN);
    delay(pause_ms);
  }
}

void stopBuzzer() {
  currentBuzzerPattern = BUZZER_OFF;
  buzzerStep = 0;
  buzzerState = false;
  noTone(BUZZER_PIN);
}

// Non-blocking update (call inside loop)
void updateBuzzerAlarm() {
  unsigned long now = millis();

  if (now - buzzerDemoStart >= BUZZER_DEMO_DURATION && buzzerDemo) {
    // End demo after 5 seconds
    buzzerDemo = false;
    currentBuzzerPattern = BUZZER_OFF;
    buzzerStep = 0;
    noTone(BUZZER_PIN);
    return;
  }

  switch (currentBuzzerPattern) {
    case BUZZER_OFF:
      noTone(BUZZER_PIN);
      break;

    case BUZZER_SINGLE:
      if (buzzerStep == 0) {
        tone(BUZZER_PIN, 1000);
        buzzerLast = now;
        buzzerStep = 1;
      } else if (now - buzzerLast >= 120) {
        noTone(BUZZER_PIN);
        currentBuzzerPattern = BUZZER_OFF;
        buzzerStep = 0;
      }
      break;

    case BUZZER_CHIRP: {
      int chirpFreqs[3] = {800, 1200, 800};
      int duration = 120;
      if (buzzerStep < 3) {
        if (now - buzzerLast >= duration + 50 || buzzerStep == 0) {
          tone(BUZZER_PIN, chirpFreqs[buzzerStep]);
          buzzerLast = now;
          buzzerStep++;
        }
      } else if (now - buzzerLast >= duration) {
        noTone(BUZZER_PIN);
        currentBuzzerPattern = BUZZER_OFF;
        buzzerStep = 0;
      }
      break;
    }

    case BUZZER_LOOP:
      if (now - buzzerLast >= BUZZER_INTERVAL) {
        buzzerLast = now;
        buzzerState = !buzzerState;
        if (buzzerState) tone(BUZZER_PIN, 1000);
        else noTone(BUZZER_PIN);
      }
      break;

    case BUZZER_SOS: {
      unsigned int sosDurations[10] = {0,150,150,150,450,450,450,150,150,150};
      if (buzzerStep < 10) {
        if (now - buzzerLast >= sosDurations[buzzerStep] + 150) {
          tone(BUZZER_PIN, 900, sosDurations[buzzerStep]);
          buzzerLast = now;
          buzzerStep++;
        }
      } else if (now - buzzerLast >= 150) {
        currentBuzzerPattern = BUZZER_OFF;
        buzzerStep = 0;
      }
      break;
    }
  }
}

// === SWITCH HANDLER ========================================
void handleSwitchDetection() {
  bool switchState = digitalRead(SWITCH_PIN);
  bool limitState = digitalRead(LIMIT_PIN);

  if (switchState != switch_forward) {
    Serial.print("Switch changed to: ");
    Serial.println(switchState == HIGH ? "FORWARD" : "REVERSE");
    switch_forward = switchState;
    stateChanged = true;

    if (switchState == HIGH) {
      // Switch turned ON: always claim active and play active buzzer + LED
      Serial.println("⚡ Switch ON — claiming this box as Active.");
      currentRGBMode = activeRGBSetting;
      applyRGBMode();
      triggerBuzzerPattern(activeBuzzerSetting);
      // Broadcast active status and indicate this originated from the switch
      setActiveBox(BOX_NAME);
    } else if (switchState == LOW && active_box != BOX_NAME) {
      // Switch turned OFF
      Serial.println("⚡ Switch OFF — this box is now inactive.");
      currentRGBMode = inactiveRGBSetting;
      applyRGBMode();
      triggerBuzzerPattern(inactiveBuzzerSetting);
      // Active Box has already been changed
    }
    // Switch turned OFF: if we were still active (i.e. this change was instigated by the local box)
    // we release the claim without running the inactive buzzer (local switch shouldn't cause the inactive buzzer)
    else if (switchState == LOW && active_box == BOX_NAME) {
      Serial.println("⚡ Switch OFF — releasing this box as Active (no buzzer).");
      currentRGBMode = inactiveRGBSetting;
      applyRGBMode();
      setActiveBox("NONE");
    } 
  }

  if (limitState != limit_pressed) {
    Serial.print("Limit changed to: ");
    Serial.println(limitState == LOW ? "RELEASED" : "PRESSED");
    limit_pressed = limitState;
    stateChanged = true;
  }

  if (stateChanged) {
    modifyMotorState(switchState, limitState);
    stateChanged = false;
  }
}

// === MOTOR BEHAVIOR ===============================================
void modifyMotorState(bool switchState, bool limitState) {
  Serial.println("Modifying motor state...");
  
  // Determine desired motor state
  if (switchState == HIGH && active_box != BOX_NAME) {
    // Forward direction — limit switch ignored
    Serial.println("Forward");
    motorDirection = 1;
    motorShouldRun = true;
    lastMotorPWMUpdate = millis();
    motorPWMEnabled = false;  // Start with OFF phase of PWM
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (limitState == LOW) {
    // Reverse direction
    Serial.println("Reverse");
    motorDirection = -1;
    motorShouldRun = true;
    lastMotorPWMUpdate = millis();
    motorPWMEnabled = false;  // Start with OFF phase of PWM
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    // Stop motor
    Serial.println("Stop");
    motorShouldRun = false;
    motorDirection = 0;
  }
}

// === MOTOR PWM UPDATE (Dynamic PWM based on motorSpeed) ===========
void updateMotorPWM() {
  unsigned long now = millis();
  
  // Calculate dynamic PWM timing based on motorSpeed
  // 100ms cycle: onTime = (motorSpeed / 100) * 100, offTime = 100 - onTime
  unsigned long onTime = (unsigned long)((float)motorSpeed / 100.0f * MOTOR_PWM_CYCLE_TIME);
  unsigned long offTime = MOTOR_PWM_CYCLE_TIME - onTime;
  
  // If motor shouldn't run, turn it off immediately
  if (!motorShouldRun) { 
    digitalWrite(EN1, LOW); // Disable motor
    motorPWMEnabled = false;
    return;
  }
  
  // Motor should run - apply soft PWM
  if (motorPWMEnabled) {
    // Currently ON - check if we should turn OFF
    if (now - lastMotorPWMUpdate >= onTime) {
      // Turn motor OFF
      //Serial.println("Motor OFF phase");
      digitalWrite(EN1, LOW);
      motorPWMEnabled = false;
      lastMotorPWMUpdate = now;
    }
  } else {
    // Currently OFF - check if we should turn ON
    if (now - lastMotorPWMUpdate >= offTime) {
      //Serial.println("Motor ON phase");
      // Turn motor ON in the desired direction
      if (motorDirection == 1) {
        //Serial.println("Motor ON Forward phase");
        // Forward
        digitalWrite(EN1, HIGH);
      } else if (motorDirection == -1) {
        //Serial.println("Motor ON Reverse phase");
        // Reverse
        digitalWrite(EN1, HIGH);
      }
      motorPWMEnabled = true;
      lastMotorPWMUpdate = now;
    }
  }
}


// ==================================================================
// === ACTIVE BOX SETTER ============================================
// ==================================================================
void setActiveBox(String box) {
  active_box = box;
}

/*
  Since ActiveBox is READ_WRITE variable, onActiveBoxChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onActiveBoxChange()  {
  Serial.print("Active Box Changed to: ");
  Serial.println(active_box);
  // Set stateChanged to true. This causes modifyMotorState() to run on the next loop even with not changes to 
  // switch positions which will trigger the motor to run based on the active_box variable and the current switch positions
  stateChanged = true; 
}