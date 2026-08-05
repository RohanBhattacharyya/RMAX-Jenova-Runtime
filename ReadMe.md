<picture>
<img style="width: 100%; height: auto; display: block;" alt="RMAX-Jenova-Framework-Banner" src="https://github.com/user-attachments/assets/c853df68-0d8b-4ff9-a18b-745516656a2d" />
</picture>

# Jenova Runtime (Core)

This repository contains source code of **Jenova Runtime** for Godot Engine and It's a part of **[Projekt J.E.N.O.V.A](https://github.com/Jenova-Framework/J.E.N.O.V.A)**

![Jenova-Screenshot](https://github.com/user-attachments/assets/9db1d96a-cd2c-4733-9465-1dc434ec1543)

<div align="center">
  <span>
    <img src="https://github.com/Jenova-Framework/Jenova-Runtime/actions/workflows/runtime-windows-x64-build.yml/badge.svg" alt="Jenova Runtime (Windows-x64)">
  </span>
  <span>
    <img src="https://github.com/Jenova-Framework/Jenova-Runtime/actions/workflows/runtime-linux-x64-build.yml/badge.svg" alt="Jenova Runtime (Linux-64)">
  </span>
</div>

## Overview

**Projekt J.E.N.O.V.A** is a comprehensive collection of modular components for the Godot Engine, designed to deliver major enhancements and break through the limitations of current development workflows. By bringing fully-featured C++ scripting directly into the Godot Editor, it enables the use of modern C++20/C++23 standards within Godot in a manner similar to GDScript.

**Jenova Framework** empowers developers to build entire games or applications natively in C/C++ with ease and stability. It supports seamless integration of cutting-edge technologies and allowing direct integration of OpenCV, CUDA, Vulkan, OpenMP and any other modern C++ features seamlessly, All supported by the powerful backend.

For more information and to view the full feature list, check out the details [here](https://github.com/Jenova-Framework/J.E.N.O.V.A#%EF%B8%8F-current-features).

### Jenova Runtime (Core)

This repository contains the full source code of **Jenova Runtime**, a full-featured scripting backend with a wide range of capabilities including a Build System, Compiler Interface, Script Objects, Script Language, Script Instances, Script Interpreter and more.

## Performance

RMAX-Jenova C++ scripts are faster than GDScript everywhere, and are indistinguishable from a hand-written GDExtension once the call is inside your code.

Measured on stock **Godot 4.7.1 Mono** (the Mono build is used so all four languages can be compared in the same binary), 600 physics ticks after a 120-tick warmup, engine run with `--headless --fixed-fps 60` so wall time per tick is CPU work rather than the real-time pacing floor, process pinned with `taskset`, median of 5 runs.

| | **RMAX-Jenova C++** | GDExtension | GDScript | C# |
|---|---|---|---|---|
| Inbound call — engine invoking `_physics_process` | **0.077 µs** | 0.032 µs | 0.171 µs | 2.545 µs |
| Outbound call — script invoking `get_child_count()` | **0.0071 µs** | 0.0074 µs | 0.0230 µs | 0.0293 µs |
| Pure compute — one loop iteration, no engine call | **0.00079 µs** | 0.00071 µs | 0.0149 µs | 0.0038 µs |
| Gameplay script — player movement, ~10 engine calls/tick | **0.405 µs** | 0.317 µs | 0.771 µs | 6.964 µs |

Versus GDScript: **2.2x faster** to enter a script, **3.2x faster** to call back into the engine, **19x faster** at plain computation, **1.9x faster** on a realistic movement script. Versus C#: 33x, 4.1x, 4.8x and 17x respectively.

Disclaimer:

- **Entering** a script is still ~2.4x more expensive than a native GDExtension class. A GDExtension method is a plain virtual call, while a script method has to travel through Godot's `ScriptInstance` interface and back across the GDExtension boundary. Everything after that point is identical. Most people use GDScript or C# as scripts in their games: using C++ through RMAX-Jenova will be faster than that for all use cases, and is still comparable to native GDExtension nonetheless.
- On a script whose cost is dominated by the engine, the language barely matters. 500 `CharacterBody3D` running full `move_and_slide()` cost 6.25 µs per body per tick in RMAX-Jenova C++ against 6.55 µs in GDScript, only 5% apart, because the physics solver is ~85% of the frame.

## Issue/Bug Reports and Questions
- If you want to report an issue or bug create a new thread at [Issues](https://github.com/Jenova-Framework/Jenova-Runtime/issues).
- If you have any questions you can create a new thread at [Discussions](https://github.com/Jenova-Framework/J.E.N.O.V.A/discussions).
- More details on the build process can be found at [Documentation](https://jenova-framework.github.io/docs/pages/Advanced/Build-Guide/).

## Dependencies

Jenova Core has following dependencies :

- [AsmJIT](https://github.com/asmjit/asmjit)
- [LibArchive](https://github.com/libarchive/libarchive)
- [LibLZMA](https://github.com/ShiftMediaProject/liblzma)
- [LibCurl](https://github.com/curl/curl)
- [LibFastZLib](https://github.com/gildor2/fast_zlib)
- [LibTinyCC](http://download.savannah.gnu.org/releases/tinycc/)
- [LibPThread](https://github.com/GerHobbelt/pthread-win32)
- [JSON++](https://github.com/nlohmann/json)
- [FileWatch](https://github.com/ThomasMonkman/filewatch)
- [ArgParse++](https://github.com/p-ranav/argparse)
- [Base64++](https://github.com/zaphoyd/websocketpp/blob/master/websocketpp/base64/base64.hpp)

> [!IMPORTANT]
> - Edit **base64.hpp** namespace to `base64`
> - Only header file `libtcc.h` is required from TinyCC beside static library
> - In **FileWatch.hpp** change `_callback(file.first, file.second);` to `_callback(_path + "/" + file.first, file.second);`
> - **By using Jenova Builder, All the dependencies are downloaded, manipulated and compiled automatically.**

## Build Systems

Jenova Runtime can be built on Windows x64 and Linux x64 using **Jenova Builder**.

**Prerequisites:**

*   Python (3.10+)
*   CMake (3.20+)
*   Ninja (1.11+)
*   Python packages: `pip install requests py7zr colored`

**Windows x64:** Requires Visual Studio (2022+) with C++20 support or the AiO Toolchain.

**Linux x64:** Compatible with Clang++ (18+) and G++ (13+).

For detailed build instructions and more information, see the [Build Guide](https://jenova-framework.github.io/docs/pages/Advanced/Build-Guide).

## Godot Compatibility
As of version 0.4.0.0 LTS, **Godot 4.7 Stable** is the minimum required version due to breaking changes in [godot-cpp](https://github.com/godotengine/godot-cpp). While it is still possible to build 0.4.0.0+ for Godot 4.2–4.6 with minor modifications, the official builder is now fully migrated and fine-tuned for Godot 4.7 only.

## Open Source vs Proprietary

While the public source code of Jenova is ~90% identical to the proprietary version, a few specific features have been removed or disabled.
### These changes include :
- **Jenova Emulator Connector** is removed and will be made available later as an addon in the Package Manager.

- **A.K.I.R.A JIT** is removed from the public source code. This component was responsible for executing obfuscated code using a proprietary highly secured VM.

- **Code Encryption and Key System** has been omitted from the public version to protect critical proprietary algorithms. However, Code Compression is fully included, Developers can add their own encryption on top of the existing buffering system.

- **Jenova Code Virtualizer/Sandbox** removed due to reliance on the proprietary SecureAngel™ 2.0 technology.

### Cross-Platform

Jenova Proprietary version includes only Microsoft Visual C++ (MSVC) and Microsoft LLVM Clang (Clang-cl) compilers and is compatible only with Windows. Open-Source version, however, is fully ported to Linux and includes support for MSVC, Clang-cl, MinGW GCC and LLVM on Windows as well as GCC and LLVM Clang on Linux.

> [!IMPORTANT]  
> The proprietary version has been deprecated and is no longer maintained. All future development efforts are now focused exclusively on the open-source version, which will continue to incorporate enhancements from the previous proprietary releases, with the exception of security-related features.

![RepoBeats](https://repobeats.axiom.co/api/embed/292d48b5da1eb7a8a7db9362fe92577877ec5b51.svg "Repobeats Analytics Image")

----
Developed & Designed By **Hamid.Memar (MemarDesign™ LLC.)**
