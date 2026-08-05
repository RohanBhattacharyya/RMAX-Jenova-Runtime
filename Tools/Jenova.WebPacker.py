#!/usr/bin/env python3
"""
Jenova Web Module Packer
------------------------
Cross-compiles a project's already-preprocessed Jenova scripts to a WebAssembly side
module and writes the module database the runtime loads at startup.

The editor's desktop build has already done the hard part: it generated the script
proxies in `.jenova/` and worked out every function's parameter and return types. Those
types are the same whatever the target is, so this reuses the desktop database and only
swaps how each symbol is located: a WebAssembly side module has no load address to add an
offset to, so entries carry the symbol name and the runtime resolves them with dlsym.

    python3 Jenova.WebPacker.py <godot-project-path> [-o <output-dir>]
"""

import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from concurrent.futures import ThreadPoolExecutor

MAGIC = b"__JENOVA_CACHE__"
DATABASE_TYPE_OPENSOURCE = 0x534F
DATABASE_VERSION = bytes((0, 3, 9, 9))
INTERPRETER_BACKEND_DIRECT = 3

"""
The module database header is a raw struct, so its layout follows the pointer width of
whoever reads it: its three sizes are size_t. The desktop runtime reads 8-byte fields at a
64-byte header, the WebAssembly runtime reads 4-byte fields at a 52-byte one. A database
written for the Web must use the wasm32 shape or the runtime rejects it as corrupt.
"""
HEADER_SIZE_DESKTOP = 64
HEADER_SIZE_WASM32 = 52

EMXX = shutil.which("em++") or "/usr/lib/emscripten/em++"
LLVM_NM = shutil.which("llvm-nm") or "/opt/emscripten-llvm/bin/llvm-nm"
LLVM_CXXFILT = shutil.which("llvm-cxxfilt") or "/opt/emscripten-llvm/bin/llvm-cxxfilt"


def fail(message):
    print(f"[ x ] {message}", file=sys.stderr)
    sys.exit(1)


def read_module_database(path):
    """Returns the metadata dictionary from a .jdb."""
    data = open(path, "rb").read()
    if data[:16] != MAGIC:
        fail(f"{path} is not a Jenova module database.")
    module_size, metadata_size, _ = struct.unpack_from("<QQQ", data, 16)
    raw = zlib.decompress(data[HEADER_SIZE_DESKTOP:])
    return json.loads(raw[module_size:module_size + metadata_size])


def write_module_database(path, module_bytes, metadata):
    metadata_bytes = json.dumps(metadata, separators=(",", ":"), sort_keys=True).encode()
    payload = zlib.compress(module_bytes + metadata_bytes, 9)
    ratio = (len(payload) / (len(module_bytes) + len(metadata_bytes))) * 100.0

    header = bytearray(HEADER_SIZE_WASM32)
    header[0:16] = MAGIC
    struct.pack_into("<III", header, 16, len(module_bytes), len(metadata_bytes), len(payload))
    struct.pack_into("<f", header, 28, ratio)
    struct.pack_into("<h", header, 32, DATABASE_TYPE_OPENSOURCE)
    header[34:38] = DATABASE_VERSION

    with open(path, "wb") as database:
        database.write(header)
        database.write(payload)
    return ratio


def compile_module(project, jenova_dir, output_wasm, godot_sdk, jenova_sdk, jobs):
    """Compiles every script proxy in .jenova/ and links them into one side module."""
    sources = sorted(f for f in os.listdir(jenova_dir) if f.endswith(".cpp"))
    if not sources:
        fail("No preprocessed scripts found. Run Rebuild Jenova Project in the editor first.")

    # Outside .jenova: the editor's build system scans that directory, and stray objects
    # from another target confuse it.
    object_dir = tempfile.mkdtemp(prefix="jenova-web-")
    flags = ["-std=c++20", "-Oz", "-fPIC", "-w", "-c", "-DNDEBUG", "-DTYPED_METHOD_BIND",
             f"-I{godot_sdk}", f"-I{jenova_sdk}"]

    objects = []
    def build(source):
        target = os.path.join(object_dir, os.path.splitext(source)[0] + ".o")
        objects.append(target)
        return subprocess.run([EMXX, *flags, os.path.join(jenova_dir, source), "-o", target],
                              capture_output=True, text=True), source

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for outcome, source in pool.map(build, sources):
            if outcome.returncode != 0:
                fail(f"Failed to compile {source}:\n{outcome.stderr}")
    print(f"[ + ] Compiled {len(objects)} script(s) for WebAssembly.")

    # godot-cpp is linked in, like libGodot.x64.a is on desktop. The runtime is already
    # loaded globally, so its copies of the interface pointers win over these.
    godot_cpp_archive = os.path.join(godot_sdk, "libGodot.wasm32.a")
    if not os.path.isfile(godot_cpp_archive):
        fail(f"Missing {godot_cpp_archive}. Re-extract the Web distribution package.")
    link = subprocess.run([EMXX, "-Oz", "-sSIDE_MODULE=1", "-sWASM_BIGINT", "-fPIC",
                           *objects, godot_cpp_archive, "-o", output_wasm], capture_output=True, text=True)
    if link.returncode != 0:
        fail(f"Failed to link module:\n{link.stderr}")
    print(f"[ + ] Linked {output_wasm} ({os.path.getsize(output_wasm)} bytes).")


