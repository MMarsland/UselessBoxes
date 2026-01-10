#pragma once
// Pin mappings for TREVOR board
#ifndef BOARD_PINS_TREVOR_H
    #define BOARD_PINS_TREVOR_H

    // Box name (moved here from arduino_secrets)
    #define BOX_NAME "TREVOR"

    constexpr int EN1 = 4;           // Motor enable (PWM capable)
    constexpr int IN1 = 3;           // Motor direction A
    constexpr int IN2 = 2;           // Motor direction B
    constexpr int RGB_R = 6;         // LED Red
    constexpr int RGB_G = 5;         // LED Green
    constexpr int RGB_B = 7;         // LED Blue
    constexpr int SWITCH_PIN = 8;    // SPDT switch
    constexpr int LIMIT_PIN = 9;     // Limit Switch
    constexpr int BUTTON_PIN = 10;   // Settings button
    constexpr int BUZZER_PIN = 11;   // Buzzer

#endif // BOARD_PINS_TREVOR_H
