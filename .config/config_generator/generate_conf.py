import sys
from pathlib import Path


# Folder, w którym znajduje się ten skrypt
BASE_DIR = Path(__file__).resolve().parent

DEFAULTS = BASE_DIR / "../defaults.txt"
TEMPLATE = BASE_DIR / "limine.conf.template"
OUTPUT = BASE_DIR / "../limine.conf"


def parse_defaults(path):
    config = {}
    section = None

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()

            if not line:
                continue

            if line.startswith("#"):
                continue

            if line.startswith("[") and line.endswith("]"):
                section = line[1:-1].strip()
                config.setdefault(section, {})
                continue

            if "=" not in line or section is None:
                continue

            key, value = line.split("=", 1)

            key = key.strip()
            value = value.strip()

            config[section][key] = value

    return config


def get(config, section, key, default=""):
    return config.get(section, {}).get(key, default)


def bool_to_limine(value):
    return "yes" if value.lower() == "true" else "no"


def main():

    print("==> Reading configuration...")

    if not DEFAULTS.exists():
        print(f"ERROR: missing {DEFAULTS}")
        sys.exit(1)

    if not TEMPLATE.exists():
        print(f"ERROR: missing {TEMPLATE}")
        sys.exit(1)

    config = parse_defaults(DEFAULTS)

    resolution = get(
        config,
        "Graphics",
        "Resolution",
        "1280x720"
    )

    timeout = get(
        config,
        "Boot",
        "Timeout",
        "5"
    )

    default_entry = get(
        config,
        "Boot",
        "DefaultEntry",
        "NasuaOS 64-bit"
    )

    verbose = bool_to_limine(
        get(
            config,
            "Boot",
            "Verbose",
            "true"
        )
    )

    wallpaper = get(
        config,
        "Desktop",
        "Wallpaper",
        "/system/assets/images/background.png"
    )

    template = TEMPLATE.read_text(
        encoding="utf-8"
    )

    replacements = {
        "{TIMEOUT}": timeout,
        "{DEFAULT_ENTRY}": default_entry,
        "{VERBOSE}": verbose,
        "{RESOLUTION}": resolution,
        "{WALLPAPER}": wallpaper,
    }

    for key, value in replacements.items():
        template = template.replace(
            key,
            value
        )

    OUTPUT.write_text(
        template,
        encoding="utf-8"
    )

    print(f"Generated {OUTPUT}")


if __name__ == "__main__":
    main()