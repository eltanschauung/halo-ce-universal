/* AI controls are not projectile launch data: an AI's ballistic solver exists
   only on the host. Send its travelling projectiles, including grenades in a
   hand, their bounces/sticking, fuse, effects and deletion. Clients display
   these objects without running a second collision/detonation simulation.
   Player shots and instant bullets retain upstream's prediction/hit reports. */
#include "cseries.h"
#include "game/game.h"
#include "main/main.h"
#include "objects/objects.h"
#include "items/projectiles.h"
#include "items/projectile_definitions.h"
#include "effects/effects.h"
#include "effects/effect_definitions.h"
#include "network_coop.h"
#include "network_distributed.h"
#include "network_extensions.h"
#include "network_projectiles.h"
void render_interpolation_correct_object(long, real_vector3d const *);
#include <string.h>

enum { PROJECTILE_STATE, PROJECTILE_DELETE, PROJECTILE_EFFECT, PROJECTILE_MARKERS = 5 };
/* Tag velocities are already world units per tick. Instant rifle bullets
   cross many world units per tick; grenades, plasma and vehicle shells
   are below this bound. Do not turn hitscan firefights into reliable traffic. */
#define PROJECTILE_MAXIMUM_SPEED 4.f
#define PROJECTILE_TIMEOUT (5 * TICKS_PER_SECOND)

struct projectile_state
{
    unsigned long serial;
    long object_index, definition_index, owner_index;
    real_point3d position;
    real_vector3d forward, up, velocity;
    unsigned long flags;
    real timer, arming, odometer;
};
struct projectile_message
{
    struct distributed_message_header header;
    word operation, reserved;
    struct projectile_state state;
};
struct projectile_effect_message
{
    struct projectile_message base;
    unsigned long event;
    long effect_index;
    long target_index;
    short target_node, count;
    real scale, material_scale;
    real_point3d points[PROJECTILE_MARKERS];
    real_vector3d forwards[PROJECTILE_MARKERS];
};
typedef char projectile_message_size_assert[sizeof(struct projectile_message) == 92 ? 1 : -1];
typedef char projectile_effect_size_assert[sizeof(struct projectile_effect_message) == 236 ? 1 : -1];
struct projectile_host
{
    long object_index;
    unsigned long serial, event;
};
struct projectile_client
{
    long object_index, time, received;
    unsigned long serial, event;
    boolean ended;
};
static struct projectile_host projectile_hosts[MAXIMUM_TRACKED_OBJECTS];
static struct projectile_client projectile_clients[MAXIMUM_TRACKED_OBJECTS];
static unsigned long projectile_known[HALO_PORT_MAXIMUM_NETWORK_MACHINES][MAXIMUM_TRACKED_OBJECTS];
static long projectile_local_slots[MAXIMUM_TRACKED_OBJECTS];
static unsigned long projectile_serial;
static boolean projectile_receiving;
static boolean projectile_stream_started;

short network_distributed_server_machines(long *, short);
void projectile_export_function_values(long);

static boolean projectile_stream_active(void)
{
    return game_in_progress() && !main_menu_is_active() && network_coop_active();
}

static boolean projectile_tag_eligible(long definition_index)
{
    struct projectile_definition const *definition;

    if (!distributed_tag_of_group(definition_index, PROJECTILE_DEFINITION_TAG)) return FALSE;
    definition = projectile_definition_get(definition_index);
    return definition->projectile.initial_velocity >= 0.f &&
        definition->projectile.initial_velocity <= PROJECTILE_MAXIMUM_SPEED;
}

boolean network_projectiles_suppress(struct object_placement_data const *placement)
{
    return !projectile_receiving && game_connection() == _game_connection_network_client &&
        projectile_stream_active() &&
        network_extensions_allows(NONE, _distributed_message_projectiles) &&
        placement->owner_player_index == NONE && projectile_tag_eligible(placement->definition_index);
}

static struct projectile_datum *projectile_try(long index)
{
    return (struct projectile_datum *)object_try_and_get_and_verify_type(index, _object_mask_projectile);
}

/* Every slot reuse has a new serial, even if its 16-bit datum identifier
   eventually wraps. A delayed state never resurrects a deleted projectile. */
