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
int activeBuzzerSetting = BUZZER_OFF;
int inactiveBuzzerSetting = BUZZER_OFF;
int buzzer_volume_percentage = DEFAULT_BUZZER_VOLUME_PERCENTAGE; // % Setting
int rgb_brightness_percentage = DEFAULT_RGB_BRIGHTNESS_PERCENTAGE; // % Setting
int motor_speed_percent = DEFAULT_MOTOR_SPEED_PERCENTAGE; // % Setting (0-100)

// ------------------------------------------------------------------
// Buzzer state
// ------------------------------------------------------------------
int currentBuzzerPattern = BUZZER_OFF;    // active buzzer pattern playback state
bool buzzerState = false;           // on/off state for looping patterns
unsigned long buzzerLast = 0;       // last toggle time
unsigned int buzzerStep = 0;        // step in sequence

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
  bool motor_forward = true;
  bool motor_enabled = true;
  bool stateChanged = false;
  
  // Soft-start state tracking
  unsigned long motorStartTime = 0;
  bool motorIsStarting = false;
  int lastMotorDirection = 0; // 1=forward, -1=reverse, 0=stopped
  
  // Software PWM state tracking
  unsigned long lastPWMUpdate = 0;
  unsigned long pwmCycleStart = 0;
  bool pwmEnableState = false;
  bool motorShouldRunPWM = false; // Should the motor run (direction determined by IN1/IN2)
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

void setBuzzerVolume(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  buzzer_volume_percentage = percent;
  prefs.putInt("buzzer_vol", buzzer_volume_percentage);
}

// Helper function to calculate effective speed percentage with soft-start
// Returns effective speed percentage (0-100) for software PWM
// NO minimum constraints - allows truly slow speeds with minimal on-time
// Applies scaling to make lower speeds even slower (10% becomes ~1-2% actual)
static int calculateEffectiveSpeed(int speedPercent, bool isStarting) {
  if (speedPercent <= 0) return 0;
  
  // During soft-start phase, use 100% (full power) to overcome static friction
  if (isStarting) {
    return 100;  // Full power during startup
  }
  
  if (speedPercent > 100) return 100;
  
  // Apply aggressive scaling to make lower speeds MUCH slower
  // This makes 10% speed setting result in barely any motor movement
  // Speeds below 20% are divided by 10: 10% -> 1%, 5% -> 0.5%, 1% -> 0.1%
  if (speedPercent <= 20) {
    return speedPercent / 10;  // 10% -> 1%, 5% -> 0.5%, 1% -> 0.1%
  }
  
  // For speeds above 20%, use linear mapping (20% -> 2%, 100% -> 100%)
  // Creates smooth transition from very slow to normal speeds
  return 2 + ((speedPercent - 20) * 98) / 80;  // 20% -> 2%, 100% -> 100%
}

void setMotorSpeed(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  motor_speed_percent = percent;
  prefs.putInt("motor_speed", motor_speed_percent);
  // Reapply motor state immediately with new speed (read current switch/limit state)
  stateChanged = true;
}

// Load persisted settings (call during setup after prefs.begin())
static void loadPersistentSettings() {
  // read stored values or keep current defaults
  currentRGBMode = (RGBMode)prefs.getInt("rgb_mode", currentRGBMode);
  // Active/Inactive presets
  activeRGBSetting = (RGBMode)prefs.getInt("active_rgb", activeRGBSetting);
  inactiveRGBSetting = (RGBMode)prefs.getInt("inactive_rgb", inactiveRGBSetting);
  rgb_brightness_percentage = prefs.getInt("rgb_brightness", rgb_brightness_percentage);
  activeBuzzerSetting = prefs.getInt("active_buzzer", activeBuzzerSetting);
  inactiveBuzzerSetting = prefs.getInt("inactive_buzzer", inactiveBuzzerSetting);
  buzzer_volume_percentage = prefs.getInt("buzzer_vol", buzzer_volume_percentage);
  // Motor speed
  motor_speed_percent = prefs.getInt("motor_speed", motor_speed_percent);
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

  // Inputs with internal pull-ups
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  // Set starting state
  switch_forward = digitalRead(SWITCH_PIN);
  limit_pressed = digitalRead(LIMIT_PIN); 
  modifyMotorState(switch_forward, limit_pressed);

  // Settings button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Reflect starting state
  onActiveBoxChange();
  Serial.println("System Initialized.");
  showMenu();
}

