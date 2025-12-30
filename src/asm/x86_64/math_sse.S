# ==============================================================================
# x86_64 SSE Math Implementations
# ==============================================================================
# SIMD-optimized vector and matrix math using SSE/SSE2 instructions.
# These are the baseline fast-path implementations - every x86_64 CPU
# supports SSE2 so we don't need a fallback here.
#
# This file handles both ABIs:
#   - System V AMD64 (Linux/macOS): args in rdi, rsi, rdx, rcx, r8, r9
#   - Microsoft x64 (Windows): args in rcx, rdx, r8, r9
#
# The macro system below abstracts this away so each function only needs
# one implementation.
# ==============================================================================

.intel_syntax noprefix

# ==============================================================================
# ABI Abstraction Macros
# ==============================================================================
# These macros map logical argument names to the correct registers for each ABI.
# Use ARG1, ARG2, etc. in function bodies instead of raw register names.
# ==============================================================================

#ifdef _WIN64
    # Microsoft x64 ABI
    # Integer args: rcx, rdx, r8, r9
    # Float args: xmm0, xmm1, xmm2, xmm3
    # Shadow space: 32 bytes must be reserved by caller
    .set ARG1, rcx
    .set ARG2, rdx
    .set ARG3, r8
    .set ARG4, r9
#else
    # System V AMD64 ABI (Linux, macOS, BSD)
    # Integer args: rdi, rsi, rdx, rcx, r8, r9
    # Float args: xmm0-xmm7
    # Red zone: 128 bytes below rsp available without allocation
    .set ARG1, rdi
    .set ARG2, rsi
    .set ARG3, rdx
    .set ARG4, rcx
#endif

.text

# ==============================================================================
# Vec4 Operations
# ==============================================================================

# ------------------------------------------------------------------------------
# vec4_add_sse: Add two Vec4s
#
# C prototype: void vec4_add_sse(Vec4* result, const Vec4* a, const Vec4* b)
#
# Inputs:
#   ARG1 - pointer to result (aligned 16)
#   ARG2 - pointer to a (aligned 16)
#   ARG3 - pointer to b (aligned 16)
#
# Straightforward SSE add. The alignment requirement is important - movaps
# will fault on unaligned access. If you're getting crashes, check your
# struct alignment.
# ------------------------------------------------------------------------------
.global vec4_add_sse
#ifndef _WIN64
.type vec4_add_sse, @function
#endif
vec4_add_sse:
    movaps xmm0, [ARG2]          # Load a
    addps  xmm0, [ARG3]          # Add b
    movaps [ARG1], xmm0          # Store result
    ret
#ifndef _WIN64
.size vec4_add_sse, .-vec4_add_sse
#endif

# ------------------------------------------------------------------------------
# vec4_sub_sse: Subtract two Vec4s
#
# C prototype: void vec4_sub_sse(Vec4* result, const Vec4* a, const Vec4* b)
# ------------------------------------------------------------------------------
.global vec4_sub_sse
#ifndef _WIN64
.type vec4_sub_sse, @function
#endif
vec4_sub_sse:
    movaps xmm0, [ARG2]
    subps  xmm0, [ARG3]
    movaps [ARG1], xmm0
    ret
#ifndef _WIN64
.size vec4_sub_sse, .-vec4_sub_sse
#endif

# ------------------------------------------------------------------------------
# vec4_mul_sse: Element-wise multiply two Vec4s
#
# C prototype: void vec4_mul_sse(Vec4* result, const Vec4* a, const Vec4* b)
# ------------------------------------------------------------------------------
.global vec4_mul_sse
#ifndef _WIN64
.type vec4_mul_sse, @function
#endif
vec4_mul_sse:
    movaps xmm0, [ARG2]
    mulps  xmm0, [ARG3]
    movaps [ARG1], xmm0
    ret
#ifndef _WIN64
.size vec4_mul_sse, .-vec4_mul_sse
#endif

# ------------------------------------------------------------------------------
# vec4_scale_sse: Scale a Vec4 by a scalar
#
# C prototype: void vec4_scale_sse(Vec4* result, const Vec4* v, float s)
#
# On System V, the float comes in xmm0
# On Windows, the float comes in xmm2 (third arg is float)
# ------------------------------------------------------------------------------
.global vec4_scale_sse
#ifndef _WIN64
.type vec4_scale_sse, @function
#endif
vec4_scale_sse:
    movaps xmm1, [ARG2]          # Load v
