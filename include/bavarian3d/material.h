/**
 * @file material.h
 * @brief Material system for surface properties
 *
 * Purpose:
 *   Provides material types for defining surface appearance. Materials
 *   contain parameters like base color, roughness, metallic that control
 *   how surfaces are shaded.
 *
 * Constraints:
 *   - Material data must be 16-byte aligned for GPU upload
 *   - Maximum one material active per draw call currently
 */

#ifndef BAV3D_MATERIAL_H
#define BAV3D_MATERIAL_H

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Material Type
     * ============================================================================= */

    /**
     * Material parameters for PBR-style shading.
     * Currently simplified - full PBR will come with textures.
     */
    typedef struct BAV3D_ALIGN16 Material
    {
        Vec4 base_color;   /* RGBA base color / tint */
        f32 metallic;      /* 0 = dielectric, 1 = metal */
        f32 roughness;     /* 0 = smooth, 1 = rough */
        f32 emission;      /* Emission intensity */
        f32 _pad;          /* Padding to 16-byte alignment */
    } Material;

    /* =============================================================================
     * Material Functions
     * ============================================================================= */

    /**
     * Create a default material (white, non-metallic, medium roughness).
     */
    Material material_default(void);

    /**
     * Create a material with just a base color.
     */
    Material material_from_color(f32 r, f32 g, f32 b, f32 a);

    /**
     * Create a metallic material.
     */
    Material material_metallic(f32 r, f32 g, f32 b, f32 roughness);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_MATERIAL_H */
