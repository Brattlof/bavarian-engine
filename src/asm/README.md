# Assembly Module

## Purpose

SIMD-optimized implementations of performance-critical functions. Provides x86_64 (SSE/AVX) and ARM64 (NEON) versions of math and transform operations.

## Responsibilities

- CPU feature detection at runtime
- Function dispatch to fastest available implementation
- Vectorized math operations (vec4, mat4, etc.)
- Batch vertex transformation

## Constraints

- All assembly must comply with platform ABI
- Every function must have documented calling convention
- Alignment requirements must be explicit (typically 16-byte for SIMD)
- No "clever" tricks without benchmarks proving the benefit

## Dependencies

- `core/`: Types only (for dispatch layer)

## Files

| File | Description |
|------|-------------|
| `dispatch.c` | CPU detection, function pointer tables |
| `x86_64/math_sse.S` | SSE math implementations |
| `x86_64/math_avx.S` | AVX math implementations |
| `x86_64/transform.S` | Matrix/vertex transforms |
| `arm64/math_neon.S` | NEON math implementations |
| `arm64/transform.S` | NEON transforms |

## Calling Conventions

### x86_64 System V (Linux/macOS)
- Integer args: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Float/vector args: `xmm0`-`xmm7`
- Return: `rax` (int), `xmm0` (float)
- Caller-saved: `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`-`r11`, `xmm0`-`xmm15`
- Callee-saved: `rbx`, `rbp`, `r12`-`r15`

### x86_64 Microsoft (Windows)
- Integer args: `rcx`, `rdx`, `r8`, `r9`
- Float/vector args: `xmm0`-`xmm3`
- Return: `rax` (int), `xmm0` (float)
- Caller-saved: `rax`, `rcx`, `rdx`, `r8`-`r11`, `xmm0`-`xmm5`
- Callee-saved: `rbx`, `rbp`, `rdi`, `rsi`, `r12`-`r15`, `xmm6`-`xmm15`

### ARM64 AAPCS64
- Args: `x0`-`x7` (int), `v0`-`v7` (float/vector)
- Return: `x0` (int), `v0` (float/vector)
- Caller-saved: `x0`-`x18`, `v0`-`v7`, `v16`-`v31`
- Callee-saved: `x19`-`x28`, `v8`-`v15`

## Usage

The dispatch layer automatically selects the best implementation:

```c
// Dispatch happens internally - just call the standard functions
Vec4 a = vec4(1, 2, 3, 4);
Vec4 b = vec4(5, 6, 7, 8);
Vec4 c = vec4_add(a, b);  // Uses SSE/AVX/NEON if available
```

For direct SIMD calls (if you know alignment is correct):

```c
BAV3D_ALIGN16 Vec4 a = {1, 2, 3, 4};
BAV3D_ALIGN16 Vec4 b = {5, 6, 7, 8};
BAV3D_ALIGN16 Vec4 result;
vec4_add_sse(&result, &a, &b);  // Direct SSE call
```

## Notes

- All SIMD functions require 16-byte aligned arguments
- The scalar C implementations in `core/` serve as fallback and reference
- Profile before optimizing - sometimes the compiler beats hand-written assembly