static boolean projectile_snapshot(long index, struct projectile_state *state)
{
    struct projectile_datum *projectile = projectile_try(index);
    long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
    struct projectile_host *host;

    if (!projectile || slot >= MAXIMUM_TRACKED_OBJECTS ||
        projectile->object.owner_player_index != NONE ||
        !projectile_tag_eligible(projectile->definition_index)) return FALSE;
    host = &projectile_hosts[slot];
    if (!host->serial || host->object_index != index)
    {
        host->object_index = index;
        host->serial = ++projectile_serial;
        host->event = 0;
    }
    memset(state, 0, sizeof(*state));
    state->serial = host->serial;
    state->object_index = index;
    state->definition_index = projectile->definition_index;
    state->owner_index = projectile->object.owner_object_index;
    object_get_origin(index, &state->position);
    object_get_orientation(index, &state->forward, &state->up);
    state->velocity = projectile->object.translational_velocity;
    state->flags = projectile->projectile.flags & 255;
    state->timer = projectile->projectile.detonation_timer;
    state->arming = projectile->projectile.arming_time;
    state->odometer = projectile->projectile.odometer;
    return TRUE;
}

void network_projectiles_machine_joined(long machine)
{
    if (machine >= 0 && machine < HALO_PORT_MAXIMUM_NETWORK_MACHINES)
        memset(projectile_known[machine], 0, sizeof(projectile_known[machine]));
}

void network_projectiles_reset(void)
{
    /* Objects belong to the game state, not this table. A new game has already
       disposed of the previous map's objects before resetting the netcode. */
    memset(projectile_hosts, 0, sizeof(projectile_hosts));
    memset(projectile_clients, 0, sizeof(projectile_clients));
    memset(projectile_known, 0, sizeof(projectile_known));
    memset(projectile_local_slots, 255, sizeof(projectile_local_slots));
    projectile_serial = 0;
    projectile_receiving = FALSE;
    projectile_stream_started = FALSE;
}

void network_projectiles_host_tick(void)
{
    struct object_iterator iterator;
    struct projectile_message message;
    long machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
    short count, i;

    if (game_connection() != _game_connection_network_server || !projectile_stream_active()) return;
    count = network_distributed_server_machines(machines, NUMBEROF(machines));
    memset(&message, 0, sizeof(message));
    object_iterator_new(&iterator, _object_mask_projectile, 0);
    while (object_iterator_next(&iterator))
    {
        long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.index);

        if (!projectile_snapshot(iterator.index, &message.state)) continue;
        for (i = 0; i < count; i++)
        {
            long machine = machines[i];

            if (machine < 0 || machine >= HALO_PORT_MAXIMUM_NETWORK_MACHINES ||
                !network_extensions_allows(machine, _distributed_message_projectiles)) continue;
            /* Creation is reliable per peer (also for a late join). Subsequent
               complete states can be lost: the next tick repairs them. */
            if (projectile_known[machine][slot] != message.state.serial)
            {
                distributed_send_to_machine_reliably(machine, &message,
                    _distributed_message_projectiles, 1, sizeof(message));
                projectile_known[machine][slot] = message.state.serial;
            }
            else
                distributed_send_to_machine(machine, &message,
                    _distributed_message_projectiles, 1, sizeof(message));
        }
    }
}

void network_projectiles_deleted(long index)
{
    struct projectile_message message;
    long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(index);

    if (game_connection() != _game_connection_network_server || !projectile_stream_active()) return;
    memset(&message, 0, sizeof(message));
    if (!projectile_snapshot(index, &message.state)) return;
    message.operation = PROJECTILE_DELETE;
    distributed_send(&message, _distributed_message_projectiles, 1, sizeof(message), _distributed_to_clients_reliably);
    projectile_hosts[slot].serial = 0;
}

void network_projectiles_effect(long index, long effect_index, long target_index, short target_node, short count,
    real_point3d const *points, real_vector3d const *forwards, real scale, real material_scale)
{
    struct projectile_effect_message message;
    short i;

    if (game_connection() != _game_connection_network_server || !projectile_stream_active() ||
        effect_index == NONE || count < 1 || count > PROJECTILE_MARKERS) return;
    memset(&message, 0, sizeof(message));
    if (!projectile_snapshot(index, &message.base.state)) return;
    message.base.operation = PROJECTILE_EFFECT;
    message.event = ++projectile_hosts[DATUM_INDEX_TO_ABSOLUTE_INDEX(index)].event;
    message.effect_index = effect_index;
    message.target_index = target_index;
    message.target_node = target_node;
    message.count = count;
    message.scale = scale;
    message.material_scale = material_scale;
    for (i = 0; i < count; i++)
    {
        message.points[i] = points[i];
        message.forwards[i] = forwards[i];
    }
    distributed_send(&message, _distributed_message_projectiles, 1, sizeof(message), _distributed_to_clients_reliably);
}

