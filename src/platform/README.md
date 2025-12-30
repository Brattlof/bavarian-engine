# Platform Module

## Purpose

OS abstraction layer providing window management, input handling, timing, file I/O, and threading primitives. The rest of the engine doesn't need to know if it's running on Windows, Linux, or macOS.

## Responsibilities

- **Window**: Creation, resizing, event handling, native handle access
- **Input**: Keyboard, mouse, gamepad polling and events
- **Time**: High-resolution timing, sleep
- **File**: Synchronous file I/O operations
- **Thread**: Thread creation, mutexes, condition variables

## Constraints

- Platform-specific code isolated in subdirectories (`win32/`, `linux/`, `macos/`)
- Public headers are platform-agnostic
- Window operations must be called from main thread
- Threading primitives must match OS semantics (no hidden magic)

## Dependencies

- `core/`: Types and memory

## Files

| File | Description |
|------|-------------|
| `platform.c` | Platform detection, initialization |
| `window.c` | Window management dispatch |
| `input.c` | Input state management |
| `time.c` | Timing utilities dispatch |
| `file.c` | File I/O dispatch |
| `thread.c` | Threading primitives dispatch |

Platform-specific implementations:
- `win32/` - Windows implementations
- `linux/` - Linux/X11 implementations
- `macos/` - macOS/Cocoa implementations

## Usage

```c
#include <bavarian3d/platform.h>
#include <bavarian3d/window.h>
#include <bavarian3d/input.h>

platform_init();

WindowDesc desc = {
    .title = "My Window",
    .width = 1280,
    .height = 720,
};
Window* window = window_create(&desc);

while (!window_should_close(window))
{
    window_poll_events();
    input_update();
    // ... game logic ...
}

window_destroy(window);
platform_shutdown();
```

## Notes

- Native handles from `window_get_native_handle()` are needed for GPU initialization
- Input state is double-buffered; call `input_update()` once per frame
- File operations are synchronous - use threads for async I/O