// ==================================================================
// === MAIN LOOP ====================================================
// ==================================================================
void loop() {
    ArduinoCloud.update();
    handleSettingsButton();   // independent from motor
  
    // Update software PWM frequently (5ms intervals) for smooth motor control
    updateMotorPWM();
  
    // Run motor control every 50 ms (non-blocking)
    if (millis() - lastMotorUpdate >= MOTOR_UPDATE_INTERVAL) {
      lastMotorUpdate = millis();
      handleSwitchDetection();
    }

    updateAnimations();   // RGB effects (rainbow, pulse, etc)
    updateBuzzerAlarm();  // Looping buzzer patterns
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
  { "Buzzer Volume",        showBuzzerVolume,        adjustBuzzerVolume,        confirmBuzzerVolume },
  { "Motor Speed",          showMotorSpeed,          adjustMotorSpeed,          confirmMotorSpeed }
};

int totalMenus = sizeof(menuItems) / sizeof(MenuItem);

// =============================================================
//     MAIN MENU HANDLER (Button-driven navigation)
// =============================================================

void handleSerialMenu() {
  unsigned long now = millis();

  // Timeout → return to main screen
  if ((now - lastInteractionTime) > MENU_TIMEOUT_MS && (menuIndex != 0 || inSubMenu)) {
    Serial.println("\n⏱️ Menu timed out — returning to main screen.\n");
    menuIndex = 0;
    inSubMenu = false;
    showMenu();
  }

  // Short press: next menu item OR adjust submenu value
  if (shortPressCount > lastShortPressCount) {
    lastShortPressCount = shortPressCount;
    lastInteractionTime = now;

    if (!inSubMenu) {
      menuIndex = (menuIndex + 1) % totalMenus;
      showMenu();
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
    } else {
      inSubMenu = false;
      menuItems[menuIndex].onConfirm();
      Serial.println("✅ Saved and returned to main menu.");
      showMenu();
    }
  }
}

// =============================================================
//    MENU DISPLAY
// =============================================================
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
// === INDIVIDUAL MENU HANDLERS ======================================
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
}
void adjustActiveRGB() {
  activeRGBSetting = (activeRGBSetting + 1) % RGB_MODE_COUNT;
  setActiveRGBSetting(activeRGBSetting);
  showActiveRGB();
}
void confirmActiveRGB() { showActiveRGB(); }

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
}
void adjustInactiveRGB() {
  inactiveRGBSetting = (inactiveRGBSetting + 1) % RGB_MODE_COUNT;
  setInactiveRGBSetting(inactiveRGBSetting);
  showInactiveRGB();
}
void confirmInactiveRGB() { showInactiveRGB(); }

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
}
void confirmRGBBrightness() {
  showRGBBrightness();
}
// ==================================================================

// ---------------- ACTIVE/INACTIVE BUZZER PRESETS ----------------
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
  showActiveBuzzerSetting();
}
// Play the given buzzer pattern once (non-blocking)
static void triggerBuzzerPattern(int pattern) {
  currentBuzzerPattern = pattern;
  buzzerStep = 0;
  buzzerState = false;
  buzzerLast = millis();
  noTone(BUZZER_PIN);
}

void confirmActiveBuzzerSetting() {
  // user saved a new active buzzer selection — play it once as confirmation
  triggerBuzzerPattern(activeBuzzerSetting);
  showActiveBuzzerSetting();
}

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
  showInactiveBuzzerSetting();
}
void confirmInactiveBuzzerSetting() {
  // user saved a new inactive buzzer selection — play it once as confirmation
  triggerBuzzerPattern(inactiveBuzzerSetting);
  showInactiveBuzzerSetting();
}

// ---------------- BUZZER VOLUME ----------------
void showBuzzerVolume() {
  Serial.print("Buzzer Volume: ");
  Serial.print(buzzer_volume_percentage);
  Serial.println("%");
}
void adjustBuzzerVolume() {
  int next = buzzer_volume_percentage + 10;
  if (next > 100) next = 0;
  setBuzzerVolume(next);
  showBuzzerVolume();
}
void confirmBuzzerVolume() { showBuzzerVolume(); }

