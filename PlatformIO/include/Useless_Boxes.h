#ifndef USELESS_BOXES_H
#define USELESS_BOXES_H

// ===== Pin assignments =====
extern const int IN1;
extern const int IN2;
extern const int EN1;
extern const int RGB_R;
extern const int RGB_G;
extern const int RGB_B;
extern const int SWITCH_PIN;
extern const int LIMIT_PIN;
extern const int BUTTON_PIN;
extern const int BUZZER_PIN;

// ===== SETTINGS BUTTON =====
extern const unsigned long LONG_PRESS_TIME;
extern const unsigned long DEBOUNCE_TIME;

// Button state tracking
extern bool settingsButtonState;
extern bool lastSettingsButtonState;
extern unsigned long pressedTime;
extern unsigned long releasedTime;
extern bool longPressActive;
extern unsigned long lastDebounceTime;
extern int shortPressCount;
extern int longPressCount;
extern int lastShortPressCount;
extern int lastLongPressCount;

// ===== MENU SYSTEM =====
extern int menuIndex;
extern int totalMenus;
extern bool inSubMenu;
extern const unsigned long MENU_TIMEOUT_MS;
extern unsigned long lastInteractionTime;

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
void showMotorMode();
void adjustMotorMode();
void confirmMotorMode();

void showBrightness();
void adjustBrightness();
void confirmBrightness();

void showActiveBox();
void adjustActiveBox();
void confirmActiveBox();

void showRGB();
void adjustRGB();
void confirmRGB();

void showBuzzer();
void adjustBuzzer();
void confirmBuzzer();

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

int currentRGBMode = RGB_OFF;
unsigned long lastRGBAnimation = 0;
int rainbowPos = 0;
int breathValue = 0;
int breathDir = 1;

const int RGB_UPDATE_INTERVAL = 20; // ms

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

int requestedBuzzerPattern = BUZZER_OFF; // updated by menu immediately
int activeBuzzerPattern = BUZZER_OFF;    // only updated on "confirm" (long press)
bool buzzerState = false;           // on/off state for looping patterns
unsigned long buzzerLast = 0;       // last toggle time
unsigned int buzzerStep = 0;        // step in sequence
const unsigned long BUZZER_INTERVAL = 250; // ms

void stopBuzzer();
void confirmBuzzerPattern();
void updateBuzzerAlarm();


// ===== MOTOR CONTROL =====
extern bool switch_forward;
extern bool button_pressed;
extern bool motor_forward;
extern bool motor_enabled;

void handleMotorControl();
void modifyMotorState(bool switchState, bool buttonState);

// ===== BOARD LED CONTROL =====
extern bool led_on;
extern int led_brightness_percentage;
void adjustLED();

// ===== ACTIVE BOX =====
extern String active_box;

void setActiveBox(String box);
void onActiveBoxChange();

// ===== SETTINGS BUTTON HANDLER =====
void handleSettingsButton();

#endif // USELESS_BOXES_H
