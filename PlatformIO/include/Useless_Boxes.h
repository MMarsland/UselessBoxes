#ifndef USELESS_BOXES_H
#define USELESS_BOXES_H

// ===== Pin assignments =====
extern const int IN1;
extern const int IN2;
extern const int EN1;
extern const int LED_R;
extern const int LED_G;
extern const int LED_B;
extern const int SWITCH_PIN;
extern const int LIMIT_PIN;
extern const int BUTTON_PIN;
extern const int BUZZER_PIN;

// ===== SETTINGS BUTTON HANDLER =====
extern const unsigned long LONG_PRESS_TIME;
extern const unsigned long DEBOUNCE_TIME;

extern int menuIndex;
extern const int totalMenus;
extern bool inSubMenu;

extern bool motorAutoMode;
extern int led_brightness_percentage;

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

extern const unsigned long MENU_TIMEOUT_MS;
extern unsigned long lastInteractionTime;

extern bool led_on;

// ===== MOTOR CONTROL =====
extern unsigned long lastMotorUpdate;
extern const unsigned long MOTOR_UPDATE_INTERVAL;

extern bool switch_forward;
extern bool button_pressed;
extern bool motor_forward;
extern bool motor_enabled;

// ===== FUNCTION PROTOTYPES =====

// Core handlers
void handleSettingsButton();
void handleSerialMenu();
void handleMotorControl();
void modifyMotorState(bool switchState, bool buttonState);
void adjustLED();
void setActiveBox(String box);
void onActiveBoxChange();

// Menu helpers
void showMenu();
void enterSubMenu();
void adjustSubMenu();
void showMotorMode();
void showBrightness();
void showActiveBox();

#endif