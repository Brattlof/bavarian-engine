/**
 * @file loader.c
 * @brief Asset importer implementations
 *
 * This file handles the conversion of source assets (PNG, FBX, Lua, etc.)
 * into the engine's binary formats (.bav_*). In a shipping build, these
 * importers wouldn't even be compiled in - everything would be pre-cooked.
 *
 * For dev builds, we need them for hot-reload and quick iteration.
 * Just don't expect miracles from the FBX parsing - that's a nightmare
 * wrapped in an enigma wrapped in Autodesk's documentation.
 */

#include <bavarian3d/resource.h>
#include <bavarian3d/types.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Texture Import (PNG/JPG/TGA -> .bav_texture)
 *
 * This is a minimal implementation. A real importer would:
 * - Use stb_image or similar for decoding
 * - Support block compression (BC1-BC7)
 * - Generate mipmaps
 * - Handle sRGB conversion
 *
 * For now, we just read raw RGBA data and wrap it in our format.
 * ============================================================================= */

/* Simple TGA loader - TGA is easy to parse and good enough for testing */
typedef struct TgaHeader
{
    u8 id_length;
    u8 colormap_type;
    u8 image_type;
    u16 colormap_origin;
    u16 colormap_length;
    u8 colormap_depth;
    u16 x_origin;
    u16 y_origin;
    u16 width;
    u16 height;
    u8 bits_per_pixel;
    u8 image_descriptor;
} TgaHeader;

