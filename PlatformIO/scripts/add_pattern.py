from __future__ import annotations

"""Scaffold RGB and buzzer patterns into the Useless Boxes firmware.

Examples:
  python scripts/add_pattern.py rgb --name Sunset
  python scripts/add_pattern.py rgb --name OceanWave --body-file rgb_body.txt
  python scripts/add_pattern.py buzzer --name Victory --steps-file victory_steps.json --loops true
"""

import argparse
import ast
import json
import re
import sys
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parents[1]
HEADER_PATH = PROJECT_DIR / "include" / "Useless_Boxes.h"
SOURCE_PATH = PROJECT_DIR / "src" / "Useless_Boxes.cpp"


class PatternScaffoldError(RuntimeError):
    pass


def normalize_words(name: str) -> list[str]:
    raw_tokens = re.findall(r"[A-Za-z0-9]+", name)
    words: list[str] = []

    for token in raw_tokens:
        words.extend(
            re.findall(r"[A-Z]+(?=[A-Z][a-z]|\d|$)|[A-Z]?[a-z]+|\d+", token)
        )

    if not words:
        raise PatternScaffoldError("Pattern name must contain letters or digits.")

    return words


def make_enum_name(prefix: str, name: str) -> str:
    return f"{prefix}_{'_'.join(word.upper() for word in normalize_words(name))}"


def make_display_name(name: str) -> str:
    return "_".join(word.upper() for word in normalize_words(name))


def make_function_suffix(name: str) -> str:
    return "".join(word[:1].upper() + word[1:] for word in normalize_words(name))


def parse_bool(value: str) -> bool:
    lowered = value.strip().lower()
    if lowered in {"1", "true", "yes", "y", "on"}:
        return True
    if lowered in {"0", "false", "no", "n", "off"}:
        return False
    raise PatternScaffoldError(f"Could not parse boolean value: {value}")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def strip_comments(raw: str) -> str:
    result: list[str] = []
    in_string = False
    string_char = ""
    i = 0
    length = len(raw)

    while i < length:
        char = raw[i]
        next_char = raw[i + 1] if i + 1 < length else ""

        if in_string:
            result.append(char)
            if char == "\\" and i + 1 < length:
                result.append(raw[i + 1])
                i += 2
                continue
            if char == string_char:
                in_string = False
            i += 1
            continue

        if char in {'"', "'"}:
            in_string = True
            string_char = char
            result.append(char)
            i += 1
            continue

        if char == "/" and next_char == "/":
            while i < length and raw[i] != "\n":
                i += 1
            continue

        if char == "#":
            while i < length and raw[i] != "\n":
                i += 1
            continue

        if char == "/" and next_char == "*":
            i += 2
            while i + 1 < length and not (raw[i] == "*" and raw[i + 1] == "/"):
                i += 1
            i += 2
            continue

        result.append(char)
        i += 1

    return "".join(result)


def parse_structured_text(raw: str, source_name: str):
    text = strip_comments(raw).strip()
    if not text:
        raise PatternScaffoldError(f"{source_name} is empty.")

    try:
        return json.loads(text)
    except json.JSONDecodeError:
        try:
            return ast.literal_eval(text)
        except (SyntaxError, ValueError) as exc:
            raise PatternScaffoldError(
                f"Could not parse {source_name} as JSON or Python literal: {exc}"
            ) from exc


def load_structured_file(path: Path):
    return parse_structured_text(read_text(path), str(path))


def normalize_pattern_entries(data, source_name: str) -> list[dict]:
    if isinstance(data, dict):
        entries = [data]
    elif isinstance(data, list):
        entries = data
    else:
        raise PatternScaffoldError(f"{source_name} must contain an object or a list of objects.")

    if not entries:
        raise PatternScaffoldError(f"{source_name} must contain at least one pattern entry.")

    for index, entry in enumerate(entries, start=1):
        if not isinstance(entry, dict):
            raise PatternScaffoldError(f"Entry {index} in {source_name} must be an object.")

    return entries


def resolve_relative_path(base_file: Path, configured_path: str) -> Path:
    candidate = Path(configured_path)
    if candidate.is_absolute():
        return candidate
    return (base_file.parent / candidate).resolve()


