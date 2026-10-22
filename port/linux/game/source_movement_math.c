/* Independently expressed Source movement equations. References and parameter
   choices: docs/source_movement.md. No Valve/FreemanAPI code is linked. */
#include "cseries.h"
#include "game/game.h"
#include "math/real_math.h"
#include "source_movement.h"

void source_movement_step(real_vector3d *velocity, real_vector3d const *wish,
    real_vector3d const *normal, boolean airborne, boolean jumping)
{
    real_vector3d direction = *wish;
    real speed, wishspeed, projection, remaining, acceleration;
    direction.k = 0.f;
    wishspeed = normalize3d(&direction);
    if (!airborne)
    {
        speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j);
        /* Jump before friction: landing with jump held preserves hop momentum. */
        if (speed > 0.f && !jumping)
        {
            real control = MAX(speed, 100.f / SOURCE_UNITS_PER_WORLD / TICKS_PER_SECOND);
            real retained = MAX(0.f, speed - control * 4.f / TICKS_PER_SECOND) / speed;
            velocity->i *= retained;
            velocity->j *= retained;
        }
    }
    if (wishspeed > 0.f)
    {
        projection = velocity->i * direction.i + velocity->j * direction.j;
        remaining = (airborne ? MIN(wishspeed, 30.f / SOURCE_UNITS_PER_WORLD / TICKS_PER_SECOND) : wishspeed) - projection;
        if (remaining > 0.f)
        {
            acceleration = MIN(remaining, 10.f * wishspeed / TICKS_PER_SECOND);
            velocity->i += direction.i * acceleration;
            velocity->j += direction.j * acceleration;
        }
    }
    if (airborne)
        velocity->k -= SOURCE_GRAVITY * 0.5f;
    else if (normal->k > 0.f)
    {
        /* Retain Halo's swept pill and floor contact, including elevators. */
        velocity->k = -(velocity->i * normal->i + velocity->j * normal->j) / normal->k;
        velocity->i -= normal->i / 128.f;
        velocity->j -= normal->j / 128.f;
        velocity->k -= normal->k / 128.f;
    }
}

/* Independently expressed TF2 DamageForce / ApplyPushFromDamage equations.
   Constants and geometry are documented in docs/source_movement.md. */
void source_movement_blast_impulse(real_point3d const *feet, real_point3d const *explosion,
    real crouch, boolean airborne, boolean rocket, real damage, real radius, real_vector3d *impulse)
{
    real_vector3d direction;
    real center_height, force_height, distance, feet_distance, force, scale;
    crouch = MAX(0.f, MIN(crouch, 1.f));
    center_height = (82.f - 20.f * crouch) * 0.5f / SOURCE_UNITS_PER_WORLD;
    force_height = 82.f - 27.f * crouch;
    direction.i = feet->x - explosion->x;
    direction.j = feet->y - explosion->y;
    direction.k = feet->z - explosion->z;
    feet_distance = (real)sqrt(direction.i * direction.i + direction.j * direction.j + direction.k * direction.k);
    direction.k += center_height;
    distance = (real)sqrt(direction.i * direction.i + direction.j * direction.j + direction.k * direction.k);
    distance = MIN(distance, feet_distance);
    /* Valve offsets the inflictor downward 10 units when choosing push direction. */
    direction.k += 10.f / SOURCE_UNITS_PER_WORLD;
    normalize3d(&direction);
    scale = rocket ? (airborne ? 10.f : 5.f) : 9.f;
    damage = MAX(damage, 0.f);
    if (radius > 0.f)
        damage *= 1.f - 0.5f * MIN(distance / radius, 1.f);
    else
        damage = 0.f;
    if (distance > radius) damage = 0.f;
    if (!rocket) damage *= 0.75f;
    force = MIN(1000.f, damage * (82.f / force_height) * scale);
    force /= SOURCE_UNITS_PER_WORLD * TICKS_PER_SECOND;
    impulse->i = direction.i * force;
    impulse->j = direction.j * force;
    impulse->k = direction.k * force;
}

/* TF2 FlPlayerFallDamage with tf_fall_damage_disablespread enabled.
   Return a fraction of maximum health, independent of class/max-health size. */
real source_movement_fall_fraction(real velocity)
{
    real speed = velocity * SOURCE_UNITS_PER_WORLD * TICKS_PER_SECOND;
    return speed > 650.f ? speed / 6000.f : 0.f;
}