static Result load_tga(const char* path, u8** out_data, u32* out_width, u32* out_height)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return RESULT_ERROR_IO;

    TgaHeader header;
    if (fread(&header.id_length, 1, 1, f) != 1 || fread(&header.colormap_type, 1, 1, f) != 1 ||
        fread(&header.image_type, 1, 1, f) != 1 || fread(&header.colormap_origin, 2, 1, f) != 1 ||
        fread(&header.colormap_length, 2, 1, f) != 1 || fread(&header.colormap_depth, 1, 1, f) != 1 ||
        fread(&header.x_origin, 2, 1, f) != 1 || fread(&header.y_origin, 2, 1, f) != 1 ||
        fread(&header.width, 2, 1, f) != 1 || fread(&header.height, 2, 1, f) != 1 ||
        fread(&header.bits_per_pixel, 1, 1, f) != 1 || fread(&header.image_descriptor, 1, 1, f) != 1)
    {
        fclose(f);
        return RESULT_ERROR_IO;
    }

    /* Skip ID field */
    if (header.id_length > 0)
    {
        fseek(f, header.id_length, SEEK_CUR);
    }

    /* Only support uncompressed RGB/RGBA for now */
    if (header.image_type != 2 && header.image_type != 3)
    {
        fclose(f);
        return RESULT_ERROR_UNSUPPORTED;
    }

    if (header.bits_per_pixel != 24 && header.bits_per_pixel != 32)
    {
        fclose(f);
        return RESULT_ERROR_UNSUPPORTED;
    }

    u32 pixel_count = header.width * header.height;
    u32 src_bytes_per_pixel = header.bits_per_pixel / 8;
    u32 src_size = pixel_count * src_bytes_per_pixel;

    u8* src_data = malloc(src_size);
    if (!src_data)
    {
        fclose(f);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    if (fread(src_data, 1, src_size, f) != src_size)
    {
        free(src_data);
        fclose(f);
        return RESULT_ERROR_IO;
    }
    fclose(f);

    /* Convert to RGBA */
    u8* rgba_data = malloc(pixel_count * 4);
    if (!rgba_data)
    {
        free(src_data);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    for (u32 i = 0; i < pixel_count; i++)
    {
        /* TGA is BGR(A) order */
        rgba_data[i * 4 + 0] = src_data[i * src_bytes_per_pixel + 2]; /* R */
        rgba_data[i * 4 + 1] = src_data[i * src_bytes_per_pixel + 1]; /* G */
        rgba_data[i * 4 + 2] = src_data[i * src_bytes_per_pixel + 0]; /* B */
        rgba_data[i * 4 + 3] =
            (src_bytes_per_pixel == 4) ? src_data[i * src_bytes_per_pixel + 3] : 255; /* A */
    }

    /* TGA images are stored bottom-up by default, flip if needed */
    if ((header.image_descriptor & 0x20) == 0)
    {
        /* Flip vertically */
        u32 row_bytes = header.width * 4;
        u8* temp_row = malloc(row_bytes);
        if (temp_row)
        {
            for (u32 y = 0; y < (u32)header.height / 2; y++)
            {
                u8* top = rgba_data + y * row_bytes;
                u8* bottom = rgba_data + (header.height - 1 - y) * row_bytes;
                memcpy(temp_row, top, row_bytes);
                memcpy(top, bottom, row_bytes);
                memcpy(bottom, temp_row, row_bytes);
            }
            free(temp_row);
        }
    }

    free(src_data);

    *out_data = rgba_data;
    *out_width = header.width;
    *out_height = header.height;

    return RESULT_OK;
}

Result bav_import_texture(const char* source_path, const char* output_path, u32 flags)
{
    u8* rgba_data = NULL;
    u32 width = 0, height = 0;

    /* Detect format from extension and load */
    const char* ext = strrchr(source_path, '.');
    if (!ext)
        return RESULT_ERROR_INVALID_ARG;

    if (strcmp(ext, ".tga") == 0 || strcmp(ext, ".TGA") == 0)
    {
        Result r = load_tga(source_path, &rgba_data, &width, &height);
        if (RESULT_FAILED(r))
            return r;
    }
    else
    {
        /* Would use stb_image or similar for PNG/JPG here */
        fprintf(stderr, "Texture import: unsupported format '%s'\n", ext);
        return RESULT_ERROR_UNSUPPORTED;
    }

    /* Build the .bav_texture file */
    FILE* out = fopen(output_path, "wb");
    if (!out)
    {
        free(rgba_data);
        return RESULT_ERROR_IO;
    }

    /* Write asset header */
    BavAssetHeader asset_header = {0};
    asset_header.magic = BAV_ASSET_MAGIC;
    asset_header.version = BAV_ASSET_VERSION_1_0;
    asset_header.asset_type = BAV_ASSET_TEXTURE;
    asset_header.flags = flags;
    asset_header.data_size = sizeof(BavTextureHeader) + (width * height * 4);
    fwrite(&asset_header, sizeof(asset_header), 1, out);

    /* Write texture header */
    BavTextureHeader tex_header = {0};
    tex_header.width = width;
    tex_header.height = height;
    tex_header.depth = 1;
    tex_header.mip_count = 1;
    tex_header.format = (flags & BAV_TEX_FLAG_SRGB) ? BAV_TEX_FORMAT_RGBA8_SRGB : BAV_TEX_FORMAT_RGBA8;
    tex_header.flags = flags;
    tex_header.row_pitch = width * 4;
    tex_header.slice_pitch = width * height * 4;
    tex_header.mip_offsets[0] = 0;
    fwrite(&tex_header, sizeof(tex_header), 1, out);

    /* Write pixel data */
    fwrite(rgba_data, 1, width * height * 4, out);

    fclose(out);
    free(rgba_data);

    return RESULT_OK;
}

/* =============================================================================
 * Mesh Import (FBX/glTF/OBJ -> .bav_mesh)
 *
 * This is where dreams go to die. FBX is a proprietary binary format from
 * Autodesk that's poorly documented. glTF is better but still complex.
 * OBJ is simple but lacks many features.
 *
 * For now, we implement a minimal OBJ loader because life is too short
 * for FBX parsing. Would use Assimp or cgltf in a real implementation.
 * ============================================================================= */

/* Simple OBJ loader - handles v, vt, vn, f only */
typedef struct ObjVertex
{
    f32 position[3];
    f32 normal[3];
    f32 texcoord[2];
} ObjVertex;

static Result load_obj(const char* path, ObjVertex** out_vertices, u32** out_indices,
                       u32* out_vertex_count, u32* out_index_count)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return RESULT_ERROR_IO;

    /* First pass: count elements */
    u32 position_count = 0;
    u32 normal_count = 0;
    u32 texcoord_count = 0;
    u32 face_count = 0;

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == 'v' && line[1] == ' ')
            position_count++;
        else if (line[0] == 'v' && line[1] == 'n')
            normal_count++;
        else if (line[0] == 'v' && line[1] == 't')
            texcoord_count++;
        else if (line[0] == 'f' && line[1] == ' ')
            face_count++;
    }

    if (position_count == 0 || face_count == 0)
    {
        fclose(f);
        return RESULT_ERROR_INVALID_ARG;
    }

    /* Allocate temporary storage for raw OBJ data */
    f32* positions = malloc(position_count * 3 * sizeof(f32));
    f32* normals = normal_count > 0 ? malloc(normal_count * 3 * sizeof(f32)) : NULL;
    f32* texcoords = texcoord_count > 0 ? malloc(texcoord_count * 2 * sizeof(f32)) : NULL;

    if (!positions)
    {
        fclose(f);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    /* Worst case: each face is a triangle with unique vertices */
    u32 max_vertices = face_count * 3;
    ObjVertex* vertices = malloc(max_vertices * sizeof(ObjVertex));
    u32* indices = malloc(max_vertices * sizeof(u32));

    if (!vertices || !indices)
    {
        free(positions);
        free(normals);
        free(texcoords);
        free(vertices);
        free(indices);
        fclose(f);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    /* Second pass: read data */
    rewind(f);

    u32 pos_idx = 0, norm_idx = 0, tex_idx = 0;
    u32 vertex_count = 0, index_count = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == 'v' && line[1] == ' ')
        {
            sscanf(line + 2, "%f %f %f", &positions[pos_idx * 3], &positions[pos_idx * 3 + 1],
                   &positions[pos_idx * 3 + 2]);
            pos_idx++;
        }
        else if (line[0] == 'v' && line[1] == 'n')
        {
            if (normals)
            {
                sscanf(line + 3, "%f %f %f", &normals[norm_idx * 3], &normals[norm_idx * 3 + 1],
                       &normals[norm_idx * 3 + 2]);
                norm_idx++;
            }
        }
        else if (line[0] == 'v' && line[1] == 't')
        {
            if (texcoords)
            {
                sscanf(line + 3, "%f %f", &texcoords[tex_idx * 2], &texcoords[tex_idx * 2 + 1]);
                tex_idx++;
            }
        }
        else if (line[0] == 'f' && line[1] == ' ')
        {
            /* Parse face - support v, v/vt, v/vt/vn, v//vn formats */
            i32 v[4] = {0}, vt[4] = {0}, vn[4] = {0};
            int matches = 0;
            char* ptr = line + 2;

            for (int i = 0; i < 4 && *ptr; i++)
            {
                int scanned = 0;

                /* Try v/vt/vn first */
                if (sscanf(ptr, "%d/%d/%d%n", &v[i], &vt[i], &vn[i], &scanned) >= 3)
                {
                    matches++;
                }
                /* Try v//vn */
                else if (sscanf(ptr, "%d//%d%n", &v[i], &vn[i], &scanned) >= 2)
                {
                    matches++;
                }
                /* Try v/vt */
                else if (sscanf(ptr, "%d/%d%n", &v[i], &vt[i], &scanned) >= 2)
                {
                    matches++;
                }
                /* Try v only */
                else if (sscanf(ptr, "%d%n", &v[i], &scanned) >= 1)
                {
                    matches++;
                }
                else
                {
                    break;
                }

                ptr += scanned;
                while (*ptr == ' ')
                    ptr++;
            }

            /* Triangulate (assumes convex faces) */
            for (int i = 0; i < matches - 2; i++)
            {
                int tri_indices[3] = {0, i + 1, i + 2};

                for (int j = 0; j < 3; j++)
                {
                    int idx = tri_indices[j];
                    ObjVertex* vert = &vertices[vertex_count];

                    /* OBJ indices are 1-based, negative means relative to end */
                    i32 pi = v[idx];
                    if (pi < 0)
                        pi = (i32)position_count + pi + 1;
                    pi--;

                    if (pi >= 0 && pi < (i32)position_count)
                    {
                        vert->position[0] = positions[pi * 3];
                        vert->position[1] = positions[pi * 3 + 1];
                        vert->position[2] = positions[pi * 3 + 2];
                    }

                    if (normals && vn[idx] != 0)
                    {
                        i32 ni = vn[idx];
                        if (ni < 0)
                            ni = (i32)normal_count + ni + 1;
                        ni--;
                        if (ni >= 0 && ni < (i32)normal_count)
                        {
                            vert->normal[0] = normals[ni * 3];
                            vert->normal[1] = normals[ni * 3 + 1];
                            vert->normal[2] = normals[ni * 3 + 2];
                        }
                    }
                    else
                    {
                        vert->normal[0] = 0;
                        vert->normal[1] = 1;
                        vert->normal[2] = 0;
                    }

                    if (texcoords && vt[idx] != 0)
                    {
                        i32 ti = vt[idx];
                        if (ti < 0)
                            ti = (i32)texcoord_count + ti + 1;
                        ti--;
                        if (ti >= 0 && ti < (i32)texcoord_count)
                        {
                            vert->texcoord[0] = texcoords[ti * 2];
                            vert->texcoord[1] = texcoords[ti * 2 + 1];
                        }
                    }
                    else
                    {
                        vert->texcoord[0] = 0;
                        vert->texcoord[1] = 0;
                    }

                    indices[index_count++] = vertex_count++;
                }
            }
        }
    }

    fclose(f);
    free(positions);
    free(normals);
    free(texcoords);

    *out_vertices = vertices;
    *out_indices = indices;
    *out_vertex_count = vertex_count;
    *out_index_count = index_count;

    return RESULT_OK;
}

