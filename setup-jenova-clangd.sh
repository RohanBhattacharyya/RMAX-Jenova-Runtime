#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# Jenova + clangd workspace setup
#
# Usage:
#   ./setup-jenova-clangd.sh
#
# Or:
#   ./setup-jenova-clangd.sh /path/to/project
#
# Optional environment variables:
#   COMPILER=/usr/bin/clang++
#   JOBS=8
# ------------------------------------------------------------

PROJECT_ROOT="${1:-$PWD}"
PROJECT_ROOT="$(cd "$PROJECT_ROOT" && pwd -P)"

JENOVA_DIR="$PROJECT_ROOT/Jenova"
JENOVA_SDK_DIR="$JENOVA_DIR/JenovaSDK"
PACKAGES_DIR="$JENOVA_DIR/Packages"
CLANGD_DB="$PROJECT_ROOT/.clangd-db"
VSCODE_DIR="$PROJECT_ROOT/.vscode"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 8)}"

# ------------------------------------------------------------
# Find compiler
# ------------------------------------------------------------

if [[ -n "${COMPILER:-}" ]]; then
    COMPILER="$(command -v "$COMPILER" 2>/dev/null || echo "$COMPILER")"
else
    COMPILER="$(command -v g++ || true)"
fi

if [[ -z "$COMPILER" || ! -x "$COMPILER" ]]; then
    echo "Error: Could not find a C++ compiler."
    echo "Set one explicitly, for example:"
    echo
    echo "  COMPILER=/usr/bin/g++ $0"
    exit 1
fi

echo "Project root: $PROJECT_ROOT"
echo "Compiler:     $COMPILER"

# ------------------------------------------------------------
# Validate Jenova
# ------------------------------------------------------------

if [[ ! -d "$JENOVA_DIR" ]]; then
    echo "Error: No Jenova directory found:"
    echo "  $JENOVA_DIR"
    echo
    echo "Run this script from the root of a Jenova project."
    exit 1
fi

if [[ ! -d "$JENOVA_SDK_DIR" ]]; then
    echo "Error: Jenova SDK directory not found:"
    echo "  $JENOVA_SDK_DIR"
    exit 1
fi

if [[ ! -f "$JENOVA_SDK_DIR/JenovaSDK.h" ]]; then
    echo "Error: JenovaSDK.h not found:"
    echo "  $JENOVA_SDK_DIR/JenovaSDK.h"
    exit 1
fi

# ------------------------------------------------------------
# Find Godot SDK
# ------------------------------------------------------------

if [[ -d "$PACKAGES_DIR/GodotSDK-Base" ]]; then
    GODOT_SDK="$PACKAGES_DIR/GodotSDK-Base"
else
    GODOT_SDK="$(
        find "$PACKAGES_DIR" \
            -maxdepth 1 \
            -type d \
            -name 'GodotSDK-*' \
            -print \
            -quit 2>/dev/null || true
    )"
fi

if [[ -z "${GODOT_SDK:-}" || ! -d "$GODOT_SDK" ]]; then
    echo "Error: Could not find a GodotSDK package under:"
    echo "  $PACKAGES_DIR"
    exit 1
fi

if [[ ! -d "$GODOT_SDK/Godot" ]]; then
    echo "Error: Godot SDK does not contain a Godot/ directory:"
    echo "  $GODOT_SDK"
    exit 1
fi

echo "Godot SDK:    $GODOT_SDK"
echo "Jobs:         $JOBS"

mkdir -p "$CLANGD_DB"
mkdir -p "$VSCODE_DIR"

# ------------------------------------------------------------
# Generate .clangd
# ------------------------------------------------------------

cat > "$PROJECT_ROOT/.clangd" <<EOF
CompileFlags:
  CompilationDatabase: $CLANGD_DB
  Compiler: $COMPILER
  Add:
    - -std=c++20
    - -DTYPED_METHOD_BIND
    - -DHOT_RELOAD_ENABLED
    - -include
    - $JENOVA_SDK_DIR/JenovaSDK.h
    - -I$PROJECT_ROOT
    - -I$JENOVA_SDK_DIR
    - -I$GODOT_SDK

Index:
  Background: Build

Completion:
  HeaderInsertion: IWYU
  AllScopes: Yes
EOF

echo "Generated .clangd"

# ------------------------------------------------------------
# Generate synthetic compile_commands.json
#
# Every Godot SDK header is treated as its own translation unit.
# This causes clangd's background indexer to fully index the SDK.
# ------------------------------------------------------------

PROJECT_ROOT="$PROJECT_ROOT" \
GODOT_SDK="$GODOT_SDK" \
JENOVA_SDK_DIR="$JENOVA_SDK_DIR" \
COMPILER="$COMPILER" \
CLANGD_DB="$CLANGD_DB" \
python3 <<'PY'
import json
import os
from pathlib import Path