#ifdef _WIN64
    shufps xmm2, xmm2, 0x00      # Broadcast scalar to all lanes
    mulps  xmm1, xmm2
#else
    shufps xmm0, xmm0, 0x00      # Broadcast scalar (in xmm0) to all lanes
    mulps  xmm1, xmm0
#endif
    movaps [ARG1], xmm1
    ret
#ifndef _WIN64
.size vec4_scale_sse, .-vec4_scale_sse
#endif

# ------------------------------------------------------------------------------
# vec4_dot_sse: Dot product of two Vec4s
#
# C prototype: float vec4_dot_sse(const Vec4* a, const Vec4* b)
#
# Inputs:
#   ARG1 - pointer to a (aligned 16)
#   ARG2 - pointer to b (aligned 16)
# Output:
#   xmm0 - dot product (scalar)
#
# The classic SSE dot product: multiply, horizontal add, shuffle and add again.
# SSE4.1 has dpps which does this in one instruction but we're staying SSE2
# compatible here for maximum compatibility.
# ------------------------------------------------------------------------------
.global vec4_dot_sse
#ifndef _WIN64
.type vec4_dot_sse, @function
#endif
vec4_dot_sse:
    movaps xmm0, [ARG1]          # Load a
    mulps  xmm0, [ARG2]          # Multiply element-wise: [x*x, y*y, z*z, w*w]

    # Horizontal add: we need to sum all four elements
    # Step 1: xmm1 = [z*z, w*w, z*z, w*w]
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x4E      # 0b01001110 = swap high and low pairs

    # Step 2: xmm0 = [x+z, y+w, z+x, w+y]
    addps  xmm0, xmm1

    # Step 3: xmm1 = [y+w, x+z, w+y, z+x]
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0xB1      # 0b10110001 = swap adjacent pairs

    # Step 4: xmm0[0] = x+y+z+w
    addss  xmm0, xmm1            # Only need the scalar result

    ret
#ifndef _WIN64
.size vec4_dot_sse, .-vec4_dot_sse
#endif

# ------------------------------------------------------------------------------
# vec4_length_sse: Length of a Vec4
#
# C prototype: float vec4_length_sse(const Vec4* v)
# ------------------------------------------------------------------------------
.global vec4_length_sse
#ifndef _WIN64
.type vec4_length_sse, @function
#endif
vec4_length_sse:
    movaps xmm0, [ARG1]
    mulps  xmm0, xmm0            # Square each component

    # Horizontal sum
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x4E
    addps  xmm0, xmm1
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0xB1
    addss  xmm0, xmm1

    sqrtss xmm0, xmm0            # sqrt of length squared
    ret
#ifndef _WIN64
.size vec4_length_sse, .-vec4_length_sse
#endif

# ------------------------------------------------------------------------------
# vec4_normalize_sse: Normalize a Vec4
#
# C prototype: void vec4_normalize_sse(Vec4* result, const Vec4* v)
#
# Inputs:
#   ARG1 - pointer to result (aligned 16)
#   ARG2 - pointer to v (aligned 16)
#
# Normalize using rsqrtps (reciprocal sqrt approximation) with Newton-Raphson
# refinement. rsqrtps alone gives ~12 bits of precision which isn't enough
# for graphics work. One NR iteration gets us to ~23 bits which is plenty.
# ------------------------------------------------------------------------------
.global vec4_normalize_sse
#ifndef _WIN64
.type vec4_normalize_sse, @function
#endif
vec4_normalize_sse:
    movaps xmm0, [ARG2]          # Load v
    movaps xmm1, xmm0            # Save copy for later

    # Compute dot product (length squared)
    mulps  xmm0, xmm0            # Square each component
    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x4E
    addps  xmm0, xmm2
    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xB1
    addps  xmm0, xmm2            # xmm0 = length^2 in all lanes

    # Reciprocal sqrt with Newton-Raphson refinement
    # y = rsqrtps(x)      -- initial approximation
    # y = y * (1.5 - 0.5 * x * y * y)  -- one NR iteration
    rsqrtps xmm2, xmm0           # xmm2 = initial rsqrt

    movaps  xmm3, xmm0           # xmm3 = length^2
    mulps   xmm3, xmm2           # xmm3 = length^2 * y
    mulps   xmm3, xmm2           # xmm3 = length^2 * y^2

    movaps  xmm4, [rip + const_half]
    mulps   xmm3, xmm4           # xmm3 = 0.5 * length^2 * y^2

    movaps  xmm4, [rip + const_one_point_five]
    subps   xmm4, xmm3           # xmm4 = 1.5 - 0.5 * length^2 * y^2

    mulps   xmm2, xmm4           # xmm2 = refined rsqrt

    # Multiply original vector by reciprocal length
    mulps   xmm1, xmm2           # xmm1 = normalized vector
    movaps  [ARG1], xmm1

    ret
