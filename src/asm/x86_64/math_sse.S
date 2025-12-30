# ==============================================================================
# x86_64 SSE Math Implementations
# ==============================================================================
# SIMD-optimized vector and matrix math using SSE/SSE2 instructions.
# These are the baseline fast-path implementations - every x86_64 CPU
# supports SSE2 so we don't need a fallback here.
#
# ABI: System V AMD64 (Linux/macOS)
#   - First 6 integer args: rdi, rsi, rdx, rcx, r8, r9
#   - First 8 float args: xmm0-xmm7
#   - Return: rax (int), xmm0 (float)
#   - Callee-saved: rbx, rbp, r12-r15
#   - Caller-saved: rax, rcx, rdx, rsi, rdi, r8-r11, xmm0-xmm15
#
# For Windows (Microsoft x64), we'd need different register assignments.
# That's handled by the _WIN64 preprocessor checks.
# ==============================================================================

.intel_syntax noprefix

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
#   rdi - pointer to result (aligned 16)
#   rsi - pointer to a (aligned 16)
#   rdx - pointer to b (aligned 16)
#
# Straightforward SSE add. The alignment requirement is important - movaps
# will fault on unaligned access. If you're getting crashes, check your
# struct alignment.
# ------------------------------------------------------------------------------
.global vec4_add_sse
.type vec4_add_sse, @function
vec4_add_sse:
    movaps xmm0, [rsi]          # Load a
    addps  xmm0, [rdx]          # Add b
    movaps [rdi], xmm0          # Store result
    ret
.size vec4_add_sse, .-vec4_add_sse

# ------------------------------------------------------------------------------
# vec4_dot_sse: Dot product of two Vec4s
#
# C prototype: float vec4_dot_sse(const Vec4* a, const Vec4* b)
#
# Inputs:
#   rdi - pointer to a (aligned 16)
#   rsi - pointer to b (aligned 16)
# Output:
#   xmm0 - dot product (scalar)
#
# The classic SSE dot product: multiply, horizontal add, shuffle and add again.
# SSE4.1 has dpps which does this in one instruction but we're staying SSE2
# compatible here for maximum compatibility.
# ------------------------------------------------------------------------------
.global vec4_dot_sse
.type vec4_dot_sse, @function
vec4_dot_sse:
    movaps xmm0, [rdi]          # Load a
    mulps  xmm0, [rsi]          # Multiply element-wise: [x*x, y*y, z*z, w*w]

    # Horizontal add: we need to sum all four elements
    # Step 1: xmm1 = [z*z, w*w, z*z, w*w]
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0x4E     # 0b01001110 = swap high and low pairs

    # Step 2: xmm0 = [x+z, y+w, z+x, w+y]
    addps  xmm0, xmm1

    # Step 3: xmm1 = [y+w, x+z, w+y, z+x]
    movaps xmm1, xmm0
    shufps xmm1, xmm1, 0xB1     # 0b10110001 = swap adjacent pairs

    # Step 4: xmm0[0] = x+y+z+w
    addss  xmm0, xmm1           # Only need the scalar result

    ret
.size vec4_dot_sse, .-vec4_dot_sse

# ------------------------------------------------------------------------------
# vec4_normalize_sse: Normalize a Vec4
#
# C prototype: void vec4_normalize_sse(Vec4* result, const Vec4* v)
#
# Inputs:
#   rdi - pointer to result (aligned 16)
#   rsi - pointer to v (aligned 16)
#
# Normalize using rsqrtps (reciprocal sqrt approximation) with Newton-Raphson
# refinement. rsqrtps alone gives ~12 bits of precision which isn't enough
# for graphics work. One NR iteration gets us to ~23 bits which is plenty.
# ------------------------------------------------------------------------------
.global vec4_normalize_sse
.type vec4_normalize_sse, @function
vec4_normalize_sse:
    movaps xmm0, [rsi]          # Load v
    movaps xmm1, xmm0           # Save copy for later

    # Compute dot product (length squared)
    mulps  xmm0, xmm0           # Square each component
    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0x4E
    addps  xmm0, xmm2
    movaps xmm2, xmm0
    shufps xmm2, xmm2, 0xB1
    addps  xmm0, xmm2           # xmm0 = length^2 in all lanes

    # Reciprocal sqrt with Newton-Raphson refinement
    # y = rsqrtps(x)      -- initial approximation
    # y = y * (1.5 - 0.5 * x * y * y)  -- one NR iteration
    rsqrtps xmm2, xmm0          # xmm2 = initial rsqrt

    movaps  xmm3, xmm0          # xmm3 = length^2
    mulps   xmm3, xmm2          # xmm3 = length^2 * y
    mulps   xmm3, xmm2          # xmm3 = length^2 * y^2

    movaps  xmm4, [rip + half]
    mulps   xmm3, xmm4          # xmm3 = 0.5 * length^2 * y^2

    movaps  xmm4, [rip + one_point_five]
    subps   xmm4, xmm3          # xmm4 = 1.5 - 0.5 * length^2 * y^2

    mulps   xmm2, xmm4          # xmm2 = refined rsqrt

    # Multiply original vector by reciprocal length
    mulps   xmm1, xmm2          # xmm1 = normalized vector
    movaps  [rdi], xmm1

    ret
.size vec4_normalize_sse, .-vec4_normalize_sse

# ==============================================================================
# Constants
# ==============================================================================

.section .rodata
.align 16
half:
    .float 0.5, 0.5, 0.5, 0.5
one_point_five:
    .float 1.5, 1.5, 1.5, 1.5
