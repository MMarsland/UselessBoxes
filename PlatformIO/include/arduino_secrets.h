#pragma once
// Wrapper header: selects device-specific arduino_secrets file based on build flags
// Uses the same BOARD_PINS_* build flags that select the board pin mappings.

#ifndef ARDUINO_SECRETS_H
    #define ARDUINO_SECRETS_H

    #if defined(BOARD_PINS_TREVOR)
        #include "arduino_secrets_trevor.h"
    #elif defined(BOARD_PINS_MICHAEL)
        #include "arduino_secrets_michael.h"
    #else
        // Default to MICHAEL
        #include "arduino_secrets_michael.h"
    #endif

#endif // ARDUINO_SECRETS_H