Result bav_import_mesh(const char* source_path, const char* output_path)
{
    ObjVertex* vertices = NULL;
    u32* indices = NULL;
    u32 vertex_count = 0, index_count = 0;

    const char* ext = strrchr(source_path, '.');
    if (!ext)
        return RESULT_ERROR_INVALID_ARG;

    if (strcmp(ext, ".obj") == 0 || strcmp(ext, ".OBJ") == 0)
    {
        Result r = load_obj(source_path, &vertices, &indices, &vertex_count, &index_count);
        if (RESULT_FAILED(r))
            return r;
    }
    else
    {
        fprintf(stderr, "Mesh import: unsupported format '%s' (only .obj supported)\n", ext);
        return RESULT_ERROR_UNSUPPORTED;
    }

    /* Calculate bounding box */
    f32 bounds_min[3] = {1e30f, 1e30f, 1e30f};
    f32 bounds_max[3] = {-1e30f, -1e30f, -1e30f};

    for (u32 i = 0; i < vertex_count; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (vertices[i].position[j] < bounds_min[j])
                bounds_min[j] = vertices[i].position[j];
            if (vertices[i].position[j] > bounds_max[j])
                bounds_max[j] = vertices[i].position[j];
        }
    }

    /* Calculate bounding sphere (simple - center of AABB, radius to furthest point) */
    f32 center[3] = {(bounds_min[0] + bounds_max[0]) * 0.5f, (bounds_min[1] + bounds_max[1]) * 0.5f,
                     (bounds_min[2] + bounds_max[2]) * 0.5f};
    f32 max_dist_sq = 0;
    for (u32 i = 0; i < vertex_count; i++)
    {
        f32 dx = vertices[i].position[0] - center[0];
        f32 dy = vertices[i].position[1] - center[1];
        f32 dz = vertices[i].position[2] - center[2];
        f32 dist_sq = dx * dx + dy * dy + dz * dz;
        if (dist_sq > max_dist_sq)
            max_dist_sq = dist_sq;
    }
    f32 radius = sqrtf(max_dist_sq);

    /* Write .bav_mesh file */
    FILE* out = fopen(output_path, "wb");
    if (!out)
    {
        free(vertices);
        free(indices);
        return RESULT_ERROR_IO;
    }

    u32 vertex_data_size = vertex_count * sizeof(ObjVertex);
    u32 index_data_size = index_count * sizeof(u32);

    /* Asset header */
    BavAssetHeader asset_header = {0};
    asset_header.magic = BAV_ASSET_MAGIC;
    asset_header.version = BAV_ASSET_VERSION_1_0;
    asset_header.asset_type = BAV_ASSET_MESH;
    asset_header.data_size = sizeof(BavMeshHeader) + vertex_data_size + index_data_size;
    fwrite(&asset_header, sizeof(asset_header), 1, out);

    /* Mesh header */
    BavMeshHeader mesh_header = {0};
    mesh_header.vertex_count = vertex_count;
    mesh_header.index_count = index_count;
    mesh_header.submesh_count = 1; /* Single submesh for now */
    mesh_header.vertex_stride = sizeof(ObjVertex);
    mesh_header.vertex_attributes = BAV_VERTEX_POSITION | BAV_VERTEX_NORMAL | BAV_VERTEX_TEXCOORD0;
    mesh_header.index_format = 1; /* u32 indices */
    memcpy(mesh_header.bounds_min, bounds_min, sizeof(bounds_min));
    memcpy(mesh_header.bounds_max, bounds_max, sizeof(bounds_max));
    mesh_header.bounds_sphere[0] = center[0];
    mesh_header.bounds_sphere[1] = center[1];
    mesh_header.bounds_sphere[2] = center[2];
    mesh_header.bounds_sphere[3] = radius;
    mesh_header.vertex_data_offset = sizeof(BavMeshHeader);
    mesh_header.index_data_offset = sizeof(BavMeshHeader) + vertex_data_size;
    mesh_header.submesh_data_offset =
        sizeof(BavMeshHeader) + vertex_data_size + index_data_size; /* No submesh data yet */
    fwrite(&mesh_header, sizeof(mesh_header), 1, out);

    /* Vertex data */
    fwrite(vertices, sizeof(ObjVertex), vertex_count, out);

    /* Index data */
    fwrite(indices, sizeof(u32), index_count, out);

    fclose(out);
    free(vertices);
    free(indices);

    return RESULT_OK;
}

