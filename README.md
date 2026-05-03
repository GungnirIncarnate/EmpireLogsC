# EmpireLogsC

EmpireLogsC is a UE4SS C++ mod for Palworld dedicated servers that tracks PalBox-related activity and writes a persistent guild/player snapshot.

## What It Does

- Hooks PalBox build requests and increments guild camp count.
- Hooks PalBox dismantle requests and decrements guild camp count.
- Hooks player initialization/join flow to keep player-to-guild mapping current.
- Writes state to `EmpireLog.json` in the mod directory.
- On startup, restores previous state from `EmpireLog.json` and can seed guild/member data from `PalDefender/guildexport.json` when present.

## Build (CMake)

From project root:

```powershell
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --config Game__Shipping__Win64
```

Notes:

- Requires Visual Studio C++ build tools and CMake.
- `deps/RE-UE4SS` must be available (submodule initialized).

## Project Layout

- `src/EmpireLogs` - hook handlers and shared state logic
- `include/EmpireLogs` - public headers for mod components
- `src/dllmain.cpp` - UE4SS mod entry point
- `CMakeLists.txt` - top-level build configuration
