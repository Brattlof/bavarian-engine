# ==============================================================================
# Bavarian Engine - Scripting Subsystem
# ==============================================================================
#
# Purpose:
#   Lua scripting with custom compiler infrastructure.
#   Designed for future multi-language support.
#
# Components:
#   - compiler/  Custom Lua compiler (lexer, parser, analyzer, codegen)
#   - vm/        Bytecode interpreter
#   - bindings/  Engine API bindings for Lua
#
# Dependencies:
#   - core (types, memory)
#   - Lua 5.4 (optional, can use bundled)
#
# ==============================================================================

add_library(bavarian_scripting OBJECT
    # Compiler infrastructure
    compiler/lexer.c
    compiler/parser.c
    compiler/analyzer.c
    compiler/codegen.c

    # Virtual machine
    vm/vm.c
    vm/gc.c
    vm/table.c
    vm/string.c

    # API bindings
    bindings/lua_bindings.c
    bindings/lua_ecs.c
    bindings/lua_math.c
    bindings/lua_renderer.c
    bindings/lua_input.c
    bindings/lua_scene.c

    # Core scripting
    scripting.c
    context.c
)

target_include_directories(bavarian_scripting PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(bavarian_scripting PRIVATE
    $<$<CONFIG:Debug>:BAV_DEBUG=1>
)

if(Lua_FOUND)
    target_include_directories(bavarian_scripting PRIVATE ${LUA_INCLUDE_DIR})
    target_compile_definitions(bavarian_scripting PRIVATE BAV_USE_SYSTEM_LUA=1)
endif()
