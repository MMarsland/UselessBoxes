#ifndef USELESS_BOXES_H
#define USELESS_BOXES_H

// ------------------------------------------------------------------
// Hardware pin mappings — change here to remap pins for your board
// ------------------------------------------------------------------
constexpr int EN1 = 2;           // Motor enable (PWM capable)
constexpr int IN1 = 3;           // Motor direction A
constexpr int IN2 = 4;           // Motor direction B
constexpr int RGB_R = 7;         // LED Red
constexpr int RGB_G = 5;         // LED Green
constexpr int RGB_B = 6;         // LED Blue
constexpr int SWITCH_PIN = 8;    // SPDT switch
constexpr int LIMIT_PIN = 9;     // Limit Switch
constexpr int BUTTON_PIN = 10;   // Settings button
constexpr int BUZZER_PIN = 11;   // Buzzer

// ------------------------------------------------------------------
// Configurable defaults (change these compile-time defaults to tune
// product defaults; runtime values are initialized from these)
// ------------------------------------------------------------------
constexpr unsigned long DEFAULT_LONG_PRESS_TIME = 1000;   // ms  // Adjustable
constexpr unsigned long DEFAULT_DEBOUNCE_TIME   = 50;     // ms  // Adjustable
constexpr unsigned long DEFAULT_MENU_TIMEOUT_MS = 30000; // ms  // Adjustable
constexpr unsigned long DEFAULT_MOTOR_UPDATE_INTERVAL = 50; // ms // Adjustable
constexpr int RGB_UPDATE_INTERVAL = 20; // ms // Adjustable

constexpr int DEFAULT_RGB_BRIGHTNESS_PERCENTAGE = 100; // % // Setting
constexpr int DEFAULT_BUZZER_VOLUME_PERCENTAGE = 100; // % // Setting

// ------------------------------------------------------------------
// Runtime-configurable settings (modifiable during development)
// Use the setters below to ensure side-effects are applied.
// ------------------------------------------------------------------
extern unsigned long LONG_PRESS_TIME;
extern unsigned long DEBOUNCE_TIME;
extern unsigned long MENU_TIMEOUT_MS;
extern unsigned long MOTOR_UPDATE_INTERVAL;
extern int rgb_brightness_percentage;
// Setter APIs (validate / apply side-effects)
void setLongPressTime(unsigned long ms);
void setDebounceTime(unsigned long ms);
void setMenuTimeout(unsigned long ms);
void setMotorUpdateInterval(unsigned long ms);
void setRGBBrightness(int percent);

// Menu structure
struct MenuItem {
  const char* name;
  void (*onShow)();
  void (*onAdjust)();
  void (*onConfirm)();
};
extern MenuItem menuItems[];

// Menu handlers
void handleSerialMenu();
void showMenu();

// Individual menu functions
void showRGBBrightness();
void adjustRGBBrightness();
void confirmRGBBrightness();

// ===== RGB LED CONTROL =====
enum RGBMode {
  RGB_OFF,
  RGB_WHITE,
  RGB_RAINBOW,
  RGB_BREATHING,
  RGB_SOLID_RED,
  RGB_SOLID_GREEN,
  RGB_SOLID_BLUE,
  RGB_MODE_COUNT
};

extern int led_brightness_percentage; // Setting
extern int currentRGBMode;
extern unsigned long lastRGBAnimation;
extern int rainbowPos;
extern int breathValue;
extern int breathDir;

void setRGB(uint8_t r, uint8_t g, uint8_t b);
void applyRGBMode();
void updateAnimations();

// ===== BUZZER CONTROL =====
enum BuzzerPattern {
  BUZZER_OFF,
  BUZZER_SINGLE,
  BUZZER_CHIRP,
  BUZZER_LOOP,
  BUZZER_SOS,
  BUZZER_PATTERN_COUNT
};

extern int requestedBuzzerPattern; // updated by menu immediately
extern int currentBuzzerPattern;    // applied on confirm
extern bool buzzerState;           // internal on/off used by loops
extern unsigned long buzzerLast;   // last toggle time
extern unsigned int buzzerStep;    // step in sequence

constexpr unsigned long BUZZER_INTERVAL = 250; // ms

void stopBuzzer();
void confirmBuzzerPattern();
void updateBuzzerAlarm();

// Settings for Active/Inactive behaviors (persisted presets)
extern int activeRGBSetting;    // RGB mode when this box is active
extern int inactiveRGBSetting;  // RGB mode when this box is inactive
extern int activeBuzzerSetting;   // buzzer pattern to play when active
extern int inactiveBuzzerSetting; // buzzer pattern to play when inactive
extern int buzzer_volume_percentage; // buzzer volume (0-100)

// Setter APIs for Active/Inactive presets
void setActiveRGBSetting(int mode);
void setInactiveRGBSetting(int mode);
void setActiveBuzzerSetting(int pattern);
void setInactiveBuzzerSetting(int pattern);
void setBuzzerVolume(int percent);

// Menu handlers for Active/Inactive presets
void showActiveRGB();
void adjustActiveRGB();
void confirmActiveRGB();

void showInactiveRGB();
void adjustInactiveRGB();
void confirmInactiveRGB();

void showActiveBuzzerSetting();
void adjustActiveBuzzerSetting();
void confirmActiveBuzzerSetting();

void showInactiveBuzzerSetting();
void adjustInactiveBuzzerSetting();
void confirmInactiveBuzzerSetting();

void showBuzzerVolume();
void adjustBuzzerVolume();
void confirmBuzzerVolume();


// ===== MOTOR CONTROL =====
// Motor internal state is kept private to the implementation (.cpp)
void handleMotorControl();
void modifyMotorState(bool switchState, bool buttonState);

// ===== ACTIVE BOX =====
extern String active_box;
extern bool box_active; // true when this physical box is currently "active"

void setActiveBox(String box);
void onActiveBoxChange();

// ===== SETTINGS BUTTON HANDLER =====
void handleSettingsButton();

#endif // USELESS_BOXES_H
