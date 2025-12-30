# ==============================================================================
# Test Suite
# ==============================================================================
# Unit and integration tests for the engine.
# We're using a minimal custom test framework because dependencies are annoying.
# ==============================================================================

add_executable(bavarian3d_tests
    test_main.c
    test_math.c
    test_memory.c
    test_arena.c
    test_ecs.c
)

target_include_directories(bavarian3d_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bavarian3d_tests PRIVATE bavarian-runtime)

# Register with CTest
add_test(NAME unit_tests COMMAND bavarian3d_tests)

# SIMD tests - now works on MSVC too thanks to intrinsic implementations
if(BAV_ARCH_X86_64)
    add_executable(bavarian3d_simd_tests
        test_simd.c
    )
    target_include_directories(bavarian3d_simd_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(bavarian3d_simd_tests PRIVATE bavarian-runtime)
    add_test(NAME simd_tests COMMAND bavarian3d_simd_tests)

    # SIMD benchmark - verifies SIMD is faster than scalar
    add_executable(bavarian3d_benchmark
        test_benchmark.c
    )
    target_include_directories(bavarian3d_benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(bavarian3d_benchmark PRIVATE bavarian-runtime)
endif()

# ECS benchmark - verifies optimized iteration is faster than standard
add_executable(bavarian3d_ecs_benchmark
    test_ecs_benchmark.c
)
target_include_directories(bavarian3d_ecs_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(bavarian3d_ecs_benchmark PRIVATE bavarian-runtime)
