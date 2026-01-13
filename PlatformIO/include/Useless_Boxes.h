#ifndef USELESS_BOXES_H
#define USELESS_BOXES_H

// ------------------------------------------------------------------
// Hardware pin mappings — supplied by include/board_pins.h
// Select the active mapping by setting a build flag (e.g. -DBOARD_MICHAEL)
// or by providing your own `include/board_pins.h` in the project.
// ------------------------------------------------------------------
#include "board_pins.h"

// ------------------------------------------------------------------
// Configurable defaults (change these compile-time defaults to tune
// product defaults; runtime values are initialized from these)
// ------------------------------------------------------------------
constexpr unsigned long DEFAULT_LONG_PRESS_TIME = 500;   // ms  // Adjustable
constexpr unsigned long DEFAULT_DEBOUNCE_TIME   = 50;     // ms  // Adjustable
constexpr unsigned long DEFAULT_MENU_TIMEOUT_MS = 10000; // ms  // Adjustable
constexpr unsigned long DEFAULT_MOTOR_UPDATE_INTERVAL = 1; // ms // Adjustable
constexpr int RGB_UPDATE_INTERVAL = 20; // ms // Adjustable

constexpr int DEFAULT_RGB_BRIGHTNESS_PERCENTAGE = 100; // % // Setting

// ------------------------------------------------------------------
// Runtime-configurable settings (modifiable during development)
// Use the setters below to ensure side-effects are applied.
// ------------------------------------------------------------------
extern unsigned long LONG_PRESS_TIME;
extern unsigned long DEBOUNCE_TIME;
extern unsigned long MENU_TIMEOUT_MS;
extern unsigned long MOTOR_UPDATE_INTERVAL;

// Setter APIs (validate / apply side-effects)
void setLongPressTime(unsigned long ms);
void setDebounceTime(unsigned long ms);
void setMenuTimeout(unsigned long ms);
void setMotorUpdateInterval(unsigned long ms);


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

void updateRGBModeFromBoxState();
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
void beepBuzzer(int quantity, int duration_ms=100, int pause_ms=100, int toneFreq=1000);
void updateBuzzerAlarm();
void triggerBuzzerPattern(int pattern);
void demoBuzzerPattern(int pattern);


// Settings for Active/Inactive behaviors (persisted presets)
extern int activeRGBSetting;    // RGB mode when this box is active
extern int inactiveRGBSetting;  // RGB mode when this box is inactive
extern int rgb_brightness_percentage;
extern int activeBuzzerSetting;   // buzzer pattern to play when active
extern int inactiveBuzzerSetting; // buzzer pattern to play when inactive
extern int motorSpeed; // Motor speed control (0-100%)

// Setter APIs for Active/Inactive presets
void setActiveRGBSetting(int mode);
void setInactiveRGBSetting(int mode);
void setRGBBrightness(int percent);
void setActiveBuzzerSetting(int pattern);
void setInactiveBuzzerSetting(int pattern);
void setMotorSpeed(int speed);

// Menu handlers for Active/Inactive presets
void showActiveRGB();
void adjustActiveRGB();
void confirmActiveRGB();

void showInactiveRGB();
void adjustInactiveRGB();
void confirmInactiveRGB();

void showRGBBrightness();
void adjustRGBBrightness();
void confirmRGBBrightness();

void showActiveBuzzerSetting();
void adjustActiveBuzzerSetting();
void confirmActiveBuzzerSetting();

void showInactiveBuzzerSetting();
void adjustInactiveBuzzerSetting();
void confirmInactiveBuzzerSetting();

void showMotorSpeed();
void adjustMotorSpeed();
void confirmMotorSpeed();

// ===== SETTINGS BUTTON HANDLER =====
void handleSettingsButton();

void handleSwitchDetection();

// ===== MOTOR PWM CONTROL =====
void modifyMotorState(bool switchState, bool buttonState);
void updateMotorPWM();



// ===== ACTIVE BOX =====
extern String active_box;

void setActiveBox(String box);
void onActiveBoxChange();
#endif // USELESS_BOXES_H
