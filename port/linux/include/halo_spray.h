/* Singleplayer image decals. All storage is outside the saved game state. */
#ifndef HALO_SPRAY_H
#define HALO_SPRAY_H

#define HALO_SPRAY_MAXIMUM_VERTICES 3072

struct halo_spray_vertex
{
	float position[3];
	float uv[2];
};

struct halo_spray_clip_vertex
{
	float position[4];
	float uv[2];
};

struct render_camera;
struct render_frustum;
struct collision_result;

void halo_spray_reset(void);
void platform_spray_request(void);
int platform_spray_take_request(void);
void halo_spray_render(short local_player_index, struct render_camera const *camera,
	struct render_frustum const *frustum);
int decal_build_spray_geometry(struct collision_result const *collision, float aspect,
	struct halo_spray_vertex *vertices, int capacity);

/* Native renderer: PNG reload on each successful placement, one texture. */
int halo_spray_image_load(float *aspect);
void halo_spray_image_forget(void);
void halo_spray_image_draw(struct halo_spray_clip_vertex const *vertices, int count);
void halo_spray_draw(struct halo_spray_clip_vertex const *vertices, int count);
int spray_image_load(float *aspect);

#endif
