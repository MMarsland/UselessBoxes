from __future__ import annotations

"""Add buzzer patterns from the standard `buzzer_pattern.json` file.

Example `buzzer_pattern.json`:
{
  "name": "Example Fanfare",
  "steps": [
    { "frequency": 900, "duration": 100, "pauseDuration": 60 },
    { "frequency": 1200, "duration": 120, "pauseDuration": 0 }
  ],
  "loops": false
}
"""

import argparse
import sys
from pathlib import Path

from add_pattern import (
    PatternScaffoldError,
    add_buzzer_pattern,
    load_structured_file,
    normalize_pattern_entries,
    normalize_steps_data,
    parse_bool,
    write_text,
)

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CONFIG_PATH = SCRIPT_DIR / "buzzer_pattern.json"
DEFAULT_BUZZER_TEMPLATE = """// Buzzer pattern template\n// Edit the active object below, then run:\n//   .\\.venv\\Scripts\\python.exe scripts\\add_buzzer_pattern.py --dry-run\n//   .\\.venv\\Scripts\\python.exe scripts\\add_buzzer_pattern.py\n\n/* Example alternative:\n{\n  \"name\": \"Victory Chime\",\n  \"steps\": [\n    { \"frequency\": 900,  \"duration\": 100, \"pauseDuration\": 60 },\n    { \"frequency\": 1200, \"duration\": 120, \"pauseDuration\": 40 },\n    { \"frequency\": 1500, \"duration\": 160, \"pauseDuration\": 0 }\n  ],\n  \"loops\": false\n}\n*/\n\n{\n  \"name\": \"My New Buzzer Pattern\",\n  \"steps\": [\n    { \"frequency\": 1000, \"duration\": 120, \"pauseDuration\": 50 },\n    { \"frequency\": 1400, \"duration\": 120, \"pauseDuration\": 0 }\n  ],\n  \"loops\": false\n}\n"""


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read buzzer_pattern.json, then add the new buzzer pattern."
    )
    parser.add_argument(
        "--config",
        default=str(DEFAULT_CONFIG_PATH),
        help="Path to the buzzer pattern config file (default: scripts/buzzer_pattern.json)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate and preview without changing the firmware files",
    )
    args = parser.parse_args()

    try:
        config_path = Path(args.config).resolve()
        entries = normalize_pattern_entries(load_structured_file(config_path), str(config_path))

        for index, entry in enumerate(entries, start=1):
            name = entry.get("name")
            if not isinstance(name, str) or not name.strip():
                raise PatternScaffoldError(f"Buzzer entry {index} is missing a valid 'name'.")

            steps_data = entry.get("steps", entry.get("pattern", entry.get("tones")))
            if steps_data is None:
                raise PatternScaffoldError(
                    f"Buzzer entry {index} must include 'steps' (or 'pattern' / 'tones')."
                )

            loops_value = entry.get("loops", False)
            loops = loops_value if isinstance(loops_value, bool) else parse_bool(str(loops_value))
            steps = normalize_steps_data(steps_data)
            add_buzzer_pattern(name, steps, loops, args.dry_run)

        if not args.dry_run:
            write_text(config_path, DEFAULT_BUZZER_TEMPLATE, dry_run=False)
            print(f"Reset {config_path.name} back to the starter example.")

        print(f"Processed {len(entries)} buzzer pattern(s) from {config_path.name}.")
        return 0
    except PatternScaffoldError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
