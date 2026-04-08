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
// OTA helpers
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <Update.h>

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

// Active/Inactive presets (persisted)
int activeRGBSetting = RGB_RAINBOW;
int inactiveRGBSetting = RGB_SOLID_RED;
int activeBuzzerSetting = BUZZER_CHIRP;
int inactiveBuzzerSetting = BUZZER_SINGLE;
int rgb_brightness_percentage = DEFAULT_RGB_BRIGHTNESS_PERCENTAGE; // % Setting
int motorSpeed = 100; // 0-100% - controls PWM duty cycle
bool soloMode = false; // When true, this box ignores shared activation and runs locally

// ------------------------------------------------------------------
// Buzzer state
// ------------------------------------------------------------------
int currentBuzzerPattern = BUZZER_OFF;    // active buzzer pattern playback state
bool buzzerState = false;           // true while a buzzer tone is currently playing
unsigned long buzzerLast = 0;       // last playback state change
unsigned int buzzerStep = 0;        // step in sequence
bool buzzerDemo = false;            // true when playing a temporary demo pattern
unsigned long buzzerDemoStart = 0;  // start time of demo pattern
constexpr unsigned long BUZZER_DEMO_DURATION = 5000; // ms

// ------------------------------------------------------------------
// File-local internal state (kept private to this .cpp)
// ------------------------------------------------------------------
namespace {
  template <typename T, size_t N>
  constexpr size_t arrayCount(const T (&)[N]) {
    return N;
  }

  struct RGBColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  using RGBPatternRenderer = RGBColor (*)(unsigned long now);

  struct RGBPatternDefinition {
    const char* name;
    RGBPatternRenderer render;
  };

