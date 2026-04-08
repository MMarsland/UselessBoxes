from __future__ import annotations

"""Add an LED pattern from the standard `led_pattern.txt` file.

Format:
    name: Example Pulse

    const float cycle = (now % 2000UL) / 2000.0f;
    return makeRGBColor(20, 60, 200);
"""

import argparse
import sys
from pathlib import Path

from add_pattern import PatternScaffoldError, add_rgb_pattern, read_text, write_text

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_PATTERN_PATH = SCRIPT_DIR / "led_pattern.txt"
DEFAULT_LED_TEMPLATE = """// LED pattern template\n// 1) Change the name below\n// 2) Edit the renderer body\n// 3) Run:\n//    .\\.venv\\Scripts\\python.exe scripts\\add_led_pattern.py --dry-run\n//    .\\.venv\\Scripts\\python.exe scripts\\add_led_pattern.py\n\nname: My New LED Pattern\n\n// Write only the body of:\n//   RGBColor renderYourPattern(unsigned long now)\n\nconst float cycle = (now % 2000UL) / 2000.0f;\nconst float wave = (sin(cycle * TWO_PI) + 1.0f) * 0.5f;\nconst uint8_t red = static_cast<uint8_t>(20.0f + (wave * 80.0f));\nconst uint8_t blue = static_cast<uint8_t>(40.0f + (wave * 215.0f));\nreturn makeRGBColor(red, 60, blue);\n"""


def parse_led_pattern_file(path: Path) -> tuple[str, str]:
    raw = read_text(path)
    lines = raw.splitlines()

    name: str | None = None
    body_start_index: int | None = None

    for index, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue

        if stripped.lower().startswith("name:"):
            name = stripped.split(":", 1)[1].strip()
            body_start_index = index + 1
            break

        raise PatternScaffoldError(
            f"{path.name}: first non-comment content must be `name: Your Pattern Name`."
        )

    if not name:
        raise PatternScaffoldError(f"{path.name} is missing a `name:` line.")

    body = "\n".join(lines[body_start_index:]).strip()
    if not body:
        raise PatternScaffoldError(f"{path.name} is missing the LED renderer body.")

    return name, body


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read led_pattern.txt, then add the new LED pattern."
    )
    parser.add_argument(
        "--config",
        default=str(DEFAULT_PATTERN_PATH),
        help="Path to the LED pattern text file (default: scripts/led_pattern.txt)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate and preview without changing the firmware files",
    )
    args = parser.parse_args()

    try:
        pattern_path = Path(args.config).resolve()
        name, body = parse_led_pattern_file(pattern_path)
        add_rgb_pattern(name, body, args.dry_run)

        if not args.dry_run:
            write_text(pattern_path, DEFAULT_LED_TEMPLATE, dry_run=False)
            print(f"Reset {pattern_path.name} back to the starter example.")

        print(f"Processed LED pattern from {pattern_path.name}.")
        return 0
    except PatternScaffoldError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
