/* Optional fork movement; all velocities are world units per simulation tick. */
#ifndef SOURCE_MOVEMENT_H
#define SOURCE_MOVEMENT_H
#include "cseries.h"
#include "math/real_math.h"
#define SOURCE_UNITS_PER_WORLD 120.f
#define SOURCE_MOVE_SPEED (320.f / SOURCE_UNITS_PER_WORLD / TICKS_PER_SECOND)
#define SOURCE_JUMP_SPEED (268.3281573f / SOURCE_UNITS_PER_WORLD / TICKS_PER_SECOND)
#define SOURCE_GRAVITY (800.f / SOURCE_UNITS_PER_WORLD / (TICKS_PER_SECOND * TICKS_PER_SECOND))
struct damage_data;
void source_movement_reset(void);
void source_movement_update(void);
void source_movement_handle(long machine, word const *data, word size, boolean reliable);
boolean source_movement_player(long player_index);
boolean source_movement_biped(long biped_index);
real source_movement_player_gravity(long player_index, real stock);
real source_movement_self_damage(struct damage_data const *damage, long object_index);
void source_movement_wish(long biped_index, real_vector3d const *forward, real_vector3d *wish);
void source_movement_step(real_vector3d *velocity, real_vector3d const *wish,
    real_vector3d const *normal, boolean airborne, boolean jumping);
#endif
