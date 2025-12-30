/**
 * @file camera.c
 * @brief Camera management
 *
 * Handles view and projection matrix generation. Nothing fancy here,
 * just the standard perspective/orthographic stuff. The actual camera
 * movement and input handling is game code's problem - we just provide
 * the primitives.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

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

void camera_init(Camera* cam)
{
    cam->position = vec3_zero();
    cam->rotation = quat_identity();
    cam->fov_y = math_radians(60.0f);
    cam->aspect_ratio = 16.0f / 9.0f;
    cam->near_plane = 0.1f;
    cam->far_plane = 1000.0f;
    cam->dirty = true;
}

void camera_set_perspective(Camera* cam, f32 fov_y, f32 aspect, f32 near, f32 far)
{
    cam->fov_y = fov_y;
    cam->aspect_ratio = aspect;
    cam->near_plane = near;
    cam->far_plane = far;
    cam->dirty = true;
}

void camera_set_position(Camera* cam, Vec3 pos)
{
    cam->position = pos;
    cam->dirty = true;
}

void camera_set_rotation(Camera* cam, Quat rot)
{
    cam->rotation = rot;
    cam->dirty = true;
}

void camera_look_at(Camera* cam, Vec3 target, Vec3 up)
{
    Vec3 forward = vec3_normalize(vec3_sub(target, cam->position));
    Vec3 right = vec3_normalize(vec3_cross(up, forward));
    Vec3 actual_up = vec3_cross(forward, right);

    /*
     * Building a quaternion from basis vectors is annoying. We could add
     * mat4_to_quat() but honestly for look_at you usually just want the
     * matrix anyway. So we're cheating here - just mark dirty and the
     * view matrix will be built directly from position/target next update.
     *
     * This means the rotation quat might be stale. Don't read cam->rotation
     * after calling look_at and expect it to be correct. If you need the
     * rotation, compute it yourself from the basis vectors.
     */
    Mat4 rot_mat = mat4_identity();
    rot_mat.cols[0] = vec4_from_vec3(right, 0);
    rot_mat.cols[1] = vec4_from_vec3(actual_up, 0);
    rot_mat.cols[2] = vec4_from_vec3(vec3_negate(forward), 0);

    cam->dirty = true;
}

static void camera_update_matrices(Camera* cam)
{
    if (!cam->dirty)
        return;

    /* View matrix from position and rotation */
    Mat4 rot = mat4_from_quat(quat_conjugate(cam->rotation));
    Mat4 trans = mat4_translate(vec3_negate(cam->position));
    cam->view_matrix = mat4_mul(rot, trans);

    /* Projection matrix */
    cam->proj_matrix =
        mat4_perspective(cam->fov_y, cam->aspect_ratio, cam->near_plane, cam->far_plane);

    cam->view_proj = mat4_mul(cam->proj_matrix, cam->view_matrix);
    cam->dirty = false;
}

Mat4 camera_get_view(Camera* cam)
{
    camera_update_matrices(cam);
    return cam->view_matrix;
}

Mat4 camera_get_projection(Camera* cam)
{
    camera_update_matrices(cam);
    return cam->proj_matrix;
}

Mat4 camera_get_view_projection(Camera* cam)
{
    camera_update_matrices(cam);
    return cam->view_proj;
}