  constexpr RGBColor makeRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
    return { red, green, blue };
  }

  constexpr RGBPatternDefinition makeRGBPattern(const char* name, RGBPatternRenderer render) {
    return { name, render };
  }

  struct BuzzerToneStep {
    uint16_t frequency;
    uint16_t durationMs;
    uint16_t pauseAfterMs;
  };

  struct BuzzerPatternDefinition {
    const char* name;
    const BuzzerToneStep* steps;
    size_t stepCount;
    bool loops;
  };

  enum BuzzerPlaybackPhase : uint8_t {
    BUZZER_PHASE_START_NOTE,
    BUZZER_PHASE_PLAYING_NOTE,
    BUZZER_PHASE_NOTE_GAP
  };

  // Add new RGB patterns in one place: extend the enum in the header,
  // then add a renderer with the shape `RGBColor renderX(unsigned long now)`.
  // Example:
  //   RGBColor renderSunset(unsigned long now) { return makeRGBColor(...); }
  //   makeRGBPattern("SUNSET", renderSunset),
  RGBColor renderOff(unsigned long /*now*/) {
    return makeRGBColor(0, 0, 0);
  }

  RGBColor renderWhite(unsigned long /*now*/) {
    return makeRGBColor(255, 255, 255);
  }

  RGBColor renderSolidRed(unsigned long /*now*/) {
    return makeRGBColor(255, 0, 0);
  }

  RGBColor renderSolidGreen(unsigned long /*now*/) {
    return makeRGBColor(0, 255, 0);
  }

  RGBColor renderSolidBlue(unsigned long /*now*/) {
    return makeRGBColor(0, 0, 255);
  }

  RGBColor renderRainbow(unsigned long now) {
    const float phase = (now / 18.0f) * 0.05f;
    const uint8_t r = static_cast<uint8_t>((sin(phase) * 127.0f) + 128.0f);
    const uint8_t g = static_cast<uint8_t>((sin(phase + (2.0f * PI / 3.0f)) * 127.0f) + 128.0f);
    const uint8_t b = static_cast<uint8_t>((sin(phase + (4.0f * PI / 3.0f)) * 127.0f) + 128.0f);
    return makeRGBColor(r, g, b);
  }

  RGBColor renderBreathing(unsigned long now) {
    const float cycle = (now % 4000UL) / 4000.0f;
    const float wave = (sin((cycle * TWO_PI) - (PI / 2.0f)) + 1.0f) * 0.5f;
    const uint8_t value = static_cast<uint8_t>(5.0f + (wave * 245.0f));
    return makeRGBColor(value, value, value);
  }

  RGBColor renderPolice(unsigned long now) {
    constexpr unsigned long POLICE_FLASH_INTERVAL_MS = 120;
    const bool showRed = ((now / POLICE_FLASH_INTERVAL_MS) % 2UL) == 0;
    return showRed ? makeRGBColor(255, 0, 0) : makeRGBColor(0, 0, 255);
  }

  RGBColor renderSunset(unsigned long now) {
    const float cycle = (now % 6000UL) / 6000.0f;
    const float wave = (sin(cycle * TWO_PI) + 1.0f) * 0.5f;
    const uint8_t red = static_cast<uint8_t>(180.0f + (wave * 75.0f));
    const uint8_t green = static_cast<uint8_t>(40.0f + (wave * 70.0f));
    const uint8_t blue = static_cast<uint8_t>(10.0f + ((1.0f - wave) * 40.0f));
    return makeRGBColor(red, green, blue);
  }

  RGBColor renderAurora(unsigned long now) {
    const float phase = now / 900.0f;
    const uint8_t red = static_cast<uint8_t>(25.0f + ((sin(phase * 0.9f) + 1.0f) * 45.0f));
    const uint8_t green = static_cast<uint8_t>(80.0f + ((sin(phase * 1.3f) + 1.0f) * 85.0f));
    const uint8_t blue = static_cast<uint8_t>(90.0f + ((sin((phase * 1.1f) + 1.7f) + 1.0f) * 75.0f));
    return makeRGBColor(red, green, blue);
  }

  RGBColor renderFireFlicker(unsigned long now) {
    const float fast = (sin(now / 70.0f) + 1.0f) * 0.5f;
    const float slow = (sin((now / 190.0f) + 1.3f) + 1.0f) * 0.5f;
    const uint8_t red = static_cast<uint8_t>(180.0f + (fast * 75.0f));
    const uint8_t green = static_cast<uint8_t>(35.0f + (slow * 90.0f));
    const uint8_t blue = static_cast<uint8_t>(fast * 25.0f);
    return makeRGBColor(red, green, blue);
  }

  RGBColor renderCyberPulse(unsigned long now) {
    const float cycle = (now % 1800UL) / 1800.0f;
    const float pulse = pow((sin(cycle * TWO_PI) + 1.0f) * 0.5f, 2.0f);
    const uint8_t red = static_cast<uint8_t>(10.0f + (pulse * 35.0f));
    const uint8_t green = static_cast<uint8_t>(90.0f + (pulse * 80.0f));
    const uint8_t blue = static_cast<uint8_t>(120.0f + (pulse * 100.0f));
    return makeRGBColor(red, green, blue);
  }

  RGBColor renderParty(unsigned long now) {
    const unsigned long frame = (now / 90UL) % 6UL;
    switch (frame) {
      case 0: return makeRGBColor(255, 0, 120);
      case 1: return makeRGBColor(0, 255, 80);
      case 2: return makeRGBColor(0, 160, 255);
      case 3: return makeRGBColor(255, 180, 0);
      case 4: return makeRGBColor(180, 0, 255);
      default: return makeRGBColor(255, 30, 30);
    }
  }

  RGBColor renderLaser(unsigned long now) {
    const float sweep = (sin(now / 120.0f) + 1.0f) * 0.5f;
    const float sparkle = (sin((now / 40.0f) + 1.2f) + 1.0f) * 0.5f;
    const uint8_t red = static_cast<uint8_t>(180.0f + (sweep * 75.0f));
    const uint8_t green = static_cast<uint8_t>(sparkle * 40.0f);
    const uint8_t blue = static_cast<uint8_t>(sparkle * 20.0f);
    return makeRGBColor(red, green, blue);
  }

  const RGBPatternDefinition RGB_PATTERN_DEFINITIONS[] = {
    makeRGBPattern("OFF", renderOff),
    makeRGBPattern("WHITE", renderWhite),
    makeRGBPattern("RAINBOW", renderRainbow),
    makeRGBPattern("BREATHING", renderBreathing),
    makeRGBPattern("RED", renderSolidRed),
    makeRGBPattern("GREEN", renderSolidGreen),
    makeRGBPattern("BLUE", renderSolidBlue),
    makeRGBPattern("POLICE", renderPolice),
    makeRGBPattern("SUNSET", renderSunset),
    makeRGBPattern("AURORA", renderAurora),
    makeRGBPattern("FIRE FLICKER", renderFireFlicker),
    makeRGBPattern("CYBERPULSE", renderCyberPulse),
    makeRGBPattern("PARTY", renderParty),
    makeRGBPattern("LASER", renderLaser)
  };

  const BuzzerToneStep BUZZER_STEPS_SINGLE[] = {
    { 1000, 120, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_CHIRP[] = {
    { 800, 120, 50 },
    { 1200, 120, 50 },
    { 800, 120, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_LOOP[] = {
    { 1000, BUZZER_INTERVAL, BUZZER_INTERVAL }
  };

  constexpr uint16_t MORSE_UNIT_MS = 120;
  const BuzzerToneStep BUZZER_STEPS_SOS[] = {
    { 900, MORSE_UNIT_MS, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS, MORSE_UNIT_MS * 3 },
    { 900, MORSE_UNIT_MS * 3, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS * 3, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS * 3, MORSE_UNIT_MS * 3 },
    { 900, MORSE_UNIT_MS, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS, MORSE_UNIT_MS },
    { 900, MORSE_UNIT_MS, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_DOUBLE[] = {
    { 1100, 90, 70 },
    { 1400, 110, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_FANFARE[] = {
    { 988, 110, 40 },
    { 988, 110, 40 },
    { 988, 110, 80 },
    { 1319, 320, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_SAD_TROMBONE[] = {
    { 659, 150, 15 },
    { 622, 170, 15 },
    { 554, 220, 20 },
    { 440, 420, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_LEVEL_UP[] = {
    { 523, 90, 30 },
    { 659, 90, 30 },
    { 784, 100, 30 },
    { 1047, 180, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_GAME_OVER[] = {
    { 784, 120, 20 },
    { 659, 130, 20 },
    { 523, 150, 30 },
    { 392, 300, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_COIN[] = {
    { 988, 70, 20 },
    { 1319, 130, 0 }
  };

  const BuzzerToneStep BUZZER_STEPS_POWER_UP[] = {
    { 440, 70, 20 },
    { 554, 70, 20 },
    { 659, 80, 20 },
    { 880, 110, 0 }
  };

  // Add new buzzer patterns in one place: define the step sequence,
  // then add one entry here in the EXACT same order as enum `BuzzerPattern`.
  const BuzzerPatternDefinition BUZZER_PATTERN_DEFINITIONS[] = {
    { "OFF", nullptr, 0, false },
    { "SINGLE", BUZZER_STEPS_SINGLE, arrayCount(BUZZER_STEPS_SINGLE), false },
    { "CHIRP", BUZZER_STEPS_CHIRP, arrayCount(BUZZER_STEPS_CHIRP), false },
    { "LOOP", BUZZER_STEPS_LOOP, arrayCount(BUZZER_STEPS_LOOP), true },
    { "SOS", BUZZER_STEPS_SOS, arrayCount(BUZZER_STEPS_SOS), false },
    { "DOUBLE", BUZZER_STEPS_DOUBLE, arrayCount(BUZZER_STEPS_DOUBLE), false },
    { "FANFARE", BUZZER_STEPS_FANFARE, arrayCount(BUZZER_STEPS_FANFARE), false },
    { "SAD TROMBONE", BUZZER_STEPS_SAD_TROMBONE, arrayCount(BUZZER_STEPS_SAD_TROMBONE), false },
    { "LEVEL UP", BUZZER_STEPS_LEVEL_UP, arrayCount(BUZZER_STEPS_LEVEL_UP), false },
    { "GAME OVER", BUZZER_STEPS_GAME_OVER, arrayCount(BUZZER_STEPS_GAME_OVER), false },
    { "COIN", BUZZER_STEPS_COIN, arrayCount(BUZZER_STEPS_COIN), false },
    { "POWER UP", BUZZER_STEPS_POWER_UP, arrayCount(BUZZER_STEPS_POWER_UP), false },
  };

  static_assert(arrayCount(RGB_PATTERN_DEFINITIONS) == RGB_MODE_COUNT,
                "RGB pattern table must stay in sync with RGBMode");
  static_assert(arrayCount(BUZZER_PATTERN_DEFINITIONS) == BUZZER_PATTERN_COUNT,
                "Buzzer pattern table must stay in sync with BuzzerPattern");

  int clampRGBMode(int mode) {
    return (mode >= 0 && mode < RGB_MODE_COUNT) ? mode : RGB_OFF;
  }

  int clampBuzzerPattern(int pattern) {
    return (pattern >= 0 && pattern < BUZZER_PATTERN_COUNT) ? pattern : BUZZER_OFF;
  }

  const RGBPatternDefinition& getRGBPatternDefinition(int mode) {
    return RGB_PATTERN_DEFINITIONS[clampRGBMode(mode)];
  }

  const BuzzerPatternDefinition& getBuzzerPatternDefinition(int pattern) {
    return BUZZER_PATTERN_DEFINITIONS[clampBuzzerPattern(pattern)];
  }

  const char* getRGBModeName(int mode) {
    return getRGBPatternDefinition(mode).name;
  }

  const char* getBuzzerPatternName(int pattern) {
    return getBuzzerPatternDefinition(pattern).name;
  }

  int menuIndex = 0;
  bool inSubMenu = false;
  BuzzerPlaybackPhase buzzerPlaybackPhase = BUZZER_PHASE_START_NOTE;

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

  constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
  constexpr unsigned long WIFI_CONNECT_RETRY_MS = 250;
}

// Preferences (non-volatile storage) instance
static Preferences prefs;

// ------------------------------------------------------------------
// OTA via GitHub Releases
// ------------------------------------------------------------------
constexpr unsigned long OTA_CHECK_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour
constexpr char CURRENT_FW_VERSION[] = "v1.1.1"; // Bump this for each release
unsigned long lastOTACheck = 0;

#if defined(BOARD_MICHAEL)
  constexpr char OTA_MANIFEST_URL[] = "https://raw.githubusercontent.com/MMarsland/UselessBoxes/main/ota/michael.txt";
#elif defined(BOARD_TREVOR)
  constexpr char OTA_MANIFEST_URL[] = "https://raw.githubusercontent.com/MMarsland/UselessBoxes/main/ota/trevor.txt";
#else
  constexpr char OTA_MANIFEST_URL[] = "";
#endif

void handleOTACheck();
void checkForOTAUpdate();

void connectToPreferredWiFi() {
  Serial.println("Attempting Wi-Fi connection...");
  WiFi.mode(WIFI_STA);

  for (int i = 0; i < SECRET_WIFI_COUNT; ++i) {
    const SecretWiFiCredential& network = SECRET_WIFI_NETWORKS[i];
    const char* ssid = network.ssid;
    const char* pass = network.password;

    Serial.print("Trying Wi-Fi [");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(SECRET_WIFI_COUNT);
    Serial.print("]: ");
    Serial.println(ssid);

    ArduinoIoTPreferredConnection = decltype(ArduinoIoTPreferredConnection)(ssid, pass);

    WiFi.disconnect(true, true);
    delay(100);
    WiFi.begin(ssid, pass);

    unsigned long attemptStarted = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - attemptStarted < WIFI_CONNECT_TIMEOUT_MS) {
      delay(WIFI_CONNECT_RETRY_MS);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected to Wi-Fi: ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.print("Failed to connect to: ");
    Serial.println(ssid);
  }

  Serial.println("No configured Wi-Fi networks connected. Arduino Cloud will keep retrying the last network.");
}

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
  activeRGBSetting = clampRGBMode(mode);
  prefs.putInt("active_rgb", activeRGBSetting);
}

void setInactiveRGBSetting(int mode) {
  inactiveRGBSetting = clampRGBMode(mode);
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
  activeBuzzerSetting = clampBuzzerPattern(pattern);
  prefs.putInt("active_buzzer", activeBuzzerSetting);
}

void setInactiveBuzzerSetting(int pattern) {
  inactiveBuzzerSetting = clampBuzzerPattern(pattern);
  prefs.putInt("inactive_buzzer", inactiveBuzzerSetting);
}

void setMotorSpeed(int speed) {
  if (speed <= 0) speed = 0;
  if (speed > 100) speed = 100;
  motorSpeed = speed;
  prefs.putInt("motor_speed", motorSpeed);
}

void setSoloMode(bool enabled) {
  soloMode = enabled;
  prefs.putBool("solo_mode", soloMode);

  if (soloMode && active_box == BOX_NAME) {
    setActiveBox("NONE");
  }

  updateRGBModeFromBoxState();
  stateChanged = true;
}


// Load persisted settings (call during setup after prefs.begin())
void loadPersistentSettings() {
  // Active/Inactive presets
  activeRGBSetting = clampRGBMode(prefs.getInt("active_rgb", activeRGBSetting));
  inactiveRGBSetting = clampRGBMode(prefs.getInt("inactive_rgb", inactiveRGBSetting));
  rgb_brightness_percentage = prefs.getInt("rgb_brightness", rgb_brightness_percentage);
  activeBuzzerSetting = clampBuzzerPattern(prefs.getInt("active_buzzer", activeBuzzerSetting));
  inactiveBuzzerSetting = clampBuzzerPattern(prefs.getInt("inactive_buzzer", inactiveBuzzerSetting));
  motorSpeed = prefs.getInt("motor_speed", motorSpeed);
  soloMode = prefs.getBool("solo_mode", soloMode);
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
  connectToPreferredWiFi();

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
  Serial.print("Firmware version: ");
  Serial.println(CURRENT_FW_VERSION);
  Serial.print("OTA manifest: ");
  Serial.println(OTA_MANIFEST_URL[0] != '\0' ? OTA_MANIFEST_URL : "(not configured)");
  showMenu();
}

// ==================================================================
// === MAIN LOOP ====================================================
// ==================================================================
void loop() {
    ArduinoCloud.update();
    handleOTACheck();
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
  { "Motor Speed",          showMotorSpeed,          adjustMotorSpeed,          confirmMotorSpeed },
  { "Solo Mode",            showSoloMode,            adjustSoloMode,            confirmSoloMode }
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
  Serial.println(getRGBModeName(activeRGBSetting));
}
void adjustActiveRGB() {
  activeRGBSetting = (activeRGBSetting + 1) % RGB_MODE_COUNT;
  setActiveRGBSetting(activeRGBSetting);
  showActiveRGB();
  beepBuzzer(1, 100, 100);
  currentRGBMode = activeRGBSetting;
  applyRGBMode();
}
void confirmActiveRGB() { 
  showActiveRGB(); 
  updateRGBModeFromBoxState();
}

// ---------------- INACTIVE RGB PRESET ----------------
void showInactiveRGB() {
  Serial.print("Inactive RGB Mode: ");
  Serial.println(getRGBModeName(inactiveRGBSetting));
}
void adjustInactiveRGB() {
  inactiveRGBSetting = (inactiveRGBSetting + 1) % RGB_MODE_COUNT;
  setInactiveRGBSetting(inactiveRGBSetting);
  showInactiveRGB();
  beepBuzzer(1, 100, 100);
  currentRGBMode = inactiveRGBSetting;
  applyRGBMode();
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
  Serial.println(getBuzzerPatternName(activeBuzzerSetting));
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
  Serial.println(getBuzzerPatternName(inactiveBuzzerSetting));
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

// ---------------- SOLO MODE ----------------
void showSoloMode() {
  Serial.print("Solo Mode: ");
  Serial.println(soloMode ? "ON" : "OFF");
}
void adjustSoloMode() {
  setSoloMode(!soloMode);
  showSoloMode();
  beepBuzzer(soloMode ? 2 : 1, 100, 100);
}
void confirmSoloMode() {
  Serial.print("Solo Mode ");
  Serial.println(soloMode ? "enabled." : "disabled.");
  showSoloMode();
}
// ==================================================================


// === RGB LED CONTROL ==============================================
void updateRGBModeFromBoxState() {
  if (soloMode) {
    bool switchState = (digitalRead(SWITCH_PIN) == HIGH);
    currentRGBMode = (switchState || motorShouldRun) ? activeRGBSetting : inactiveRGBSetting;
  } else if (active_box == BOX_NAME) {
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
  const unsigned long now = millis();
  const RGBColor color = getRGBPatternDefinition(currentRGBMode).render(now);
  lastRGBAnimation = now;
  setRGB(color.red, color.green, color.blue);
}

void updateAnimations() {
  const unsigned long now = millis();

  if (now - lastRGBAnimation < RGB_UPDATE_INTERVAL) {
    return;
  }

  lastRGBAnimation = now;
  const RGBColor color = getRGBPatternDefinition(currentRGBMode).render(now);
  setRGB(color.red, color.green, color.blue);
}


// === BUZZER CONTROL ===============================================
void triggerBuzzerPattern(int pattern) {
  currentBuzzerPattern = clampBuzzerPattern(pattern);
  buzzerStep = 0;
  buzzerState = false;
  buzzerPlaybackPhase = BUZZER_PHASE_START_NOTE;
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
  buzzerPlaybackPhase = BUZZER_PHASE_START_NOTE;
  noTone(BUZZER_PIN);
}

// Non-blocking update (call inside loop)
void updateBuzzerAlarm() {
  unsigned long now = millis();

  if (buzzerDemo && (now - buzzerDemoStart >= BUZZER_DEMO_DURATION)) {
    buzzerDemo = false;
    stopBuzzer();
    return;
  }

  const BuzzerPatternDefinition& pattern = getBuzzerPatternDefinition(currentBuzzerPattern);

  if (pattern.stepCount == 0) {
    noTone(BUZZER_PIN);
    return;
  }

  if (buzzerStep >= pattern.stepCount) {
    if (pattern.loops) {
      buzzerStep = 0;
      buzzerPlaybackPhase = BUZZER_PHASE_START_NOTE;
    } else {
      stopBuzzer();
      return;
    }
  }

  const BuzzerToneStep& step = pattern.steps[buzzerStep];

  switch (buzzerPlaybackPhase) {
    case BUZZER_PHASE_START_NOTE:
      if (step.frequency > 0 && step.durationMs > 0) {
        tone(BUZZER_PIN, step.frequency);
        buzzerState = true;
      } else {
        noTone(BUZZER_PIN);
        buzzerState = false;
      }
      buzzerLast = now;
      buzzerPlaybackPhase = BUZZER_PHASE_PLAYING_NOTE;
      break;

    case BUZZER_PHASE_PLAYING_NOTE:
      if (now - buzzerLast >= step.durationMs) {
        noTone(BUZZER_PIN);
        buzzerState = false;
        buzzerLast = now;
        buzzerPlaybackPhase = BUZZER_PHASE_NOTE_GAP;
      }
      break;

    case BUZZER_PHASE_NOTE_GAP:
      if (now - buzzerLast >= step.pauseAfterMs) {
        ++buzzerStep;
        buzzerPlaybackPhase = BUZZER_PHASE_START_NOTE;
      }
      break;
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

    if (soloMode) {
      if (switchState == HIGH) {
        Serial.println("⚡ Solo Mode ON — local activation only.");
        currentRGBMode = activeRGBSetting;
        applyRGBMode();
        triggerBuzzerPattern(activeBuzzerSetting);
      } else {
        Serial.println("⚡ Solo Mode OFF position — local box inactive.");
        currentRGBMode = inactiveRGBSetting;
        applyRGBMode();
        triggerBuzzerPattern(inactiveBuzzerSetting);
      }

      if (active_box == BOX_NAME) {
        setActiveBox("NONE");
      }
    } else if (switchState == HIGH) {
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
  
  bool shouldRunForward = (switchState == HIGH) && (soloMode || active_box != BOX_NAME);

  // Determine desired motor state
  if (shouldRunForward) {
    // Forward direction — limit switch ignored
    Serial.println(soloMode ? "Forward (Solo Mode)" : "Forward");
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

  if (soloMode) {
    Serial.println("Solo Mode is enabled — ignoring shared active_box changes.");
    return;
  }

  // Set stateChanged to true. This causes modifyMotorState() to run on the next loop even with not changes to 
  // switch positions which will trigger the motor to run based on the active_box variable and the current switch positions
  stateChanged = true; 
}

namespace {
  struct ParsedHttpsUrl {
    String host;
    String path;
    uint16_t port = 443;
  };

  bool parseHttpsUrl(const String& url, ParsedHttpsUrl& parsed) {
    if (!url.startsWith("https://")) {
      Serial.println("Only https:// OTA URLs are supported.");
      return false;
    }

    int hostStart = 8; // strlen("https://")
    int pathStart = url.indexOf('/', hostStart);
    String hostPort = pathStart >= 0 ? url.substring(hostStart, pathStart) : url.substring(hostStart);

    parsed.path = pathStart >= 0 ? url.substring(pathStart) : "/";
    if (parsed.path.length() == 0) {
      parsed.path = "/";
    }

    int colonPos = hostPort.indexOf(':');
    if (colonPos >= 0) {
      parsed.host = hostPort.substring(0, colonPos);
      parsed.port = static_cast<uint16_t>(hostPort.substring(colonPos + 1).toInt());
    } else {
      parsed.host = hostPort;
      parsed.port = 443;
    }

    return parsed.host.length() > 0;
  }

  bool isRedirectStatus(int statusCode) {
    return statusCode == 301 || statusCode == 302 || statusCode == 307 || statusCode == 308;
  }

  String resolveRedirectUrl(const String& location, const ParsedHttpsUrl& currentUrl) {
    String redirectUrl = location;
    redirectUrl.trim();

    if (redirectUrl.startsWith("https://")) {
      return redirectUrl;
    }

    String baseUrl = "https://" + currentUrl.host;
    if (currentUrl.port != 443) {
      baseUrl += ":" + String(currentUrl.port);
    }

    if (redirectUrl.startsWith("/")) {
      return baseUrl + redirectUrl;
    }

    return baseUrl + "/" + redirectUrl;
  }

  String readHeaderValueByName(HttpClient& http, const String& headerName) {
    while (http.headerAvailable()) {
      String name = http.readHeaderName();
      String value = http.readHeaderValue();
      if (name.equalsIgnoreCase(headerName)) {
        return value;
      }
    }
    return "";
  }

  String fetchTextFromUrl(const String& url, uint8_t redirectCount = 0) {
    if (redirectCount > 5) {
      Serial.println("Too many redirects while fetching the OTA manifest.");
      return "";
    }

    ParsedHttpsUrl parsedUrl;
    if (!parseHttpsUrl(url, parsedUrl)) {
      return "";
    }

    WiFiClientSecure networkClient;
    networkClient.setInsecure(); // Simplest setup for GitHub-hosted HTTPS files
    HttpClient http(networkClient, parsedUrl.host, parsedUrl.port);

    int requestResult = http.get(parsedUrl.path);
    if (requestResult != HTTP_SUCCESS) {
      Serial.print("Manifest GET start failed: ");
      Serial.println(requestResult);
      http.stop();
      return "";
    }

    int statusCode = http.responseStatusCode();
    if (statusCode < 0) {
      Serial.print("Manifest response error: ");
      Serial.println(statusCode);
      http.stop();
      return "";
    }

    if (isRedirectStatus(statusCode)) {
      String location = readHeaderValueByName(http, "Location");
      http.stop();
      if (location.length() == 0) {
        Serial.println("Redirect response missing Location header.");
        return "";
      }
      return fetchTextFromUrl(resolveRedirectUrl(location, parsedUrl), redirectCount + 1);
    }

    if (statusCode != 200) {
      Serial.print("Manifest GET failed, code: ");
      Serial.println(statusCode);
      http.stop();
      return "";
    }

    String body = http.responseBody();
    http.stop();
    return body;
  }

  void performOTAFromUrlWithRedirects(const String& url, uint8_t redirectCount = 0) {
    if (redirectCount > 5) {
      Serial.println("Too many redirects while downloading firmware.");
      return;
    }

    ParsedHttpsUrl parsedUrl;
    if (!parseHttpsUrl(url, parsedUrl)) {
      return;
    }

    WiFiClientSecure networkClient;
    networkClient.setInsecure(); // Simplest setup for GitHub-hosted HTTPS files
    HttpClient http(networkClient, parsedUrl.host, parsedUrl.port);

    Serial.print("Starting OTA from URL: ");
    Serial.println(url);

    int requestResult = http.get(parsedUrl.path);
    if (requestResult != HTTP_SUCCESS) {
      Serial.print("HTTP GET start failed: ");
      Serial.println(requestResult);
      http.stop();
      return;
    }

    int statusCode = http.responseStatusCode();
    if (statusCode < 0) {
      Serial.print("HTTP response error: ");
      Serial.println(statusCode);
      http.stop();
      return;
    }

    if (isRedirectStatus(statusCode)) {
      String location = readHeaderValueByName(http, "Location");
      http.stop();
      if (location.length() == 0) {
        Serial.println("Redirect response missing Location header.");
        return;
      }

      String redirectUrl = resolveRedirectUrl(location, parsedUrl);
      Serial.print("Following redirect to: ");
      Serial.println(redirectUrl);
      performOTAFromUrlWithRedirects(redirectUrl, redirectCount + 1);
      return;
    }

    if (statusCode != 200) {
      Serial.print("HTTP GET failed, code: ");
      Serial.println(statusCode);
      http.stop();
      return;
    }

    long contentLength = http.contentLength();
    Serial.print("Firmware size: ");
    Serial.println(contentLength);

    if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
      Serial.println("Not enough space or OTA begin failed.");
      http.stop();
      return;
    }

    size_t written = Update.writeStream(http);
    Serial.print("Written bytes: ");
    Serial.println(written);

    if (contentLength > 0 && written != static_cast<size_t>(contentLength)) {
      Serial.println("OTA write incomplete.");
      Update.abort();
      http.stop();
      return;
    }

    if (!Update.end()) {
      Serial.print("OTA Update failed. Error #: ");
      Serial.println(Update.getError());
      http.stop();
      return;
    }

    if (!Update.isFinished()) {
      Serial.println("OTA Update did not finish cleanly.");
      http.stop();
      return;
    }

    Serial.println("OTA Update successful — restarting.");
    http.stop();
    delay(500);
    ESP.restart();
  }
}

void handleOTACheck() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (lastOTACheck != 0 && (now - lastOTACheck < OTA_CHECK_INTERVAL_MS)) {
    return;
  }

  lastOTACheck = now;
  checkForOTAUpdate();
}

void checkForOTAUpdate() {
  if (OTA_MANIFEST_URL[0] == '\0') {
    return;
  }

  Serial.println("Checking OTA manifest...");
  Serial.println(OTA_MANIFEST_URL);

  String body = fetchTextFromUrl(OTA_MANIFEST_URL);
  if (body.length() == 0) {
    Serial.println("OTA manifest fetch returned no data.");
    return;
  }

  int newline = body.indexOf('\n');
  if (newline < 0) {
    Serial.println("Invalid manifest format.");
    return;
  }

  String remoteVersion = body.substring(0, newline);
  remoteVersion.trim();

  String firmwareUrl = body.substring(newline + 1);
  firmwareUrl.trim();

  Serial.print("Current FW: ");
  Serial.println(CURRENT_FW_VERSION);
  Serial.print("Remote FW: ");
  Serial.println(remoteVersion);

  if (remoteVersion == CURRENT_FW_VERSION) {
    Serial.println("No OTA update available.");
    return;
  }

  if (firmwareUrl.length() == 0) {
    Serial.println("Manifest missing firmware URL.");
    return;
  }

  Serial.println("New firmware found. Starting OTA...");
  performOTAFromUrl(firmwareUrl);
}

// Called when `ota_url` or `ota_target` change via Arduino IoT Cloud.
void onOtaRequestChange() {
  Serial.println("OTA request changed via Cloud.");
  Serial.print("Target: "); Serial.println(ota_target);
  Serial.print("URL: "); Serial.println(ota_url);

  if (ota_url.length() == 0) {
    Serial.println("No OTA URL provided — ignoring.");
    return;
  }

  // If the request targets this box or "ALL", proceed
  if (ota_target.equalsIgnoreCase(BOX_NAME) || ota_target.equalsIgnoreCase("ALL")) {
    Serial.println("This device is targeted for OTA. Starting download...");
    performOTAFromUrl(ota_url);
  } else {
    Serial.println("OTA request not targeted at this device. Ignoring.");
  }
}

// Download firmware from a URL and apply OTA update.
void performOTAFromUrl(const String& url) {
  performOTAFromUrlWithRedirects(url);
}
