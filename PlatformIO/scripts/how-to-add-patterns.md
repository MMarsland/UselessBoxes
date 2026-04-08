# How to add new LED and buzzer patterns

This folder supports a **simple file-based workflow** for creating new patterns.
After a successful add, the template files are automatically reset back to starter examples.

---

## LED pattern workflow

### 1) Edit `scripts/led_pattern.txt`

Use a single file with the pattern name on a `name:` line.
Everything below that is treated as the renderer body.

Example:

```txt
name: Ocean Pulse

const float cycle = (now % 2500UL) / 2500.0f;
const float wave = (sin(cycle * TWO_PI) + 1.0f) * 0.5f;
const uint8_t blue = static_cast<uint8_t>(40.0f + (wave * 215.0f));
const uint8_t green = static_cast<uint8_t>(10.0f + (wave * 90.0f));
return makeRGBColor(10, green, blue);
```

### 2) Preview safely (recommended)

```powershell
.\.venv\Scripts\python.exe scripts\add_led_pattern.py --dry-run
```

### 3) Add the pattern for real

```powershell
.\.venv\Scripts\python.exe scripts\add_led_pattern.py
```

### 4) What happens next

- The new LED pattern is added to the firmware files.
- `led_pattern.txt` is reset back to the starter example template.

---

## Buzzer pattern workflow

### 1) Edit `scripts/buzzer_pattern.json`

Add a name, a `steps` array, and a `loops` flag:

```jsonc
{
  "name": "Victory Chime",
  "steps": [
    { "frequency": 900,  "duration": 100, "pauseDuration": 60 },
    { "frequency": 1200, "duration": 120, "pauseDuration": 40 },
    { "frequency": 1500, "duration": 160, "pauseDuration": 0 }
  ],
  "loops": false
}
```

### 2) Preview safely (recommended)

```powershell
.\.venv\Scripts\python.exe scripts\add_buzzer_pattern.py --dry-run
```

### 3) Add the pattern for real

```powershell
.\.venv\Scripts\python.exe scripts\add_buzzer_pattern.py
```

### 4) What happens next

- The new buzzer pattern is added to the firmware files.
- `buzzer_pattern.json` is reset back to the starter example template.

---

## Comment support

The scripts accept:

- standard JSON
- Python-style single-quoted literals
- comment lines using `//`, `#`, or `/* ... */`

So you can keep example blocks commented out in the template files.

---

## Tips

- Use unique names so patterns are not added twice.
- In `led_pattern.txt`, keep only the `name:` line plus the renderer body — **do not** include the full function signature.
- Use `--dry-run` first if you want to validate before changing firmware files.
- After adding a pattern, build/upload as normal and select it from the menu.
