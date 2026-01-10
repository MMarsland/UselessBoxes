#pragma once
// board_pins.h — include one of the per-board mappings via build flags
// Usage: add -DBOARD_PINS_MICHAEL or -DBOARD_PINS_TREVOR to build_flags in platformio.ini

#ifndef BOARD_PINS_H
    #define BOARD_PINS_H

    #if defined(BOARD_PINS_MICHAEL)
        #include "board_pins_michael.h"
    #elif defined(BOARD_PINS_TREVOR)
        #include "board_pins_trevor.h"
    #else
        // Default to MICHAEL if no build flag provided
        #include "board_pins_michael.h"
    #endif

#endif // BOARD_PINS_H
