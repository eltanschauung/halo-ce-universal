# Co-op enemy projectiles

The host has AI actors and their ballistic solver; clients have units driven
by delayed controls. Reconstructing an AI grenade on a client therefore used
the unit's default throw speed instead of the host's solved launch velocity.
AI vehicle controls were skipped altogether to stop a second vehicle physics
simulation, also dropping their weapon controls. Projectiles were absent from
the object stream, so a missing shot, different bounce or early local explosion
was never corrected. The host still dealt its damage. Plasma grenade warning
audio is an attachment of the projectile, so it could be missing too.

`network_projectiles.c` is an optional fork extension for co-op. The host sends
AI-owned travelling projectiles whose tag's initial speed is at most four world
units per tick (grenades, plasma and vehicle shells). Fast rifle bullets retain
their existing simulation. Player-owned projectiles, player hit reports and
the host's physics, AI solver and damage decisions are unchanged.

- The first complete state is reliable per peer, including for late joins.
  Subsequent complete states are sent each tick in the existing datagram batch.
  Lost states are repaired by the next state, without delaying other messages.
- States carry the actual world-space pose, velocity, projectile flags and
  fuse/arming/range values. A grenade in a hand or stuck to an object follows
  the host's world transform. Clients do not solve a second launch, collide it
  with their differently timed units, or expire its fuse independently.
- The normal object constructor creates its model, lights, particles and
  looping warning sound. Normal tracer cleanup and the original flyby-sound
  test still run. Host impact and explosion effects, including the chosen
  supercombine effect, are reliable events; impacts retain their struck
  object's node attachment when that object exists. Deletion stops the
  projectile's attachments.
- Per-slot spawn serials and event numbers prevent duplication and stale-state
  resurrection. Full local datum handles protect against slot reuse. Allocation
  failure retries on a later state; an orphaned replica is removed after five
  seconds without updates. Switching after a delayed capability reply removes
  old locally reconstructed AI copies.

AI vehicles now receive aiming/firing controls. Their client vehicle physics
still follows the host's transform, through the same unsteered-vehicle test
used by object correction; animation and unit/weapon updates continue.
Player-driven vehicles retain their existing player-input/prediction path.

The core protocol stays at version 32. Private message 255 requires capability
bit 2048, acknowledged through the existing fork handshake. Stock and older
fork peers receive none of this stream and retain their existing path. A fork
client playing on an upstream host also retains that path. Every client needs
this updated build to benefit from the projectile fix.

## Verification

The pre-fix production-function probes reproduced skipped vehicle firing and
different host/client grenade launch velocities. The original object mask
also excluded projectiles. Regression tests compile the actual host snapshot,
receiver, grenade release, vehicle control and flyby functions. They cover
launch velocity, bounces, sticking, fuse/audio lifetime, duplicates, same-tick
impacts, reordered states/events, latency/loss, slot reuse, pool exhaustion,
late joins/negotiation, malformed input and stock/player/hitscan exclusions.
Deliberate faults verify that the checks detect broken behavior.

Native two-peer probes use isolated saves and real b40 grenade/Wraith tags,
the normal object constructor, network sockets, client replicas and terminal
effects. They inspect matched poses, velocities, fuse values and the plasma
grenade's actual looping-sound attachment, followed by host-driven deletion.
They do not substitute a full firefight playtest on separate machines.
