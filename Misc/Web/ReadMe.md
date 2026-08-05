# Jenova on the Web platform

C++ scripts run in the browser. This describes what to build and in what order.

Two things have to be built once — the runtime and a patched export template — and after
that the Web is a normal export target: same scripts, same project, no per-platform code.

## Why the Web build is different

* **No code generation.** Both interpreter backends (NitroJIT/AsmJIT and Meteora/TinyCC)
  emit x86 machine code, and WebAssembly has nowhere to put it. The Web build uses the
  `Direct` backend instead, which reconstructs the C++ type of each call so the compiler
  emits it — no code is produced at run time. `Direct` works on desktop too, and is
  slightly faster than TinyCC there.
* **No load address.** A side module's code is placed by the dynamic linker and there is no
  base address to add an offset to, so the module database carries symbol *names* and the
  runtime resolves them with `dlsym`.
* **Everything is sized differently.** Godot's opaque types follow the target's pointer
  width: a `StringName` is 4 bytes on wasm32 and 8 on x86-64. The desktop `GodotSDK`
  headers therefore cannot be used for a Web build — they silently shift every member
  offset in godot-cpp's own structures. The Web build generates and ships its own
  `GodotSDK-Web`, and script modules must be compiled against it.
* **No package manager or archive support.** libcurl and libarchive back editor-side
  features and are stubbed out; reaching one logs a warning.

## 1. Build the runtime

Requires Emscripten on `PATH` (`em++`, `emcmake`, `embuilder`).

```
python3 ./Jenova.Builder.py --compiler web-emcc
```

Builds godot-cpp for wasm32 first (into `Libs/GodotSDK-Web` and
`Libs/libgodotcpp-static-wasm32.a`), then the runtime, and writes
`Web/Distribution/Jenova-Framework-Web-emcc.7z`. That package contains
`Jenova.Runtime.Web.wasm`, the `.gdextension`, `JenovaSDK.h`, and the wasm32
`GodotSDK-Web` your scripts will be compiled against.

Extract it over your Godot project, the same way as the desktop package.

## 2. Build the export template

Godot's stock Web templates cannot host Jenova: they have no GDExtension support unless
built with `dlink_enabled=yes`, they are built without exception support, and their
dynamic loader keeps an extension's symbols private so the extension cannot load a module
of its own. `godot-web-gdextension.patch` in this directory fixes the last two.

```
cd <godot-source>
git apply <jenova>/Misc/Web/godot-web-gdextension.patch
scons platform=web target=template_release dlink_enabled=yes threads=no disable_exceptions=no
```

That produces `bin/godot.web.template_release.wasm32.nothreads.dlink.zip`. Point your
export preset's **custom release template** at it, and enable **Extensions Support**.

## 3. Export

Export to Web as you would to any other platform. Jenova's export plugin cross-compiles
your scripts to WebAssembly during the export, packs the Web module database instead of
the desktop one, and Godot ships `Jenova.Runtime.Web.wasm` alongside the page. There is no
separate step and no Web-specific project setup.

Serve the output over HTTP; opening `index.html` from disk will not work.

To run the cross-compile by hand, for a build script or to see its output on its own:

```
python3 <project>/Jenova/Tools/Jenova.WebPacker.py <project>
```

## Developing for both at once

The same `.cpp` is the whole story. A script is written once and runs on desktop and in the
browser unchanged: no `#ifdef`, no second copy, no Web-specific API. Jenova's own SDK and
the parts of godot-cpp your scripts use are the same on both.

The two targets are built at different times, which is the only thing to keep in mind:

* The **desktop** module is built by the editor whenever you build the project, and is what
  you get when you press Play. That is your normal edit-run loop.
* The **Web** module is built during a Web export, from the same sources.

So develop against desktop, and export to Web when you want to check it in a browser. If a
script fails to compile for wasm32 it will fail the export with the compiler's message,
not at run time.

Two things genuinely differ at run time, both because a browser cannot do otherwise:

* Hot reload is desktop only.
* The Package Manager and resource-pack extraction are unavailable, and log a warning if
  something reaches them.

## Testing

`Tools/Jenova.WebTest.js` runs an exported build in headless Chromium and prints what it
logs, exiting non-zero if the marker it waits for never appears or does not say `PASS`:

```
node Tools/Jenova.WebTest.js http://localhost:8000/index.html JENOVA_WEB_TEST 120
```

## Known limitations

* Hot reload is desktop only.
* Threads are off, matching the `nothreads` template.
* The `Direct` backend rebuilds a call from its declared types, which caps a script
  function at four parameters (three when it takes a `Caller*`, and one fewer again when it
  returns a type that comes back through a hidden pointer). A signature that does not fit
  is refused with `ERROR::SIGNATURE_NOT_SUPPORTED_BY_DIRECT_BACKEND` rather than called
  with missing arguments. Desktop can use a JIT backend for those; the Web cannot.
