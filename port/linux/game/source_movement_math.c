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
