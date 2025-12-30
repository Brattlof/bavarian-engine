/**
 * @file material.c
 * @brief Material management
 *
 * Materials define how surfaces look - textures, shaders, blend modes, etc.
 * This is the frontend representation; the actual pipeline objects live in
 * the backend.
 */

#include <bavarian3d/material.h>

Material material_default(void)
{
    Material mat = {0};
    mat.base_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    mat.metallic = 0.0f;
    mat.roughness = 0.5f;
    mat.emission = 0.0f;
    return mat;
}

Material material_from_color(f32 r, f32 g, f32 b, f32 a)
{
    Material mat = {0};
    mat.base_color = vec4(r, g, b, a);
    mat.metallic = 0.0f;
    mat.roughness = 0.5f;
    mat.emission = 0.0f;
    return mat;
}

Material material_metallic(f32 r, f32 g, f32 b, f32 roughness)
{
    Material mat = {0};
    mat.base_color = vec4(r, g, b, 1.0f);
    mat.metallic = 1.0f;
    mat.roughness = roughness;
    mat.emission = 0.0f;
    return mat;
}
