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
    b8 use_look_at_view; /* If true, view_matrix was set directly by look_at */
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
    cam->use_look_at_view = false; /* Position changed, need to rebuild view */
    cam->dirty = true;
}

void camera_set_rotation(Camera* cam, Quat rot)
{
    cam->rotation = rot;
    cam->use_look_at_view = false; /* Rotation changed, use quaternion view */
    cam->dirty = true;
}

void camera_look_at(Camera* cam, Vec3 target, Vec3 up)
{
    Vec3 forward = vec3_normalize(vec3_sub(target, cam->position));
    Vec3 right = vec3_normalize(vec3_cross(up, forward));
    Vec3 actual_up = vec3_cross(forward, right);

    /*
     * Build the view matrix directly. The view matrix is the inverse of the
     * camera's world transform. For a look-at camera:
     * - The rotation part transposes the basis vectors into rows
     * - The translation is the negated position dotted with each basis
     */
    Mat4 view = mat4_identity();

    /* Rotation part (transposed basis vectors as rows) */
    view.cols[0].x = right.x;
    view.cols[1].x = right.y;
    view.cols[2].x = right.z;

    view.cols[0].y = actual_up.x;
    view.cols[1].y = actual_up.y;
    view.cols[2].y = actual_up.z;

    view.cols[0].z = -forward.x;
    view.cols[1].z = -forward.y;
    view.cols[2].z = -forward.z;

    /* Translation part */
    view.cols[3].x = -vec3_dot(right, cam->position);
    view.cols[3].y = -vec3_dot(actual_up, cam->position);
    view.cols[3].z = vec3_dot(forward, cam->position);

    cam->view_matrix = view;
    cam->use_look_at_view = true;
    cam->dirty = true;
}

static void camera_update_matrices(Camera* cam)
{
    if (!cam->dirty)
        return;

    /* View matrix - either from look_at or from position/rotation */
    if (!cam->use_look_at_view)
    {
        Mat4 rot = mat4_from_quat(quat_conjugate(cam->rotation));
        Mat4 trans = mat4_translate(vec3_negate(cam->position));
        cam->view_matrix = mat4_mul(rot, trans);
    }
    /* else: view_matrix was already set by camera_look_at */

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
