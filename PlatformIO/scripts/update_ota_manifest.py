from pathlib import Path
import re

Import("env")

REPO_OWNER = "MMarsland"
REPO_NAME = "UselessBoxes"

MANIFEST_MAP = {
    "arduino_nano_esp32_michael": ("michael", "michael-firmware.bin"),
    "arduino_nano_esp32_trevor": ("trevor", "trevor-firmware.bin"),
}


def update_manifest(source, target, env):
    pio_env = env.subst("$PIOENV")
    mapping = MANIFEST_MAP.get(pio_env)
    if mapping is None:
        print(f"[ota] No manifest mapping for {pio_env}; skipping.")
        return

    device_name, firmware_filename = mapping
    project_dir = Path(env.subst("$PROJECT_DIR"))
    repo_root = project_dir.parent
    source_file = project_dir / "src" / "Useless_Boxes.cpp"
    manifest_path = repo_root / "ota" / f"{device_name}.txt"

    source_text = source_file.read_text(encoding="utf-8")
    match = re.search(r'CURRENT_FW_VERSION\[\]\s*=\s*"([^"]+)"', source_text)
    if not match:
        raise ValueError(f"[ota] Could not find CURRENT_FW_VERSION in {source_file}")

    version = match.group(1).strip()
    release_url = (
        f"https://github.com/{REPO_OWNER}/{REPO_NAME}/releases/download/"
        f"{version}/{firmware_filename}"
    )
    manifest_contents = f"{version}\n{release_url}\n"

    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    previous_contents = manifest_path.read_text(encoding="utf-8") if manifest_path.exists() else None

    if previous_contents == manifest_contents:
        print(f"[ota] {manifest_path.name} already up to date ({version})")
        return

    manifest_path.write_text(manifest_contents, encoding="utf-8")
    print(f"[ota] Updated {manifest_path.relative_to(repo_root)} -> {version}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", update_manifest)