def write_text(path: Path, content: str, dry_run: bool) -> None:
    if dry_run:
        return
    path.write_text(content, encoding="utf-8")


def insert_before_marker(text: str, marker: str, addition: str) -> str:
    if marker not in text:
        raise PatternScaffoldError(f"Could not find marker: {marker}")
    return text.replace(marker, addition + marker, 1)


def append_to_array(text: str, array_signature: str, entry: str) -> str:
    pattern = re.compile(
        rf"({re.escape(array_signature)}\s*\{{\n)(.*?)(\n  \}};)",
        re.DOTALL,
    )
    match = pattern.search(text)
    if not match:
        raise PatternScaffoldError(f"Could not find array: {array_signature}")

    body = match.group(2).rstrip()
    if body and not body.endswith(","):
        body += ","
    updated = f"{match.group(1)}{body}\n{entry}{match.group(3)}"
    return text[: match.start()] + updated + text[match.end() :]


def ensure_missing(text: str, token: str, label: str) -> None:
    if token in text:
        raise PatternScaffoldError(f"{label} '{token}' already exists.")


def indent_body(body: str, spaces: int = 4) -> str:
    prefix = " " * spaces
    lines = body.strip("\n").splitlines() or [""]
    return "\n".join(prefix + line if line else "" for line in lines)


def load_body_text(body: str | None, body_file: str | None) -> str:
    if body and body_file:
        raise PatternScaffoldError("Use either --body or --body-file, not both.")
    if body_file:
        return Path(body_file).read_text(encoding="utf-8")
    if body:
        return body
    return "// TODO: use `now` to compute custom colors.\nreturn makeRGBColor(0, 0, 0);"


def normalize_steps_data(data) -> list[dict[str, int]]:
    if not isinstance(data, list) or not data:
        raise PatternScaffoldError("Steps data must be a non-empty array.")

    normalized: list[dict[str, int]] = []
    for index, item in enumerate(data, start=1):
        if not isinstance(item, dict):
            raise PatternScaffoldError(f"Step {index} must be an object.")

        try:
            frequency = int(item["frequency"])
            duration = int(item["duration"])
            pause_duration = int(item["pauseDuration"])
        except KeyError as exc:
            raise PatternScaffoldError(
                f"Step {index} is missing required key: {exc.args[0]}"
            ) from exc

        if frequency < 0 or duration < 0 or pause_duration < 0:
            raise PatternScaffoldError(f"Step {index} values must be non-negative.")

        normalized.append(
            {
                "frequency": frequency,
                "duration": duration,
                "pauseDuration": pause_duration,
            }
        )

    return normalized


def parse_steps(steps_json: str | None, steps_file: str | None) -> list[dict[str, int]]:
    if steps_json and steps_file:
        raise PatternScaffoldError("Use either --steps or --steps-file, not both.")
    if steps_file:
        data = load_structured_file(Path(steps_file))
    elif steps_json:
        data = parse_structured_text(steps_json, "inline steps")
    else:
        raise PatternScaffoldError("Buzzer patterns require --steps or --steps-file.")

    return normalize_steps_data(data)


def add_rgb_pattern(name: str, body: str, dry_run: bool) -> None:
    enum_name = make_enum_name("RGB", name)
    display_name = make_display_name(name)
    function_name = f"render{make_function_suffix(name)}"

    header_text = read_text(HEADER_PATH)
    source_text = read_text(SOURCE_PATH)

    ensure_missing(header_text, enum_name, "RGB enum")
    ensure_missing(source_text, function_name, "RGB render function")
    ensure_missing(source_text, f'"{display_name}"', "RGB display name")

    header_text = insert_before_marker(
        header_text,
        "  RGB_MODE_COUNT",
        f"  {enum_name},\n",
    )

    function_block = (
        f"  RGBColor {function_name}(unsigned long now) {{\n"
        f"{indent_body(body, 4)}\n"
        f"  }}\n\n"
    )
    source_text = insert_before_marker(
        source_text,
        "  const RGBPatternDefinition RGB_PATTERN_DEFINITIONS[] = {",
        function_block,
    )

    source_text = append_to_array(
        source_text,
        "const RGBPatternDefinition RGB_PATTERN_DEFINITIONS[] =",
        f'    makeRGBPattern("{display_name}", {function_name})',
    )

    write_text(HEADER_PATH, header_text, dry_run)
    write_text(SOURCE_PATH, source_text, dry_run)

    print(f"Added RGB pattern '{display_name}' as {enum_name}.")
    if dry_run:
        print("Dry run only: no files were changed.")