root = Path(os.environ["PROJECT_ROOT"]).resolve()
sdk = Path(os.environ["GODOT_SDK"]).resolve()
jenova_sdk = Path(os.environ["JENOVA_SDK_DIR"]).resolve()
compiler = os.environ["COMPILER"]
output_dir = Path(os.environ["CLANGD_DB"]).resolve()

extensions = {
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
}

headers = sorted(
    p for p in (sdk / "Godot").rglob("*")
    if p.is_file() and p.suffix.lower() in extensions
)

commands = []

for header in headers:
    commands.append({
        "directory": str(root),
        "file": str(header),
        "arguments": [
            compiler,
            "-x", "c++-header",
            "-std=c++20",
            "-DTYPED_METHOD_BIND",
            "-DHOT_RELOAD_ENABLED",
            "-include",
            str(jenova_sdk / "JenovaSDK.h"),
            "-I" + str(root),
            "-I" + str(jenova_sdk),
            "-I" + str(sdk),
            "-c",
            str(header),
        ],
    })

output = output_dir / "compile_commands.json"
output.write_text(
    json.dumps(commands, indent=2),
    encoding="utf-8",
)

print(f"Generated compile_commands.json for {len(commands)} Godot headers")
PY

# ------------------------------------------------------------
# Patch .vscode/settings.json
#
# VS Code settings.json is JSONC, so this parser handles:
#   - // comments
#   - /* comments */
#   - trailing commas
#
# Existing settings are preserved semantically, although formatting/comments
# may be removed. A backup is created before modifying an existing file.
# ------------------------------------------------------------

SETTINGS="$VSCODE_DIR/settings.json"

if [[ -f "$SETTINGS" ]]; then
    BACKUP="$SETTINGS.backup.$(date +%Y%m%d-%H%M%S)"
    cp "$SETTINGS" "$BACKUP"
    echo "Backed up existing settings.json to:"
    echo "  $BACKUP"
fi

SETTINGS="$SETTINGS" JOBS="$JOBS" python3 <<'PY'
import json
import os
import re
from pathlib import Path

settings_path = Path(os.environ["SETTINGS"])
jobs = int(os.environ["JOBS"])


def strip_jsonc(text: str) -> str:
    """
    Remove // and /* */ comments while respecting quoted strings.
    """
    result = []
    i = 0
    in_string = False
    escape = False

    while i < len(text):
        c = text[i]

        if in_string:
            result.append(c)

            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_string = False

            i += 1
            continue

        if c == '"':
            in_string = True
            result.append(c)
            i += 1
            continue

        if c == "/" and i + 1 < len(text):
            nxt = text[i + 1]

            # // comment
            if nxt == "/":
                i += 2
                while i < len(text) and text[i] not in "\r\n":
                    i += 1
                continue

            # /* comment */
            if nxt == "*":
                i += 2
                while i + 1 < len(text):
                    if text[i] == "*" and text[i + 1] == "/":
                        i += 2
                        break
                    i += 1
                continue

        result.append(c)
        i += 1

    return "".join(result)


def remove_trailing_commas(text: str) -> str:
    """
    Remove commas immediately before } or ], while respecting strings.
    """
    result = []
    i = 0
    in_string = False
    escape = False

    while i < len(text):
        c = text[i]

        if in_string:
            result.append(c)

            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_string = False

            i += 1
            continue

        if c == '"':
            in_string = True
            result.append(c)
            i += 1
            continue

        if c == ",":
            j = i + 1
            while j < len(text) and text[j].isspace():
                j += 1

            if j < len(text) and text[j] in "}]":
                i += 1
                continue

        result.append(c)
        i += 1

    return "".join(result)


if settings_path.exists():
    raw = settings_path.read_text(encoding="utf-8")

    try:
        cleaned = remove_trailing_commas(strip_jsonc(raw))
        settings = json.loads(cleaned)
    except Exception as e:
        raise SystemExit(
            f"Could not parse existing {settings_path}: {e}\n"
            "The original file has not been modified."
        )
else:
    settings = {}

settings["C_Cpp.intelliSenseEngine"] = "disabled"

settings["clangd.arguments"] = [
    "--background-index",
    "--header-insertion=iwyu",
    "--header-insertion-decorators",
    "--background-index-priority=normal",
    f"-j={jobs}",
]

settings_path.write_text(
    json.dumps(settings, indent=4) + "\n",
    encoding="utf-8",
)

print("Configured .vscode/settings.json")
PY

# ------------------------------------------------------------
# Done
# ------------------------------------------------------------

echo
echo "Jenova clangd setup complete."
echo
echo "Generated:"
echo "  .clangd"
echo "  .clangd-db/compile_commands.json"
echo "  .vscode/settings.json"
echo
echo "clangd should now background-index the complete Godot SDK."
echo "Restart clangd or reload VS Code if it is already running."