static void projectile_client_remove(struct projectile_client *client)
{
    struct projectile_datum *projectile = projectile_try(client->object_index);

    if (client->serial && projectile)
    {
        projectile_local_slots[DATUM_INDEX_TO_ABSOLUTE_INDEX(client->object_index)] = NONE;
        object_delete(client->object_index);
    }
    client->object_index = NONE;
}

static boolean projectile_state_valid(struct projectile_state *state)
{
    return state->serial && distributed_object_index_valid(state->object_index) &&
        projectile_tag_eligible(state->definition_index) &&
        (state->owner_index == NONE || distributed_object_index_valid(state->owner_index)) &&
        distributed_point_valid(&state->position, UNIT_WORLD_BOUND) &&
        distributed_axes_make_valid(&state->forward, &state->up) &&
        distributed_point_valid((real_point3d const *)&state->velocity, 64.f) &&
        !(state->flags & ~255UL) && distributed_real_valid(state->timer) &&
        distributed_real_valid(state->arming) && distributed_real_valid(state->odometer) &&
        state->timer >= 0.f && state->timer <= 1000000.f &&
        state->arming >= 0.f && state->arming <= 1000000.f &&
        state->odometer >= 0.f && state->odometer <= 1000000.f;
}

static void projectile_client_apply(struct projectile_client *client, struct projectile_state const *state, long time)
{
    struct projectile_datum *projectile = projectile_try(client->object_index);
    real_vector3d offset;

    if (!projectile)
    {
        struct object_placement_data placement;
        long owner = object_try_and_get_and_verify_type(state->owner_index, _object_mask_all) ? state->owner_index : NONE;

        object_placement_data_new(&placement, state->definition_index, owner);
        placement.owner_player_index = NONE;
        placement.position = state->position;
        placement.forward = state->forward;
        placement.up = state->up;
        SET_FLAG(placement.flags, _new_object_never_automatically_delete_bit, TRUE);
        projectile_receiving = TRUE;
        client->object_index = object_new(&placement);
        projectile_receiving = FALSE;
        projectile = projectile_try(client->object_index);
        if (!projectile) return; /* pool pressure: the next state retries */
        projectile_local_slots[DATUM_INDEX_TO_ABSOLUTE_INDEX(client->object_index)] =
            DATUM_INDEX_TO_ABSOLUTE_INDEX(state->object_index);
        SET_FLAG(projectile->object.flags, _object_deleted_when_deactivated_bit, FALSE);
    }
    offset.i = projectile->object.position.x - state->position.x;
    offset.j = projectile->object.position.y - state->position.y;
    offset.k = projectile->object.position.z - state->position.z;
    {
        real_vector3d movement;

        movement.i = -offset.i;
        movement.j = -offset.j;
        movement.k = -offset.k;
        projectile_play_flyby_sound(client->object_index, &movement, &projectile->object.location,
            projectile->projectile.ignore_object_index);
    }
    object_set_position(client->object_index, &state->position, &state->forward, &state->up);
    render_interpolation_correct_object(client->object_index, &offset);
    projectile->object.translational_velocity = state->velocity;
    projectile->projectile.flags = state->flags;
    projectile->projectile.detonation_timer = state->timer;
    projectile->projectile.arming_time = state->arming;
    projectile->projectile.odometer = state->odometer;
    projectile_export_function_values(client->object_index);
    client->time = time;
    client->received = game_time_get();
}

