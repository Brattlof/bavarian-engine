/**
 * @file lua_bindings.c
 * @brief Engine API bindings for Lua
 *
 * This is where we expose engine functionality to Lua scripts. The goal is
 * to make the API feel natural to Lua programmers while being safe and
 * performant.
 *
 * Every function here that talks to the engine needs to handle errors
 * gracefully and never crash, no matter what garbage the script throws at us.
 */

#include <bavarian3d/scripting.h>

/* Placeholder - will contain all the binding registration code */
