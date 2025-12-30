# ==============================================================================
# x86_64 Transform Implementations
# ==============================================================================
# Matrix multiplication and vertex transformation routines.
# These are the real workhorses for 3D rendering - called thousands of times
# per frame when transforming geometry.
#
# The matrix multiply is particularly important because it's O(n) where n is
# the number of vertices. A 2x speedup here directly translates to being able
# to render twice as many vertices at the same framerate.
# ==============================================================================

.intel_syntax noprefix

.text

# ------------------------------------------------------------------------------
# mat4_mul_sse: Multiply two 4x4 matrices
#
# C prototype: void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b)
#
# Inputs:
#   rdi - pointer to result (aligned 16)
#   rsi - pointer to a (aligned 16)
#   rdx - pointer to b (aligned 16)
#
# Column-major layout: each column is a Vec4 stored contiguously.
# Result = A * B, where each column of Result is A * (column of B)
#
# This is one of those functions where the order of operations really matters.
# We're doing 4 matrix-vector multiplies, which is 64 multiplies and 48 adds.
# The SSE version gets it done in about 40 instructions.
# ------------------------------------------------------------------------------
.global mat4_mul_sse
.type mat4_mul_sse, @function
mat4_mul_sse:
    # Load all columns of matrix A
    movaps xmm0, [rsi]          # A column 0
    movaps xmm1, [rsi + 16]     # A column 1
    movaps xmm2, [rsi + 32]     # A column 2
    movaps xmm3, [rsi + 48]     # A column 3

    # Process B column 0
    movaps  xmm4, [rdx]         # B column 0
    pshufd  xmm5, xmm4, 0x00    # Broadcast b[0].x
    pshufd  xmm6, xmm4, 0x55    # Broadcast b[0].y
    pshufd  xmm7, xmm4, 0xAA    # Broadcast b[0].z
    pshufd  xmm4, xmm4, 0xFF    # Broadcast b[0].w

    mulps   xmm5, xmm0          # A col 0 * b[0].x
    mulps   xmm6, xmm1          # A col 1 * b[0].y
    mulps   xmm7, xmm2          # A col 2 * b[0].z
    mulps   xmm4, xmm3          # A col 3 * b[0].w

    addps   xmm5, xmm6
    addps   xmm7, xmm4
    addps   xmm5, xmm7
    movaps  [rdi], xmm5         # Store result column 0

    # Process B column 1
    movaps  xmm4, [rdx + 16]
    pshufd  xmm5, xmm4, 0x00
    pshufd  xmm6, xmm4, 0x55
    pshufd  xmm7, xmm4, 0xAA
    pshufd  xmm4, xmm4, 0xFF

    mulps   xmm5, xmm0
    mulps   xmm6, xmm1
    mulps   xmm7, xmm2
    mulps   xmm4, xmm3

    addps   xmm5, xmm6
    addps   xmm7, xmm4
    addps   xmm5, xmm7
    movaps  [rdi + 16], xmm5

    # Process B column 2
    movaps  xmm4, [rdx + 32]
    pshufd  xmm5, xmm4, 0x00
    pshufd  xmm6, xmm4, 0x55
    pshufd  xmm7, xmm4, 0xAA
    pshufd  xmm4, xmm4, 0xFF

    mulps   xmm5, xmm0
    mulps   xmm6, xmm1
    mulps   xmm7, xmm2
    mulps   xmm4, xmm3

    addps   xmm5, xmm6
    addps   xmm7, xmm4
    addps   xmm5, xmm7
    movaps  [rdi + 32], xmm5

    # Process B column 3
    movaps  xmm4, [rdx + 48]
    pshufd  xmm5, xmm4, 0x00
    pshufd  xmm6, xmm4, 0x55
    pshufd  xmm7, xmm4, 0xAA
    pshufd  xmm4, xmm4, 0xFF

    mulps   xmm5, xmm0
    mulps   xmm6, xmm1
    mulps   xmm7, xmm2
    mulps   xmm4, xmm3

    addps   xmm5, xmm6
    addps   xmm7, xmm4
    addps   xmm5, xmm7
    movaps  [rdi + 48], xmm5

    ret
.size mat4_mul_sse, .-mat4_mul_sse

# ------------------------------------------------------------------------------
# mat4_mul_vec4_sse: Transform a Vec4 by a Mat4
#
# C prototype: void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v)
#
# Inputs:
#   rdi - pointer to result (aligned 16)
#   rsi - pointer to m (aligned 16)
#   rdx - pointer to v (aligned 16)
#
# This is the core of vertex transformation. You'll call this for every
# vertex when transforming a mesh. Make sure your vertex buffers are aligned!
# ------------------------------------------------------------------------------
.global mat4_mul_vec4_sse
.type mat4_mul_vec4_sse, @function
mat4_mul_vec4_sse:
    movaps xmm0, [rdx]          # Load vector v

    # Broadcast each component and multiply by corresponding column
    pshufd xmm1, xmm0, 0x00     # v.x in all lanes
    pshufd xmm2, xmm0, 0x55     # v.y in all lanes
    pshufd xmm3, xmm0, 0xAA     # v.z in all lanes
    pshufd xmm0, xmm0, 0xFF     # v.w in all lanes

    mulps  xmm1, [rsi]          # m.col0 * v.x
    mulps  xmm2, [rsi + 16]     # m.col1 * v.y
    mulps  xmm3, [rsi + 32]     # m.col2 * v.z
    mulps  xmm0, [rsi + 48]     # m.col3 * v.w

    addps  xmm1, xmm2
    addps  xmm3, xmm0
    addps  xmm1, xmm3

    movaps [rdi], xmm1
    ret
.size mat4_mul_vec4_sse, .-mat4_mul_vec4_sse