#ifndef _WIN64
.size vec4_normalize_sse, .-vec4_normalize_sse
#endif

# ==============================================================================
# Mat4 Operations
# ==============================================================================
# Mat4 is stored column-major as 4 consecutive Vec4s.
# This matches OpenGL/Vulkan conventions.
# ==============================================================================

# ------------------------------------------------------------------------------
# mat4_mul_sse: Multiply two 4x4 matrices
#
# C prototype: void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b)
#
# This is the most critical function for 3D rendering. Every transform, every
# view matrix, every projection - all go through here. We use a vectorized
# approach where we process one column of the result at a time.
#
# For each result column c, we compute:
#   result.col[c] = a.col[0] * b.col[c].x + a.col[1] * b.col[c].y
#                 + a.col[2] * b.col[c].z + a.col[3] * b.col[c].w
#
# This broadcasts each component of B's column and multiplies by the
# corresponding column of A, then sums them up.
# ------------------------------------------------------------------------------
.global mat4_mul_sse
#ifndef _WIN64
.type mat4_mul_sse, @function
#endif
mat4_mul_sse:
    # Load all columns of matrix A into registers
    # We'll reuse these for each output column
    movaps xmm4, [ARG2]          # A.col[0]
    movaps xmm5, [ARG2 + 16]     # A.col[1]
    movaps xmm6, [ARG2 + 32]     # A.col[2]
    movaps xmm7, [ARG2 + 48]     # A.col[3]

    # --- Column 0 ---
    movaps xmm0, [ARG3]          # B.col[0]

    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x00      # Broadcast x to all lanes
    mulps  xmm1, xmm4            # A.col[0] * B.col[0].x

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x55      # Broadcast y
    mulps  xmm2, xmm5            # A.col[1] * B.col[0].y
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xAA      # Broadcast z
    mulps  xmm2, xmm6            # A.col[2] * B.col[0].z
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xFF      # Broadcast w
    mulps  xmm2, xmm7            # A.col[3] * B.col[0].w
    addps  xmm1, xmm2

    movaps [ARG1], xmm1          # Store result.col[0]

    # --- Column 1 ---
    movaps xmm0, [ARG3 + 16]     # B.col[1]

    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x00
    mulps  xmm1, xmm4

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x55
    mulps  xmm2, xmm5
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xAA
    mulps  xmm2, xmm6
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xFF
    mulps  xmm2, xmm7
    addps  xmm1, xmm2

    movaps [ARG1 + 16], xmm1     # Store result.col[1]

    # --- Column 2 ---
    movaps xmm0, [ARG3 + 32]     # B.col[2]

    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x00
    mulps  xmm1, xmm4

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x55
    mulps  xmm2, xmm5
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xAA
    mulps  xmm2, xmm6
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xFF
    mulps  xmm2, xmm7
    addps  xmm1, xmm2

    movaps [ARG1 + 32], xmm1     # Store result.col[2]

    # --- Column 3 ---
    movaps xmm0, [ARG3 + 48]     # B.col[3]

    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x00
    mulps  xmm1, xmm4

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x55
    mulps  xmm2, xmm5
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xAA
    mulps  xmm2, xmm6
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xFF
    mulps  xmm2, xmm7
    addps  xmm1, xmm2

    movaps [ARG1 + 48], xmm1     # Store result.col[3]

    ret
#ifndef _WIN64
.size mat4_mul_sse, .-mat4_mul_sse
#endif

