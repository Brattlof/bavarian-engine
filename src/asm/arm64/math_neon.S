// ==============================================================================
// ARM64 NEON Math Implementations
// ==============================================================================
// SIMD-optimized math using ARM NEON instructions.
// NEON is mandatory in ARM64 (AArch64), so no feature detection needed.
//
// ABI: AAPCS64
//   - First 8 integer args: x0-x7
//   - First 8 float/vector args: v0-v7 (also called q0-q7 for 128-bit)
//   - Return: x0 (int), v0 (float/vector)
//   - Callee-saved: x19-x28, v8-v15
//   - Caller-saved: x0-x18, v0-v7, v16-v31
// ==============================================================================

.text

// ------------------------------------------------------------------------------
// vec4_add_neon: Add two Vec4s
//
// C prototype: void vec4_add_neon(Vec4* result, const Vec4* a, const Vec4* b)
//
// Inputs:
//   x0 - pointer to result (aligned 16)
//   x1 - pointer to a (aligned 16)
//   x2 - pointer to b (aligned 16)
// ------------------------------------------------------------------------------
.global vec4_add_neon
.type vec4_add_neon, %function
vec4_add_neon:
    ldr     q0, [x1]            // Load a
    ldr     q1, [x2]            // Load b
    fadd    v0.4s, v0.4s, v1.4s // Add
    str     q0, [x0]            // Store result
    ret
.size vec4_add_neon, .-vec4_add_neon

// ------------------------------------------------------------------------------
// vec4_dot_neon: Dot product of two Vec4s
//
// C prototype: float vec4_dot_neon(const Vec4* a, const Vec4* b)
//
// Inputs:
//   x0 - pointer to a
//   x1 - pointer to b
// Output:
//   s0 - dot product
//
// NEON has nice horizontal add instructions which make this cleaner than
// the x86 version. faddp does pairwise addition.
// ------------------------------------------------------------------------------
.global vec4_dot_neon
.type vec4_dot_neon, %function
vec4_dot_neon:
    ldr     q0, [x0]            // Load a
    ldr     q1, [x1]            // Load b
    fmul    v0.4s, v0.4s, v1.4s // Multiply element-wise

    // Horizontal add: sum all 4 floats
    faddp   v0.4s, v0.4s, v0.4s // Pairwise add: [x+y, z+w, x+y, z+w]
    faddp   s0, v0.2s           // Pairwise add again: x+y+z+w
    ret
.size vec4_dot_neon, .-vec4_dot_neon

// Placeholder for more NEON implementations