// ---------------- MOTOR SPEED ----------------
void showMotorSpeed() {
  Serial.print("Motor Speed: ");
  Serial.print(motor_speed_percent);
  Serial.println("%");
}
void adjustMotorSpeed() {
  int next = motor_speed_percent + 10;
  if (next > 100) next = 0;
  setMotorSpeed(next);
  showMotorSpeed();
}
void confirmMotorSpeed() { showMotorSpeed(); }

// ==================================================================

// === RGB LED CONTROL ===============================================
// ==================================================================
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



// ==================================================================
// === BUZZER CONTROL ===============================================
// ==================================================================
void stopBuzzer() {
  currentBuzzerPattern = BUZZER_OFF;
  buzzerStep = 0;
  buzzerState = false;
  noTone(BUZZER_PIN);
}

// Non-blocking update (call inside loop)
void updateBuzzerAlarm() {
  unsigned long now = millis();

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
      unsigned int sosDurations[10] = {0,150,150,150,400,400,400,150,150,150};
      if (buzzerStep < 10) {
        if (now - buzzerLast >= sosDurations[buzzerStep] + 150) {
          tone(BUZZER_PIN, 800, sosDurations[buzzerStep]);
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



// ==================================================================
// === MOTOR CONTROL HANDLER ========================================
// ==================================================================
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
    }
    // Switch turned OFF: if we were active, release claim without running the
    // inactive buzzer (local switch shouldn't cause the inactive buzzer)
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

// ==================================================================
// === MOTOR BEHAVIOR ===============================================
// ==================================================================
void modifyMotorState(bool switchState, bool limitState) {
  Serial.println("Modifying motor state...");
  
  // Determine current motor direction (1=forward, -1=reverse, 0=stopped)
  int currentDirection = 0;
  bool motorShouldRun = false;
  
  if (switchState == HIGH && active_box != BOX_NAME) {
    motor_forward = true;
    motorShouldRun = true;
    currentDirection = 1;
    // Forward direction — limit switch ignored
    Serial.println("Motor moving FORWARD.");
    Serial.print("Motor Pins: ");
    Serial.println("EN1=" + String(EN1) + ", IN1=" + String(IN1) + ", IN2=" + String(IN2));
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (limitState == LOW) {
    motor_forward = false;
    motorShouldRun = true;
    currentDirection = -1;
    // Reverse direction
    Serial.println("Reverse Motor.");
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    motor_forward = false;
    motorShouldRun = false;
    currentDirection = 0;
    // Stop motor (limit pressed in reverse, or switch OFF)
    Serial.println("Stop Motor (limit pressed or switch OFF).");
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    // Cancel soft-start when stopping
    motorIsStarting = false;
    motorStartTime = 0;
  }
  
  // Detect if motor direction changed (triggers soft-start)
  if (motorShouldRun && currentDirection != 0 && currentDirection != lastMotorDirection) {
    // Motor started or changed direction - begin soft-start at full power
    motorIsStarting = true;
    motorStartTime = millis();
    Serial.println("Motor start detected - applying FULL POWER to overcome static friction.");
  }
  
  lastMotorDirection = currentDirection;
  
  // Check if soft-start phase has completed
  if (motorIsStarting && (millis() - motorStartTime >= MOTOR_SOFT_START_DURATION)) {
    motorIsStarting = false;
    Serial.println("Soft-start complete - motor moving, ramping down to target speed.");
  }
  
  // Set flag for software PWM (don't control enable pin here - that's done by updateMotorPWM)
  motorShouldRunPWM = (motor_enabled && motorShouldRun);
  
  // Reset PWM cycle when motor state changes
  pwmCycleStart = millis();
  pwmEnableState = false;
  
  // Calculate effective speed for logging
  int effectiveSpeed = calculateEffectiveSpeed(motor_speed_percent, motorIsStarting);
  Serial.print("Motor speed: ");
  Serial.print(effectiveSpeed);
  Serial.println("% (software PWM)");
  if (motorIsStarting) {
    Serial.println("  [FULL POWER startup phase - will ramp down after " + String(MOTOR_SOFT_START_DURATION) + "ms]");
  }
  
  // If motor should stop, immediately disable enable pin
  if (!motorShouldRunPWM) {
    digitalWrite(EN1, LOW);
  }
}

// ==================================================================
// === SOFTWARE PWM UPDATE ==========================================
// ==================================================================
// This function implements software PWM by toggling the enable pin
// It should be called frequently (every 5-10ms) from the main loop
void updateMotorPWM() {
  unsigned long now = millis();
  
  // Only update at the specified interval
  if (now - lastPWMUpdate < MOTOR_PWM_UPDATE_INTERVAL) {
    return;
  }
  lastPWMUpdate = now;
  
  // If motor shouldn't run, keep enable pin LOW
  if (!motorShouldRunPWM || !motor_enabled) {
    digitalWrite(EN1, LOW);
    pwmEnableState = false;
    return;
  }
  
  // Calculate effective speed percentage (with soft-start consideration)
  int effectiveSpeed = calculateEffectiveSpeed(motor_speed_percent, motorIsStarting);
  
  // If speed is 0 or less, disable motor
  if (effectiveSpeed <= 0) {
    digitalWrite(EN1, LOW);
    pwmEnableState = false;
    return;
  }
  
  // If speed is 100%, always keep enable pin HIGH
  if (effectiveSpeed >= 100) {
    digitalWrite(EN1, HIGH);
    pwmEnableState = true;
    return;
  }
  
  // Calculate ON and OFF times within the PWM period
  // For very slow speeds: tiny on-time, extremely long off-time
  // With 5000ms period:
  //   At 10%: onTime = 500ms, offTime = 4500ms (motor barely on)
  //   At 5%: onTime = 250ms, offTime = 4750ms
  //   At 1%: onTime = 50ms, offTime = 4950ms
  //   At 0.1%: onTime = 5ms, offTime = 4995ms (almost nothing)
  // NO minimum constraints - if calculation results in 0ms, motor stays off
  unsigned long onTime = (MOTOR_PWM_PERIOD * effectiveSpeed) / 100;
  
  unsigned long offTime = MOTOR_PWM_PERIOD - onTime;
  
  // Safety check: ensure valid times
  if (onTime > MOTOR_PWM_PERIOD) {
    onTime = MOTOR_PWM_PERIOD;
    offTime = 0;
  }
  
  // Calculate position within current PWM cycle (0 to MOTOR_PWM_PERIOD-1)
  unsigned long cyclePosition = (now - pwmCycleStart) % MOTOR_PWM_PERIOD;
  
  // Determine if enable pin should be HIGH or LOW based on position in cycle
  // For very slow speeds: short pulse at start of cycle, then long pause
  // cyclePosition 0 to onTime-1: HIGH (motor ON)
  // cyclePosition onTime to MOTOR_PWM_PERIOD-1: LOW (motor OFF)
  bool shouldBeHigh = (cyclePosition < onTime);
  
  // Only toggle if state needs to change (avoids unnecessary digitalWrite calls)
  if (shouldBeHigh != pwmEnableState) {
    pwmEnableState = shouldBeHigh;
    digitalWrite(EN1, shouldBeHigh ? HIGH : LOW);
    
    // Debug output for slow speeds to verify PWM is working
    if (effectiveSpeed <= 15) {
      static unsigned long lastDebugTime = 0;
      if (now - lastDebugTime > 2000) {  // Print debug every 2 seconds
        lastDebugTime = now;
        Serial.print("PWM: speed=");
        Serial.print(effectiveSpeed);
        Serial.print("%, onTime=");
        Serial.print(onTime);
        Serial.print("ms, offTime=");
        Serial.print(offTime);
        Serial.print("ms, period=");
        Serial.print(MOTOR_PWM_PERIOD);
        Serial.print("ms, duty=");
        Serial.print((onTime * 100) / MOTOR_PWM_PERIOD);
        Serial.println("%");
      }
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

  if (active_box == BOX_NAME) {
    currentRGBMode = activeRGBSetting;
    applyRGBMode();
  } else { // The active_box is set to the other
    // Should only be called when active_box is adjusted through ArduinoCloud
    bool switchState = digitalRead(SWITCH_PIN);
    if (switchState == HIGH) { // Another box claimed active while our switch is ON
      triggerBuzzerPattern(inactiveBuzzerSetting);
      stateChanged = true; // This causes modifyMotorState() to run on the next loop
    }
    currentRGBMode = inactiveRGBSetting;
    applyRGBMode();
  }
}