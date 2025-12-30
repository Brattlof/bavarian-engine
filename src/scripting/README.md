# Scripting Module

## Purpose

Lua integration for game logic, scene manipulation, and runtime configuration. Provides a sandboxed environment where scripts can interact with the engine safely.

## Responsibilities

- Lua state management with memory limits
- Engine API bindings (math, renderer, input)
- Error handling that doesn't crash the engine
- Type marshaling between C and Lua

## Constraints

- Scripts cannot crash the engine
- Memory usage is bounded and tracked
- Dangerous Lua libraries (io, os) are disabled by default
- All engine objects accessed via handles, not raw pointers

## Dependencies

- `core/`: Types, math
- `renderer/frontend/`: Scene manipulation
- `platform/`: Input state

## Files

| File | Description |
|------|-------------|
| `lua_state.c` | Lua VM management, sandboxing |
| `lua_bindings.c` | Engine API registration |
| `lua_math.c` | Vec3, Mat4, Quat bindings |
| `lua_renderer.c` | Scene, camera, material bindings |
| `lua_input.c` | Input polling bindings |

## Usage

```c
#include <bavarian3d/scripting.h>

ScriptConfig config = {
    .memory_limit = 16 * 1024 * 1024,  // 16 MB
};

ScriptState* script = script_create(&config);
script_load_file(script, "game.lua");

// Game loop
while (running)
{
    script_update(script, delta_time);
}

script_destroy(script);
```

Lua side:

```lua
function init()
    local camera = rend.create_camera()
    camera:set_position(0, 5, 10)
    camera:look_at(0, 0, 0)
end

function update(dt)
    if input.key_pressed("escape") then
        engine.quit()
    end
end
```

## Notes

- Memory limit of 0 means unlimited (not recommended for untrusted scripts)
- Scripts can define `init()` and `update(dt)` functions
- Errors are captured and logged, not propagated as crashes
