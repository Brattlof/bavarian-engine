/**
 * @file lua_math.c
 * @brief Math type bindings for Lua
 *
 * Exposes Vec3, Mat4, Quat, etc. to Lua. The tricky part is making these
 * feel like native Lua types while keeping the performance reasonable.
 *
 * We use userdata with metatables for operators. It's not as fast as raw
 * tables but it's way more ergonomic and catches type errors.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/scripting.h>

/* Placeholder - will contain Vec3, Mat4, Quat bindings */