void network_projectiles_handle(long machine, word const *data, word size, boolean reliable)
{
    struct projectile_effect_message message;
    struct projectile_message *base = &message.base;
    struct projectile_client *client;
    short i;
    long slot;
    char const *impact_names[PROJECTILE_MARKERS] = {"normal", "incident", "negative incident", "reflection", "gravity"};
    char const *detonation_names[2] = {"", "gravity"};

    if (game_connection() != _game_connection_network_client || machine != NONE ||
        !projectile_stream_active() || !network_extensions_allows(NONE, _distributed_message_projectiles) ||
        size < sizeof(*base) || size > sizeof(message)) return;
    memset(&message, 0, sizeof(message));
    memcpy(&message, data, size);
    if (GET_MESSAGE_TYPE(base->header.header) != 2 || base->header.count != 1 ||
        base->header.type != _distributed_message_projectiles || base->reserved ||
        base->header.game_time < 0 ||
        base->operation > PROJECTILE_EFFECT ||
        size != (base->operation == PROJECTILE_EFFECT ? sizeof(message) : sizeof(*base)) ||
        (base->operation != PROJECTILE_STATE && !reliable) ||
        !projectile_state_valid(&base->state)) return;
    if (base->operation == PROJECTILE_EFFECT)
    {
        if (!message.event || (message.count != 2 && message.count != PROJECTILE_MARKERS) ||
            (message.target_index != NONE && (!distributed_object_index_valid(message.target_index) || message.target_node < 0)) ||
            !distributed_tag_of_group(message.effect_index, EFFECT_DEFINITION_TAG) ||
            !distributed_real_valid(message.scale) || !distributed_real_valid(message.material_scale) ||
            message.scale < 0.f || message.scale > 100.f || message.material_scale < 0.f || message.material_scale > 100.f) return;
        for (i = 0; i < message.count; i++)
            if (!distributed_point_valid(&message.points[i], UNIT_WORLD_BOUND) ||
                !distributed_point_valid((real_point3d const *)&message.forwards[i], 2.f)) return;
    }
    slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(base->state.object_index);
    client = &projectile_clients[slot];
    if (client->serial && (long)(base->state.serial - client->serial) < 0) return;
    if (client->serial != base->state.serial)
    {
        projectile_client_remove(client);
        memset(client, 0, sizeof(*client));
        client->object_index = NONE;
        client->time = NONE;
        client->serial = base->state.serial;
    }
    if (client->ended) return;
    if (base->operation == PROJECTILE_DELETE)
    {
        projectile_client_remove(client);
        client->ended = TRUE;
        return;
    }
    if (client->time == NONE || base->header.game_time > client->time)
        projectile_client_apply(client, &base->state, base->header.game_time);
    if (base->operation == PROJECTILE_EFFECT && message.event > client->event)
    {
        long owner = object_try_and_get_and_verify_type(base->state.owner_index, _object_mask_all) ? base->state.owner_index : NONE;

        client->event = message.event;
        if (message.count == PROJECTILE_MARKERS && projectile_try(client->object_index))
            owner = client->object_index;
        if (message.target_index != NONE && object_try_and_get_and_verify_type(message.target_index, _object_mask_all) &&
            object_has_node(message.target_index, message.target_node))
            effect_new_attached_from_markers(message.effect_index, owner, message.target_index, message.target_node,
                message.count, impact_names, message.points, message.forwards,
                message.scale, message.material_scale, NULL, NULL);
        else
            effect_new_unattached_from_markers(message.effect_index, owner, NULL, message.count,
                message.count == 2 ? detonation_names : impact_names, message.points, message.forwards,
                message.scale, message.material_scale, NULL, NULL, TRUE);
    }
}

boolean network_projectiles_update(long index)
{
    long local = DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
    long slot;

    if (game_connection() != _game_connection_network_client || local >= MAXIMUM_TRACKED_OBJECTS) return FALSE;
    slot = projectile_local_slots[local];
    return slot >= 0 && slot < MAXIMUM_TRACKED_OBJECTS &&
        projectile_clients[slot].serial && projectile_clients[slot].object_index == index;
}

void network_projectiles_client_tick(void)
{
    long i;

    if (game_connection() != _game_connection_network_client) return;
    if (!projectile_stream_started && projectile_stream_active() &&
        network_extensions_allows(NONE, _distributed_message_projectiles))
    {
        struct object_iterator iterator;

        /* A delayed capability reply can arrive after legacy controls have
           already made local AI shots. Retire those copies when switching to
           the host stream, while retaining player shots and received replicas. */
        projectile_stream_started = TRUE;
        object_iterator_new(&iterator, _object_mask_projectile, 0);
        while (object_iterator_next(&iterator))
        {
            struct projectile_datum *projectile = projectile_try(iterator.index);

            if (projectile && !network_projectiles_update(iterator.index) &&
                projectile->object.owner_player_index == NONE &&
                projectile_tag_eligible(projectile->definition_index))
                object_delete(iterator.index);
        }
    }
    for (i = 0; i < MAXIMUM_TRACKED_OBJECTS; i++)
    {
        struct projectile_client *client = &projectile_clients[i];

        /* Lost connection or a blocked stream must not leave a warning sound
           looping forever. Keep the serial so a delayed delete still wins. */
        if (client->serial && !client->ended && client->object_index != NONE &&
            game_time_get() - client->received > PROJECTILE_TIMEOUT)
            projectile_client_remove(client);
    }
}
