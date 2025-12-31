<p align="center">
  <img src="assets/logo.png" alt="Bavarian Engine" width="250"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square&logo=cplusplus" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Windows-lightgrey?style=flat-square&logo=windows" alt="Platform"/>
  <img src="https://img.shields.io/badge/renderer-D3D12-green?style=flat-square" alt="D3D12"/>
  <img src="https://img.shields.io/badge/build-CMake-red?style=flat-square&logo=cmake" alt="CMake"/>
</p>

# Bavarian Engine

A game engine I'm building from scratch. D3D12 renderer, custom ECS, Lua scripting, the works.

## What's in here

- **ECS** - Archetype-based entity component system. Fast iteration, cache-friendly.
- **Renderer** - D3D12 backend with render graph. Vulkan/Metal eventually.
- **Scripting** - Custom Lua compiler and VM. Hot-reload works.
- **Editor** - ImGui-based. Docking, scene hierarchy, inspector, the usual.
- **Platform** - Win32 for now. Linux/Mac later.

## Building

```bash
cmake -B build
cmake --build build
```

## Running

```bash
# Demo with spinning cubes
./build/Debug/bavarian-demo.exe

# Editor
./build/Debug/bavarian-editor.exe
```

## Status

Work in progress. Core systems are functional but there's plenty left to do.