/* =============================================================================
 * Script Import (Lua -> .bav_script)
 *
 * This just compiles Lua source to bytecode using our Lua compiler.
 * The heavy lifting is done by the scripting module.
 * ============================================================================= */

Result bav_import_script(const char* source_path, const char* output_path, b8 strip_debug)
{
    BAV3D_UNUSED(strip_debug);

    /* Read source file */
    FILE* f = fopen(source_path, "rb");
    if (!f)
        return RESULT_ERROR_IO;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return RESULT_ERROR_IO;
    }

    char* source = malloc((size_t)size + 1);
    if (!source)
    {
        fclose(f);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    fread(source, 1, (size_t)size, f);
    source[size] = '\0';
    fclose(f);

    /* For now, we just store the source as-is.
     * Real implementation would call bav_lua_compile() from the scripting module
     * to produce actual bytecode. */

    FILE* out = fopen(output_path, "wb");
    if (!out)
    {
        free(source);
        return RESULT_ERROR_IO;
    }

    /* Asset header */
    BavAssetHeader asset_header = {0};
    asset_header.magic = BAV_ASSET_MAGIC;
    asset_header.version = BAV_ASSET_VERSION_1_0;
    asset_header.asset_type = BAV_ASSET_SCRIPT;
    asset_header.data_size = sizeof(BavScriptHeader) + (u64)size;
    fwrite(&asset_header, sizeof(asset_header), 1, out);

    /* Script header - for now, bytecode_size is just the source size */
    BavScriptHeader script_header = {0};
    script_header.bytecode_size = (u32)size;
    script_header.debug_info_size = 0;
    script_header.main_function = 0;
    script_header.flags = 0;
    fwrite(&script_header, sizeof(script_header), 1, out);

    /* Write source (would be bytecode in real implementation) */
    fwrite(source, 1, (size_t)size, out);

    fclose(out);
    free(source);

    return RESULT_OK;
}

