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
launcher explosion damage is multiplied by 0.2 before shield absorption and
body spillover. Damage to other players, AI, vehicles and unrelated damage
effects keeps its original behavior. Explosion impulse is independent of
this multiplier and is not reduced. Network clients do not rescale the
host's damage replay. A plasma grenade's attached instant-kill behavior is
not made harmless by this numeric damage multiplier.

## References

The mathematical implementation is independently written for OpenCE's C89
physics interface; no FreemanAPI DLL or Valve SDK code is linked or copied.
These primary sources document the algorithms and the integration design:

- [FreemanAPI](https://github.com/gaycoderprincess/FreemanAPI) and its
  [movement/collision callback interface](https://github.com/gaycoderprincess/FreemanAPI/blob/main/hl_exports.h).
- Valve's [Source movement](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/gamemovement.cpp):
  `Accelerate`, `AirAccelerate`, `Friction`, `StartGravity`, `FinishGravity`.
- Valve's [movement defaults](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/movevars_shared.cpp).
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