# ------------------------------------------------------------------------------
# mat4_mul_vec4_sse: Multiply a Mat4 by a Vec4
#
# C prototype: void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v)
#
# result = m.col[0] * v.x + m.col[1] * v.y + m.col[2] * v.z + m.col[3] * v.w
# ------------------------------------------------------------------------------
.global mat4_mul_vec4_sse
#ifndef _WIN64
.type mat4_mul_vec4_sse, @function
#endif
mat4_mul_vec4_sse:
    movaps xmm0, [ARG3]          # Load v

    # Broadcast each component and multiply by corresponding column
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x00      # v.x in all lanes
    mulps  xmm1, [ARG2]          # m.col[0] * v.x

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x55      # v.y in all lanes
    mulps  xmm2, [ARG2 + 16]     # m.col[1] * v.y
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xAA      # v.z in all lanes
    mulps  xmm2, [ARG2 + 32]     # m.col[2] * v.z
    addps  xmm1, xmm2

    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xFF      # v.w in all lanes
    mulps  xmm2, [ARG2 + 48]     # m.col[3] * v.w
    addps  xmm1, xmm2

    movaps [ARG1], xmm1

    ret
#ifndef _WIN64
.size mat4_mul_vec4_sse, .-mat4_mul_vec4_sse
#endif

# ------------------------------------------------------------------------------
# mat4_transpose_sse: Transpose a 4x4 matrix
#
# C prototype: void mat4_transpose_sse(Mat4* result, const Mat4* m)
#
# Uses the classic SSE 4x4 transpose trick with unpacklo/hi and shuffle.
# ------------------------------------------------------------------------------
.global mat4_transpose_sse
#ifndef _WIN64
.type mat4_transpose_sse, @function
#endif
mat4_transpose_sse:
    movaps xmm0, [ARG2]          # col0: a0 a1 a2 a3
    movaps xmm1, [ARG2 + 16]     # col1: b0 b1 b2 b3
    movaps xmm2, [ARG2 + 32]     # col2: c0 c1 c2 c3
    movaps xmm3, [ARG2 + 48]     # col3: d0 d1 d2 d3

    # Interleave rows
    movaps xmm4, xmm0
    unpcklps xmm4, xmm1          # a0 b0 a1 b1
    movaps xmm5, xmm0
    unpckhps xmm5, xmm1          # a2 b2 a3 b3

    movaps xmm6, xmm2
    unpcklps xmm6, xmm3          # c0 d0 c1 d1
    movaps xmm7, xmm2
    unpckhps xmm7, xmm3          # c2 d2 c3 d3

    # Final shuffle to get rows
    movaps xmm0, xmm4
    movlhps xmm0, xmm6           # a0 b0 c0 d0 (row 0)
    movaps [ARG1], xmm0

    movaps xmm0, xmm4
    movhlps xmm0, xmm6           # c1 d1 a1 b1, then we need to fix order
    shufps xmm0, xmm0, 0x4E      # a1 b1 c1 d1 (row 1) - wait this is wrong

    # Let me redo this more carefully
    # After unpcklps xmm4, xmm1: xmm4 = a0 b0 a1 b1
    # After unpcklps xmm6, xmm3: xmm6 = c0 d0 c1 d1
    # movlhps xmm0, xmm6: takes low of xmm0, high of xmm6 -> a0 b0 c0 d0 ✓
    # movhlps xmm0, xmm6: takes high of xmm6, low of xmm0 -> c1 d1 a0 b0 (not what we want)

    # Better approach: use shufps
    movaps xmm0, xmm4
    shufps xmm0, xmm6, 0x44      # a0 b0 c0 d0
    movaps [ARG1], xmm0

    movaps xmm0, xmm4
    shufps xmm0, xmm6, 0xEE      # a1 b1 c1 d1
    movaps [ARG1 + 16], xmm0

    movaps xmm0, xmm5
    shufps xmm0, xmm7, 0x44      # a2 b2 c2 d2
    movaps [ARG1 + 32], xmm0

    movaps xmm0, xmm5
    shufps xmm0, xmm7, 0xEE      # a3 b3 c3 d3
    movaps [ARG1 + 48], xmm0

    ret
#ifndef _WIN64
.size mat4_transpose_sse, .-mat4_transpose_sse
#endif

# ==============================================================================
# Constants
# ==============================================================================

.section .rodata
.align 16
const_half:
    .float 0.5, 0.5, 0.5, 0.5
const_one_point_five:
    .float 1.5, 1.5, 1.5, 1.5
const_one:
    .float 1.0, 1.0, 1.0, 1.0
const_sign_mask:
    .long 0x80000000, 0x80000000, 0x80000000, 0x80000000
