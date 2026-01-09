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
int inactiveRGBSetting = RGB_OFF;
int activeBuzzerSetting = BUZZER_SOS;
int inactiveBuzzerSetting = BUZZER_OFF;
int buzzer_volume_percentage = DEFAULT_BUZZER_VOLUME_PERCENTAGE; // % Setting
int rgb_brightness_percentage = DEFAULT_RGB_BRIGHTNESS_PERCENTAGE; // % Setting

// Track whether this physical box is currently considered "active"
bool box_active = false; // exported via header `extern bool box_active`

// ------------------------------------------------------------------
// Buzzer state
// ------------------------------------------------------------------
int requestedBuzzerPattern = BUZZER_OFF; // updated by menu immediately
int currentBuzzerPattern = BUZZER_SOS;    // only updated on "confirm"
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

// Load persisted settings (call during setup after prefs.begin())
static void loadPersistentSettings() {
  // read stored values or keep current defaults
  currentRGBMode = (RGBMode)prefs.getInt("rgb_mode", currentRGBMode);
  requestedBuzzerPattern = prefs.getInt("buzzer_requested", requestedBuzzerPattern);
  currentBuzzerPattern = prefs.getInt("buzzer_active", currentBuzzerPattern);
  // Active/Inactive presets
  activeRGBSetting = (RGBMode)prefs.getInt("active_rgb", activeRGBSetting);
  inactiveRGBSetting = (RGBMode)prefs.getInt("inactive_rgb", inactiveRGBSetting);
  rgb_brightness_percentage = prefs.getInt("rgb_brightness", rgb_brightness_percentage);
  activeBuzzerSetting = prefs.getInt("active_buzzer", activeBuzzerSetting);
  inactiveBuzzerSetting = prefs.getInt("inactive_buzzer", inactiveBuzzerSetting);
  buzzer_volume_percentage = prefs.getInt("buzzer_vol", buzzer_volume_percentage);
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

  // Set the Board LED as outputs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

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
  
    // Run motor control every 50 ms (non-blocking)
    if (millis() - lastMotorUpdate >= MOTOR_UPDATE_INTERVAL) {
      lastMotorUpdate = millis();
      handleMotorControl();
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
  { "Buzzer Volume",        showBuzzerVolume,        adjustBuzzerVolume,        confirmBuzzerVolume }
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
void confirmActiveBuzzerSetting() { showActiveBuzzerSetting(); }

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
void confirmInactiveBuzzerSetting() { showInactiveBuzzerSetting(); }

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

void confirmBuzzerPattern() {
  // Called when long press confirms selection
  currentBuzzerPattern = requestedBuzzerPattern;
  buzzerStep = 0;
  buzzerState = false;
  buzzerLast = millis();
  noTone(BUZZER_PIN);
  prefs.putInt("buzzer_active", currentBuzzerPattern);
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
void handleMotorControl() {
  bool switchState = digitalRead(SWITCH_PIN);
  bool limitState = digitalRead(LIMIT_PIN);


  if (switchState != switch_forward) {
    Serial.print("Switch changed to: ");
    Serial.println(switchState == HIGH ? "FORWARD" : "REVERSE");
    switch_forward = switchState;
    stateChanged = true;
    // If the physical switch was turned ON (forward), declare this box active
    if (switchState == HIGH && box_active == false) {
      Serial.println("⚡ Switch ON — claiming this box as Active.");
      setActiveBox(BOX_NAME);
    }
  }

  if (limitState != limit_pressed) {
    Serial.print("Limit changed to: ");
    Serial.println(limitState == LOW ? "PRESSED" : "RELEASED");
    limit_pressed = limitState;
    stateChanged = true;
  }

  if (stateChanged) {
    modifyMotorState(switchState, limitState);
  }
}

// ==================================================================
// === MOTOR BEHAVIOR ===============================================
// ==================================================================
void modifyMotorState(bool switchState, bool limitState) {
  if (switchState == HIGH && box_active == false) {
    motor_forward = true;
    // Forward direction — button ignored
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(EN1, (motor_enabled ? 255 : 0));

  } else {
    motor_forward = false;
    // Reverse direction — check button
    if (limitState == HIGH) {
      // Stop motor in reverse
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      analogWrite(EN1, (motor_enabled ? 255 : 0));
    } else {
      // Reverse normally
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      analogWrite(EN1, (motor_enabled ? 255 : 0));
    }
  }
}


// ==================================================================
// === BOARD LED CONTROL ============================================
// ==================================================================
void adjustBoardLED() {
    analogWrite(LED_RED, 255-255*led_brightness_percentage/100);
    analogWrite(LED_GREEN, 255-255*led_brightness_percentage/100);
    analogWrite(LED_BLUE, 255-255*led_brightness_percentage/100);
}

// ==================================================================
// === ACTIVE BOX SETTER ============================================
// ==================================================================
void setActiveBox(String box) {
  active_box = box;
  onActiveBoxChange();
}

/*
  Since ActiveBox is READ_WRITE variable, onActiveBoxChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onActiveBoxChange()  {
  Serial.print("Active Box Changed to: ");
  Serial.println(active_box);

  // Add your code here to act upon ActiveBox change
  if (active_box == BOX_NAME && box_active == false) {
    // Apply "active" presets
    currentRGBMode = activeRGBSetting;
    applyRGBMode();
    currentBuzzerPattern = activeBuzzerSetting;
    buzzerStep = 0;
    buzzerLast = millis();
  } else if (active_box != BOX_NAME && box_active == true) {
    // Apply "inactive" presets
    currentRGBMode = inactiveRGBSetting;
    applyRGBMode();
    currentBuzzerPattern = inactiveBuzzerSetting;
    buzzerStep = 0;
    buzzerLast = millis();

  }
  
  stateChanged = true;
  box_active = (active_box == BOX_NAME);
}