def add_buzzer_pattern(name: str, steps: list[dict[str, int]], loops: bool, dry_run: bool) -> None:
    enum_name = make_enum_name("BUZZER", name)
    display_name = make_display_name(name)
    steps_name = make_enum_name("BUZZER_STEPS", name)

    header_text = read_text(HEADER_PATH)
    source_text = read_text(SOURCE_PATH)

    ensure_missing(header_text, enum_name, "Buzzer enum")
    ensure_missing(source_text, steps_name, "Buzzer steps array")
    ensure_missing(source_text, f'"{display_name}"', "Buzzer display name")

    header_text = insert_before_marker(
        header_text,
        "  BUZZER_PATTERN_COUNT",
        f"  {enum_name},\n",
    )

    step_lines = [
        f'    {{ {step["frequency"]}, {step["duration"]}, {step["pauseDuration"]} }}'
        for step in steps
    ]
    steps_block = (
        f"  const BuzzerToneStep {steps_name}[] = {{\n"
        + ",\n".join(step_lines)
        + "\n  };\n\n"
    )
    source_text = insert_before_marker(
        source_text,
        "  // Add new buzzer patterns in one place: define the step sequence,",
        steps_block,
    )

    loop_text = "true" if loops else "false"
    source_text = append_to_array(
        source_text,
        "const BuzzerPatternDefinition BUZZER_PATTERN_DEFINITIONS[] =",
        f'    {{ "{display_name}", {steps_name}, arrayCount({steps_name}), {loop_text} }}',
    )

    write_text(HEADER_PATH, header_text, dry_run)
    write_text(SOURCE_PATH, source_text, dry_run)

    print(f"Added buzzer pattern '{display_name}' as {enum_name}.")
    if dry_run:
        print("Dry run only: no files were changed.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Scaffold new RGB or buzzer patterns into Useless Boxes firmware."
    )
    subparsers = parser.add_subparsers(dest="pattern_type", required=True)

    rgb_parser = subparsers.add_parser("rgb", help="Add a new RGB pattern")
    rgb_parser.add_argument("--name", required=True, help="Pattern name, e.g. Sunset")
    rgb_parser.add_argument(
        "--body",
        help="C++ function body for `RGBColor renderName(unsigned long now)`",
    )
    rgb_parser.add_argument(
        "--body-file",
        help="Path to a file containing the C++ function body",
    )
    rgb_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview validation without changing files",
    )

    buzzer_parser = subparsers.add_parser("buzzer", help="Add a new buzzer pattern")
    buzzer_parser.add_argument("--name", required=True, help="Pattern name, e.g. Victory")
    buzzer_parser.add_argument(
        "--steps",
        help='JSON array like [{"frequency":1000,"duration":120,"pauseDuration":50}]',
    )
    buzzer_parser.add_argument(
        "--steps-file",
        help="Path to a JSON file containing the step array",
    )
    buzzer_parser.add_argument(
        "--loops",
        default="false",
        help="Whether the buzzer pattern should loop (true/false)",
    )
    buzzer_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview validation without changing files",
    )

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        if args.pattern_type == "rgb":
            body = load_body_text(args.body, args.body_file)
            add_rgb_pattern(args.name, body, args.dry_run)
        elif args.pattern_type == "buzzer":
            steps = parse_steps(args.steps, args.steps_file)
            add_buzzer_pattern(args.name, steps, parse_bool(args.loops), args.dry_run)
        else:
            parser.error(f"Unknown pattern type: {args.pattern_type}")
    except PatternScaffoldError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
