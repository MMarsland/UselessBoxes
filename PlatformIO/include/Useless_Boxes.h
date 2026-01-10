#ifndef USELESS_BOXES_H
#define USELESS_BOXES_H

// ------------------------------------------------------------------
// Hardware pin mappings — supplied by include/board_pins.h
// Select the active mapping by setting a build flag (e.g. -DBOARD_PINS_MICHAEL)
// or by providing your own `include/board_pins.h` in the project.
// ------------------------------------------------------------------
#include "board_pins.h"

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
constexpr int DEFAULT_MOTOR_SPEED_PERCENTAGE = 100; // % // Motor speed (0-100)

// Motor soft-start configuration
constexpr int MOTOR_MIN_DUTY_CYCLE = 10;  // Minimum PWM duty (out of 255) to keep motor running smoothly (~35%)
constexpr int MOTOR_SOFT_START_DURATION = 150; // ms - duration to apply full power during startup
// Note: During soft-start, motor uses FULL POWER (255) to overcome static friction, then ramps down to target speed
constexpr int MOTOR_SOFT_START_DUTY = 200; // Unused - kept for reference (we use 255 = full power)

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

// Board on-board status LED not currently configurable — always off
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

extern int currentBuzzerPattern;    // playback state for active buzzer pattern (played by presets)
extern bool buzzerState;           // internal on/off used by loops
extern unsigned long buzzerLast;   // last toggle time
extern unsigned int buzzerStep;    // step in sequence

constexpr unsigned long BUZZER_INTERVAL = 250; // ms

void stopBuzzer();
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
void handleSwitchDetection();
void modifyMotorState(bool switchState, bool buttonState);

// Motor speed control (0-100%) — setter persists and applies
extern int motor_speed_percent;
void setMotorSpeed(int percent);

// Menu handlers for motor speed
void showMotorSpeed();
void adjustMotorSpeed();
void confirmMotorSpeed();

// ===== ACTIVE BOX =====
extern String active_box;

void setActiveBox(String box);
void onActiveBoxChange();

// ===== SETTINGS BUTTON HANDLER =====
void handleSettingsButton();

#endif // USELESS_BOXES_H
