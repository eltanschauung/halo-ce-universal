# Fork co-op damage, chat and scores

Co-op hosts have **Chain-Reaction Damage** in Server Setup > Co-op Options:
Default (100%), 75%, 50% or 25%. This multiplies damage to players from loose
grenade pickups detonated by another damage event. It does not change damage
to enemies, directly thrown grenades, competitive games, blast radius or physics.
The choice persists as `network.coop_chain_reaction_damage` in `config.toml`.

Friendly-fire reductions and chain-reaction reductions multiply once, before
shield absorption and health spillover. For example, 50% friendly-fire damage
and 25% chain damage produce 12.5% damage. Friendly Fire Off still blocks allied
damage. Instant headshots and backstabs cannot bypass reduced friendly fire;
ordinary enemy headshots and the host's authoritative death replays retain their
original behavior. AI allies retain the campaign's existing friendly-fire rules.

Grenade provenance is captured before its item disappears. Full player/object
handles and the team survive deletion of the triggering projectile in a separate
effect-slot cache. Its generation is checked, allocations overwrite it, and map
loads/checkpoint restores clear it. Existing object, effect and checkpoint
structure sizes are unchanged. Chain blasts are host-owned; clients cannot
submit the internal chain or headshot flags in hit reports.

## Chat and commands

The normal rebindable **Chat** action defaults to **Y**. It opens a bottom-right
field during play. Enter sends `profilename : message`; Escape cancels. Tab cycles
case-insensitive command/name completions, and Shift-Tab cycles backwards.
Messages fade after ten seconds; opening chat shows the recent six messages.
Long input scrolls horizontally without truncating the editor's command.

A leading slash runs the existing local console command path, for example
`/viewmodel_fov 90`. The console commands `kill` and `suicide` kill only the
invoking player's current living unit. They also work in singleplayer. A client's
suicide request is executed by the host after checking the authenticated stream,
player handle, current unit and unit-to-player backlink. Public chat text is never
executed as a command by another machine. Requests are length-checked and rate
limited. The primary local player's gameplay/debug input is suppressed while
editing; the game and other local players continue.

## Co-op notices and score

The host sends red death notices naming the victim and, where available, the
killer/player or retail damage cause. Team kills identify the player explicitly.
Causes include Grunts, Elites, Hunters, Jackals, Sentinels, Marines, Combat/Carrier/
Infection Flood, plasma/frag grenades, rockets, Wraith/Scorpion/Ghost/Warthog/Banshee,
vehicle collisions, falling and leaving the world. Unknown custom tags have a
neutral fallback rather than an invented species or weapon.

The co-op Tab scoreboard adds **Score** before Ping:

- Enemy kill: +1.
- Headshot kill: another +1.
- Elite or Hunter: another +1.

An Elite headshot is therefore 3 points. Friendly AI/player kills and suicides
do not award points. Deaths are deduplicated by full unit handle. Scores are
host-authoritative, reset per map/player generation, persist through checkpoint
reverts, and replicate at most once per second. They are separate from campaign
checkpoint data and existing multiplayer game-mode scores.

The core network version and existing message IDs are unchanged. Older peers can
join and ignore the optional new messages; they need this build to display its
chat, killfeed and co-op score or send the new requests. The host alone selects
the chain-damage policy, and authoritative damage still reaches older clients.

## Validation

The CPU harness covers damage scaling, shield/body/parent propagation, instant
headshots, chain ownership after item/projectile deletion, slot reuse, checkpoint
reset, request admission, malformed packets, rate limiting, death causes, scoring,
score identity, editor ownership and completion. Native isolated two-peer probes
exercise real campaign grenade damage, client suicide, chat in both directions,
Elite scoring and score replication. Input probes exercise the real Y binding,
Tab/Enter/Escape pipeline, slash-setting persistence and rendered UI. Runtime
probes stay outside the repository and use isolated saves.