/* =============================================================================
 * Audio Import (WAV/OGG -> .bav_audio)
 *
 * WAV is straightforward. OGG would need a decoder like stb_vorbis.
 * For now, just WAV support.
 * ============================================================================= */

typedef struct WavHeader
{
    u32 riff_magic;    /* "RIFF" */
    u32 file_size;     /* File size - 8 */
    u32 wave_magic;    /* "WAVE" */
    u32 fmt_magic;     /* "fmt " */
    u32 fmt_size;      /* Format chunk size */
    u16 audio_format;  /* 1 = PCM */
    u16 num_channels;  /* 1 or 2 */
    u32 sample_rate;   /* 44100, 48000, etc */
    u32 byte_rate;     /* sample_rate * num_channels * bits/8 */
    u16 block_align;   /* num_channels * bits/8 */
    u16 bits_per_sample;
} WavHeader;

Result bav_import_audio(const char* source_path, const char* output_path, b8 compress)
{
    BAV3D_UNUSED(compress); /* Would use Vorbis encoder */

    const char* ext = strrchr(source_path, '.');
    if (!ext)
        return RESULT_ERROR_INVALID_ARG;

    if (strcmp(ext, ".wav") != 0 && strcmp(ext, ".WAV") != 0)
    {
        fprintf(stderr, "Audio import: unsupported format '%s' (only .wav supported)\n", ext);
        return RESULT_ERROR_UNSUPPORTED;
    }

    FILE* f = fopen(source_path, "rb");
    if (!f)
        return RESULT_ERROR_IO;

    WavHeader wav;
    if (fread(&wav, sizeof(wav), 1, f) != 1)
    {
        fclose(f);
        return RESULT_ERROR_IO;
    }

    /* Validate WAV magic numbers */
    if (wav.riff_magic != 0x46464952 || /* "RIFF" */
        wav.wave_magic != 0x45564157 || /* "WAVE" */
        wav.fmt_magic != 0x20746D66)    /* "fmt " */
    {
        fclose(f);
        return RESULT_ERROR_INVALID_ARG;
    }

    if (wav.audio_format != 1)
    {
        fprintf(stderr, "Audio import: only PCM WAV supported\n");
        fclose(f);
        return RESULT_ERROR_UNSUPPORTED;
    }

    /* Skip any extra fmt bytes and find data chunk */
    if (wav.fmt_size > 16)
    {
        fseek(f, (long)(wav.fmt_size - 16), SEEK_CUR);
    }

    u32 chunk_id = 0, chunk_size = 0;
    while (fread(&chunk_id, 4, 1, f) == 1 && fread(&chunk_size, 4, 1, f) == 1)
    {
        if (chunk_id == 0x61746164) /* "data" */
            break;
        fseek(f, (long)chunk_size, SEEK_CUR);
    }

    if (chunk_id != 0x61746164)
    {
        fclose(f);
        return RESULT_ERROR_INVALID_ARG;
    }

    /* Read audio data */
    u8* audio_data = malloc(chunk_size);
    if (!audio_data)
    {
        fclose(f);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    if (fread(audio_data, 1, chunk_size, f) != chunk_size)
    {
        free(audio_data);
        fclose(f);
        return RESULT_ERROR_IO;
    }
    fclose(f);

    /* Write .bav_audio file */
    FILE* out = fopen(output_path, "wb");
    if (!out)
    {
        free(audio_data);
        return RESULT_ERROR_IO;
    }

    /* Asset header */
    BavAssetHeader asset_header = {0};
    asset_header.magic = BAV_ASSET_MAGIC;
    asset_header.version = BAV_ASSET_VERSION_1_0;
    asset_header.asset_type = BAV_ASSET_AUDIO;
    asset_header.data_size = sizeof(BavAudioHeader) + chunk_size;
    fwrite(&asset_header, sizeof(asset_header), 1, out);

    /* Audio header */
    BavAudioHeader audio_header = {0};
    audio_header.sample_rate = wav.sample_rate;
    audio_header.channel_count = wav.num_channels;

    switch (wav.bits_per_sample)
    {
        case 8:
            audio_header.format = BAV_AUDIO_FORMAT_PCM_U8;
            break;
        case 16:
            audio_header.format = BAV_AUDIO_FORMAT_PCM_S16;
            break;
        case 24:
            audio_header.format = BAV_AUDIO_FORMAT_PCM_S24;
            break;
        case 32:
            audio_header.format = BAV_AUDIO_FORMAT_PCM_F32;
            break;
        default:
            audio_header.format = BAV_AUDIO_FORMAT_PCM_S16;
    }

    u32 bytes_per_sample = wav.bits_per_sample / 8;
    audio_header.sample_count = chunk_size / (bytes_per_sample * wav.num_channels);
    audio_header.data_size = chunk_size;
    fwrite(&audio_header, sizeof(audio_header), 1, out);

    /* Audio data */
    fwrite(audio_data, 1, chunk_size, out);

    fclose(out);
    free(audio_data);

    return RESULT_OK;
}
