/* Optional host-driven AI projectiles; upstream's core object stream is unchanged. */
#ifndef NETWORK_PROJECTILES_H
#define NETWORK_PROJECTILES_H
#include "math/real_math.h"
struct object_placement_data;
void network_projectiles_reset(void);
void network_projectiles_machine_joined(long machine);
void network_projectiles_host_tick(void);
void network_projectiles_client_tick(void);
void network_projectiles_handle(long machine, word const *data, word size, boolean reliable);
boolean network_projectiles_suppress(struct object_placement_data const *placement);
boolean network_projectiles_update(long projectile_index);
void network_projectiles_deleted(long projectile_index);
void network_projectiles_effect(long projectile_index, long effect_index, long target_index, short target_node, short count,
    real_point3d const *points, real_vector3d const *forwards, real scale, real material_scale);
boolean network_objects_vehicle_follows_host(long object_index);
#endif
