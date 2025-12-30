/**
 * @file camera.h
 * @brief Camera management interface
 *
 * Purpose:
 *   Provides camera types and functions for view and projection matrix
 *   generation. The camera handles perspective/orthographic projections
 *   and view matrix construction from position and rotation.
 *
 * Constraints:
 *   - Camera matrices are column-major (OpenGL/Vulkan convention)
 *   - FOV is vertical, in radians
 *   - Forward is -Z (right-handed coordinate system)
 */

#ifndef BAV3D_CAMERA_H
#define BAV3D_CAMERA_H

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Camera Type
     * ============================================================================= */

    typedef struct Camera
    {
        Vec3 position;
        Quat rotation;
        f32 fov_y; /* Vertical FOV in radians */
        f32 aspect_ratio;
        f32 near_plane;
        f32 far_plane;

        /* Cached matrices - recomputed when dirty */
        Mat4 view_matrix;
        Mat4 proj_matrix;
        Mat4 view_proj;
        b8 dirty;
    } Camera;

    /* =============================================================================
     * Lifecycle
     * ============================================================================= */

    /**
     * Initialize camera with default values.
     * Position at origin, identity rotation, 60 degree FOV, 16:9 aspect.
     */
    void camera_init(Camera* cam);

    /* =============================================================================
     * Configuration
     * ============================================================================= */

    /**
     * Set perspective projection parameters.
     *
     * @param cam Camera to configure
     * @param fov_y Vertical field of view in radians
     * @param aspect Aspect ratio (width / height)
     * @param near Near clipping plane distance
     * @param far Far clipping plane distance
     */
    void camera_set_perspective(Camera* cam, f32 fov_y, f32 aspect, f32 near, f32 far);

    /**
     * Set camera world position.
     */
    void camera_set_position(Camera* cam, Vec3 pos);

    /**
     * Set camera rotation.
     */
    void camera_set_rotation(Camera* cam, Quat rot);

    /**
     * Orient camera to look at a target point.
     * Note: This may leave cam->rotation stale. If you need the rotation
     * quaternion after calling look_at, compute it from the view matrix.
     */
    void camera_look_at(Camera* cam, Vec3 target, Vec3 up);

    /* =============================================================================
     * Matrix Access
     * ============================================================================= */

    /**
     * Get the view matrix (world to camera transform).
     * Matrices are recomputed lazily when dirty.
     */
    Mat4 camera_get_view(Camera* cam);

    /**
     * Get the projection matrix (camera to clip space).
     */
    Mat4 camera_get_projection(Camera* cam);

    /**
     * Get the combined view-projection matrix.
     * This is what you typically pass to shaders.
     */
    Mat4 camera_get_view_projection(Camera* cam);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_CAMERA_H */
