#ifndef FRIENDLY_GRENADES_H
#define FRIENDLY_GRENADES_H

#include "math/real_math.h"
struct collision_result;
void friendly_grenades_reset(void);
void friendly_grenades_update(void);
void friendly_grenades_handle(long machine, word const *data, word size, boolean reliable);
boolean friendly_grenades_collision_test_line(long projectile_index, unsigned long flags,
    real_point3d const *from, real_point3d const *to, long ignored, struct collision_result *hit);
#endif