def collect_symbols(module_wasm):
    """Maps `JNV_<uid>::<name>` to the mangled symbol the loader has to resolve."""
    listing = subprocess.run([LLVM_NM, "--defined-only", module_wasm],
                             capture_output=True, text=True)
    mangled = [line.split()[-1] for line in listing.stdout.splitlines() if "JNV_" in line]
    if not mangled:
        fail("Module exports no Jenova script symbols.")

    demangled = subprocess.run([LLVM_CXXFILT], input="\n".join(mangled),
                               capture_output=True, text=True).stdout.splitlines()

    symbols = {}
    entry = re.compile(r"^JNV_([a-f0-9]+)::([A-Za-z_]\w*)")
    for symbol, signature in zip(mangled, demangled):
        match = entry.match(signature)
        if match:
            symbols[f"{match.group(1)}::{match.group(2)}"] = symbol
    return symbols


def retarget_metadata(metadata, symbols, module_size):
    """Replaces every load-address offset with the symbol name to resolve at load time."""
    missing = []
    for uid, script in metadata.get("Scripts", {}).items():
        for name, method in script.get("methods", {}).items():
            symbol = symbols.get(f"{uid}::{name}")
            if not symbol:
                missing.append(f"{uid}::{name}")
                continue
            method.pop("Offset", None)
            method["Symbol"] = symbol
        for name, prop in script.get("properties", {}).items():
            # A script property is reached through a generated `__prop_` pointer global.
            symbol = symbols.get(f"{uid}::__prop_{name}")
            if not symbol:
                missing.append(f"{uid}::__prop_{name}")
                continue
            prop.pop("Offset", None)
            prop["Symbol"] = symbol

    metadata["ModuleBinarySize"] = module_size
    metadata["InterpreterBackend"] = INTERPRETER_BACKEND_DIRECT
    metadata["HasDebugInformation"] = False
    return missing


def main():
    parser = argparse.ArgumentParser(description="Pack Jenova scripts for the Web platform.")
    parser.add_argument("project", help="Path to the Godot project")
    parser.add_argument("-o", "--output", default=None, help="Where to write the packed module (default: <project>/.jenova)")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count(), help="Parallel compile jobs")
    args = parser.parse_args()

    project = os.path.abspath(args.project)
    jenova_dir = os.path.join(project, ".jenova")
    output_dir = os.path.abspath(args.output) if args.output else jenova_dir
    os.makedirs(output_dir, exist_ok=True)

    desktop_database = os.path.join(jenova_dir, "JenovaRuntime.jdb")
    if not os.path.isfile(desktop_database):
        fail("No desktop build found. Run Rebuild Jenova Project in the editor first.")

    # The wasm32 header set, not the desktop one. Godot's opaque types are sized per
    # target (a StringName is 4 bytes on wasm32, 8 on x86-64), so the desktop headers
    # silently corrupt every structure they are shared with.
    godot_sdk = os.path.join(project, "Jenova", "Packages", "GodotSDK-Web")
    jenova_sdk = os.path.join(project, "Jenova", "JenovaSDK")
    if not os.path.isdir(godot_sdk):
        fail("Missing GodotSDK-Web. Extract the Web distribution package into the project;\n"
             "      the desktop GodotSDK cannot be used to build for WebAssembly.")
    if not os.path.isdir(jenova_sdk):
        fail(f"Missing SDK directory: {jenova_sdk}")

    module_wasm = os.path.join(output_dir, "Jenova.Module.wasm")
    compile_module(project, jenova_dir, module_wasm, godot_sdk, jenova_sdk, args.jobs)

    metadata = read_module_database(desktop_database)
    symbols = collect_symbols(module_wasm)
    module_bytes = open(module_wasm, "rb").read()
    missing = retarget_metadata(metadata, symbols, len(module_bytes))
    if missing:
        print(f"[ ! ] {len(missing)} symbol(s) not found in the module: {', '.join(missing[:5])}")

    database_path = os.path.join(output_dir, "JenovaRuntime.web.jdb")
    ratio = write_module_database(database_path, module_bytes, metadata)
    scripts = len(metadata.get("Scripts", {}))
    print(f"[ √ ] {database_path} written ({scripts} script(s), {ratio:.1f}% of raw).")


if __name__ == "__main__":
    main()
