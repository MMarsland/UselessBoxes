# GitHub Copilot Instructions — UselessBoxes

## Quick summary
- This is an Arduino (Nano ESP32) + PlatformIO project that runs a "Useless Box" (motor, RGB LED, buzzer, switches). The firmware is in `PlatformIO/src/Useless_Boxes.cpp` and device/cloud integration is defined in `include/thingProperties.h`.

## Where to look first (big picture)
- `PlatformIO/src/Useless_Boxes.cpp` — Main firmware: setup(), loop(), motor, LED, buzzer, menu system and non-blocking timing using `millis()`.
- `PlatformIO/include/Useless_Boxes.h` — Shared declarations (externs, enums, menu API).
- `PlatformIO/include/thingProperties.h` — Arduino IoT Cloud setup and `active_box` variable; `arduino_secrets.h` holds WiFi/device secrets.
- `README.md` — Wiring, board/libraries, and PlatformIO migration notes.

## Build / run / debug (explicit commands & notes)
- Project uses PlatformIO in VSCode. Typical commands:
  - Build: `platformio run` (or use the VSCode PlatformIO UI)
  - Upload: `platformio run --target upload --environment arduino_nano_esp32 --upload-port COM4`
  - Monitor/Serial: `platformio device monitor` (or VSCode PlatformIO Monitor)
- Important: the repo expects the Arduino ESP32 board package installed in the Arduino IDE and ArduinoIoTCloud library available as described in `README.md`. `platformio.ini` uses `lib_extra_dirs` and `lib_ignore` to reference Arduino IDE libraries.

## Secrets & cloud integration
- `PlatformIO/include/arduino_secrets.h` contains `SECRET_SSID`, `SECRET_OPTIONAL_PASS`, `SECRET_DEVICE_KEY`, and `SECRET_BOX_NAME` — replace or keep local copies before flashing devices.
- `thingProperties.h` wires `active_box` to Arduino IoT Cloud and calls `onActiveBoxChange()` on updates.

## Key project-specific conventions & patterns
- Non-blocking design: animations, motor updates, and buzzer use `millis()` intervals (see `MOTOR_UPDATE_INTERVAL`, `RGB_UPDATE_INTERVAL`, and buzzer state machine). Avoid `delay()` to preserve responsiveness.
- Menu system is data-driven: add a new menu by implementing `showX()`, `adjustX()`, `confirmX()` and adding a `MenuItem` entry to `menuItems[]` in `Useless_Boxes.cpp`.
  - Example: add `{ "New Feature", showNew, adjustNew, confirmNew }` to `menuItems[]` and implement the three functions.
- Enums + COUNT sentinel: RGB and buzzer modes use an enum and a *_COUNT value for wrap-around (e.g., `currentRGBMode = (currentRGBMode + 1) % RGB_MODE_COUNT`). Follow this pattern for new mode lists.
- Two-stage confirm pattern: menu `adjust` should update a transient variable (e.g., `requestedBuzzerPattern`), and `confirm` commits it (e.g., `activeBuzzerPattern = requestedBuzzerPattern`). Follow this pattern where appropriate.
- Hardware inputs use `INPUT_PULLUP` and rely on inverted logic (LOW = pressed/active for switches/buttons). Check `modifyMotorState()` and `handleSettingsButton()` for examples.
- Global state is declared in the header as `extern` and defined in the `.cpp`. When adding new state, add to both files to keep linkage clear.

## Tests and expectations
- There are no automated unit tests in the repo. Testing is hardware-driven: deploy to a board and use serial logs and the `showMenu()` output for verification.
- Use Serial prints (existing throughout code) and PlatformIO Monitor to observe runtime behavior and menus.

## Files you will commonly edit
- `PlatformIO/src/Useless_Boxes.cpp` — feature logic, menus, timers, hardware behavior
- `PlatformIO/include/Useless_Boxes.h` — declarations and new externs
- `PlatformIO/include/thingProperties.h` & `arduino_secrets.h` — cloud properties and credentials
- `platformio.ini` — if library paths or envs need updates

## Small examples / gotchas (copyable guidance)
- Add a timed task: create `lastX` and `X_INTERVAL` constants and check `if (millis() - lastX >= X_INTERVAL) { lastX = millis(); /* work */ }` (see RGB and motor handlers).
- Add a menu option:
  1. Add `void showFoo(); void adjustFoo(); void confirmFoo();`
  2. Add `{ "Foo", showFoo, adjustFoo, confirmFoo }` to `menuItems[]`.
  3. Implement actions and ensure `showFoo()` prints status for live preview.
- Changing behavior remotely: update `active_box` in the cloud (Arduino IoT Cloud) — `onActiveBoxChange()` is invoked from `thingProperties.h`.

## When to ask for human help
- If you need access credentials for the Arduino Cloud (device key/SSID) or if the device behaves differently than the serial logs show (hardware fault likely).
- If a build fails due to missing ArduinoIDE library packages — confirm `lib_extra_dirs` points at your local Arduino libraries.

---
If any of these sections are unclear or you want more examples (e.g., adding a new RGB animation, or a guided patch that adds a menu item), tell me which area to expand and I will iterate. — GitHub Copilot (Raptor mini, Preview)
