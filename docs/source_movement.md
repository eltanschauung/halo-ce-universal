# Fork Source-style movement

`game.source_movement` (default true, `HALO_SOURCE_MOVEMENT`) enables this
fork's on-foot player movement in solo play and on supporting fork hosts.
The host selects it per player using an optional capability. An upstream
player stays stock, including in a mixed lobby. A fork joining an upstream
host stays stock. Older forks without the capability also stay stock.
Set the host's setting false to use stock rules for everyone.

The integration uses Halo's existing swept collision capsule, stairs,
standing clearance, vehicles, moving platforms, animation and 30 Hz clock.
AI, corpses, scripted movement, cutscenes, flying bipeds and vehicle riders
do not take the new movement path. It does not import Source maps' hulls,
water/ladder behavior, class abilities or a different network tick rate.
Consequently this is Source-style movement, not an exact reproduction of
every HL2/TF2 collision or tick-dependent trick.

Walking and running use Halo's existing calculated movement speeds, including
directional speeds, crouching, stun, scripted scaling and double-speed powerups.
Parameters are 10 ground/air acceleration,
4 friction, 100 units/s stop speed, 30 units/s air wish projection cap,
800 units/s/s gravity and a 268.328 units/s jump (TF2's 45-unit jump).
One Halo world unit is 120 Source inches. Engine velocities are converted
to world units per 30 Hz tick, not per rendered frame. Wish velocity comes
from Halo's normal movement calculation, and air movement retains existing
momentum. Held jump hops immediately on landing, without Halo's
landing movement lock or six-ground-tick jump recovery. No extra horizontal
speed is awarded simply for holding forward in the air. Air strafing adds
velocity according to its projection on the wish direction.

Fall-damage checks convert velocity by sqrt(stock gravity / Source gravity),
keeping Halo's fall-distance thresholds without altering actual movement.
Source players skip the airborne maximum-speed instant kill: campaign scripts
can deliberately delay a transition after a drop trigger, as on Keyes.
Landing damage and world kill volumes remain active.

Gravity is split around collision movement. Prediction's ballistic bounds
use the selected player's gravity and jump speed; the existing host-derived
speed envelope and position validation remain in force. Policies use shared
absolute player slots and machine IDs, then bind to local datum handles;
handles themselves are not portable between machines. Upstream protocol 32
and core message layouts are unchanged. The private message (241) and feature
bit (1024) are sent only to peers acknowledging that feature. Joining an older
fork retries its older feature masks without ever probing an upstream host.

For enabled players, their own frag grenade, plasma grenade and rocket
launcher explosion damage is multiplied by 0.6 before shield absorption and
body spillover. Damage to other players, AI, vehicles and unrelated damage
effects keeps its original behavior. A plasma grenade's attached instant-kill
behavior is not made harmless by this numeric damage multiplier.

Self-blast impulses use independently expressed TF2 DamageForce and
ApplyPushFromDamage equations. The reference hull is 48 by 48 by 82 Source
units, crouching lowers its center to a 62-unit hull, and the self-force volume
uses the upstream 55-unit crouched height. Crouch transitions interpolate
between these endpoints using Halo's actual crouch fraction. Push direction
runs from the explosion (offset downward 10 Source units) toward that center,
without Halo's fixed 0.45 upward bias or its living-biped half-force reduction.
Force uses damage times the standing-to-current hull volume ratio, capped at
1000 Source units/s. Grenades use TF2's self-force scale 9 and 0.75 self-grenade
factor. Rockets use Soldier's grounded scale 5 and airborne scale 10. Distance
falloff uses the closer of feet and hull center, declining to half at the
radius. Damage reduction is independent of this force calculation.

Halo explosion damage/radius tags supply the weapon parameters. Existing Halo
blast occlusion and damage admission remain in force; this does not replace
weapons with TF2 projectiles. Halo's swept capsule, 30 Hz clock, maps and player
running speeds remain, so this is not an exact reproduction of a complete
Source game. TF2 and HL2 also have distinct weapon and movement rules.

The blast calculation is negotiated separately (capability bit 4096) through
the existing optional handshake, with retries for older fork feature masks.
No core network message or protocol version changes. Older fork players keep
the previous Halo blast response. New clients replay the normal authoritative
damage event with the same calculation only when their host supports it.
Existing Source air acceleration preserves blast momentum and uses the 30-unit
wish projection cap; it does not clamp total airborne speed to running speed.

## References

The mathematical implementation is independently written for OpenCE's C89
physics interface; no FreemanAPI DLL or Valve SDK code is linked or copied.
These primary sources document the algorithms and the integration design:

- [FreemanAPI](https://github.com/gaycoderprincess/FreemanAPI) and its
  [movement/collision callback interface](https://github.com/gaycoderprincess/FreemanAPI/blob/main/hl_exports.h).
- Valve's [Source movement](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/gamemovement.cpp):
  `Accelerate`, `AirAccelerate`, `Friction`, `StartGravity`, `FinishGravity`.
- Valve's [movement defaults](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/movevars_shared.cpp).
- Valve's [TF2 player damage/force implementation](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/tf/tf_player.cpp)
  (`DamageForce`, `ApplyPushFromDamage`) and [radius damage](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_gamerules.cpp).
- Valve's [TF2 movement](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_gamemovement.cpp)
  for the jump scale; HL2 and TF2 are distinct games, rather than one exact
  movement specification.

## Validation

The Windows release build and 1,456 CPU regression tests passed (two checks
skipped). Native sessions checked fork host/client hops,
a mixed lobby with upstream build-256, joining an upstream host, and co-op
hops after Silent Cartographer's dropship intro. The production fall/ceiling
bound tests include 100 ms and 200 ms round trips; the existing elevator tests
also pass. A controlled retail frag explosion dealt approximately 120 damage
with stock rules and 24 with Source rules, with the same initial upward impulse
of 0.0466667 world units/tick. Android and web SDKs were not configured locally.

## Self-blast validation

The Windows release build and full CPU suite passed (1,531 checks, two
skipped); 62 focused movement, prediction and capability checks include an
additional blast-air-control regression. A controlled retail frag explosion
produced about 0.225 world units/tick upward impulse standing and 0.27778
crouched, versus stock Halo's 0.04666, while health damage remained reduced
from about 120 to 24 at the then-current 80% reduction. The current
self-damage multiplier is 0.6 (40% reduction), independent of blast force. Native matching-fork, mixed-upstream and upstream-host
sessions retained their negotiated movement behavior. A host-authoritative
loose-grenade chain explosion owned by a remote player produced upward speeds
of 0.21094 (host) and 0.21084 (client) at simulated 200 ms RTT, exercising the
existing damage aftermath replay. This is a controlled native probe, not a
claim of exact Source networking or every weapon interaction. Android/web build
targets are unavailable in the local Windows configuration.
