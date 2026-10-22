/* Per-player movement policy, authenticated on the existing fork-only stream.
   Stock peers receive no policy and keep their original movement and damage. */
#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "game/cheats.h"
#include "cutscene/cinematics.h"
#include "main/main.h"
#include "math/real_math.h"
#include "objects/damage.h"
#include "tag_files/tag_groups.h"
#include "units/bipeds.h"
#include "units/biped_definitions.h"
#include "network_distributed.h"
#include "network_extensions.h"
#include "source_movement.h"
#include <string.h>

int config_boolean(char const *name);
unsigned long system_milliseconds(void);
struct source_movement_policy
{
    struct distributed_message_header header;
    /* Absolute player slots are shared; datum identifiers are machine-local. */
    short machines[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
};
static long source_players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static short source_machines[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean source_policy_valid;
static unsigned long source_last_sent;

void source_movement_reset(void)
{
    memset(source_players, 0xFF, sizeof(source_players));
    memset(source_machines, 0xFF, sizeof(source_machines));
    source_policy_valid = FALSE;
    source_last_sent = 0;
}

static boolean source_movement_host_player(long index, struct player_datum const *player)
{
    long local = local_player_get_player_index(0);
    struct player_datum *host = local == NONE ? NULL : player_try_and_get(local);
    (void)index;
    return config_boolean("game.source_movement") && host &&
        (player->network_player_data.machine_index == host->network_player_data.machine_index ||
        network_extensions_allows(player->network_player_data.machine_index, _distributed_message_source_movement));
}

boolean source_movement_player(long index)
{
    struct player_datum *player = index == NONE ? NULL : player_try_and_get(index);
    long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
    if (!player || player->quit_out_of_game || !game_in_progress() || main_menu_is_active()) return FALSE;
    if (game_connection() == _game_connection_local)
        return config_boolean("game.source_movement") != 0;
    if (game_connection() == _game_connection_network_server)
        return source_movement_host_player(index, player);
    return game_connection() == _game_connection_network_client && source_policy_valid &&
        network_extensions_allows(NONE, _distributed_message_source_movement) &&
        slot < NUMBEROF(source_players) && source_players[slot] == index;
}

boolean source_movement_biped(long index)
{
    struct biped_datum *biped = biped_try_and_get(index);
    struct biped_definition *definition;
    if (!biped || cheat.jetpack || biped->object.parent_object_index != NONE ||
        TEST_FLAG(biped->object.damage_flags, _object_dead_bit) ||
        TEST_FLAG(biped->unit.flags, _unit_suspended_bit) || cinematic_in_progress() ||
        !source_movement_player(biped->unit.player_index)) return FALSE;
    definition = biped_definition_get(biped->definition_index);
    return !TEST_FLAG(definition->biped.flags, _biped_flying_bit) &&
        !TEST_FLAG(definition->biped.flags, _biped_climbs_anything_bit) &&
        !TEST_FLAG(biped->biped.flags, _biped_absolute_movement_bit) &&
        !TEST_FLAG(biped->biped.flags, _biped_no_collision_bit);
}

real source_movement_player_gravity(long index, real stock)
{
    return source_movement_player(index) ? SOURCE_GRAVITY : stock;
}

real source_movement_fall_velocity(long index, real velocity, real stock_gravity)
{
    /* v squared = 2gh: retain Halo's fall-distance thresholds under Source gravity.
       Change only damage evaluation, never the actual velocity. */
    if (stock_gravity > 0.f && source_movement_biped(index))
        return velocity * (real)sqrt(stock_gravity / SOURCE_GRAVITY);
    return velocity;
}

void source_movement_update(void)
{
    struct source_movement_policy policy;
    struct data_iterator iterator;
    struct player_datum *player;
    unsigned long now;
    if (!game_in_progress() || main_menu_is_active() ||
        game_connection() != _game_connection_network_server) return;
    memset(&policy, 0, sizeof(policy));
    memset(policy.machines, 0xFF, sizeof(policy.machines));
    data_iterator_new(&iterator, player_data);
    while ((player = data_iterator_next(&iterator)) != NULL)
    {
        long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
        if (slot < NUMBEROF(policy.machines) && !player->quit_out_of_game &&
            source_movement_host_player(iterator.datum_index, player))
            policy.machines[slot] = player->network_player_data.machine_index;
    }
    now = system_milliseconds();
    if (source_policy_valid && !memcmp(policy.machines, source_machines, sizeof(source_machines)) &&
        now - source_last_sent < 500) return;
    memcpy(source_machines, policy.machines, sizeof(source_machines));
    source_policy_valid = TRUE;
    source_last_sent = now;
    distributed_send(&policy, _distributed_message_source_movement, 1, sizeof(policy), _distributed_to_clients_reliably);
}

void source_movement_handle(long machine, word const *data, word size, boolean reliable)
{
    struct source_movement_policy policy;
    short i;
    struct data_iterator iterator;
    struct player_datum *player;
    if (machine != NONE || !reliable || size != sizeof(policy) ||
        game_connection() != _game_connection_network_client || !game_in_progress() || main_menu_is_active() ||
        !network_extensions_allows(NONE, _distributed_message_source_movement)) return;
    memcpy(&policy, data, sizeof(policy));
    if (GET_MESSAGE_TYPE(policy.header.header) != 2 || policy.header.count != 1 ||
        policy.header.type != _distributed_message_source_movement) return;
    for (i = 0; i < NUMBEROF(policy.machines); i++)
        if (policy.machines[i] < NONE || policy.machines[i] >= HALO_PORT_MAXIMUM_NETWORK_MACHINES) return;
    memset(source_players, 0xFF, sizeof(source_players));
    data_iterator_new(&iterator, player_data);
    while ((player = data_iterator_next(&iterator)) != NULL)
    {
        long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
        if (slot < NUMBEROF(policy.machines) && policy.machines[slot] != NONE &&
            policy.machines[slot] == player->network_player_data.machine_index)
            source_players[slot] = iterator.datum_index;
    }
    source_policy_valid = TRUE;
}

void source_movement_wish(real_vector3d const *desired, real_vector3d const *forward, real_vector3d *wish)
{
    real_vector2d facing;
    facing.i = forward->i;
    facing.j = forward->j;
    normalize2d(&facing);
    /* Use Halo's calculated speed, including direction, crouch, stun and powerups.
       Source acceleration may still retain extra momentum from jumps and strafing. */
    wish->i = facing.i * desired->i - facing.j * desired->j;
    wish->j = facing.j * desired->i + facing.i * desired->j;
    wish->k = 0.f;
}

real source_movement_self_damage(struct damage_data const *damage, long index)
{
    struct biped_datum *biped = biped_try_and_get(index);
    char const *name;
    /* Never rescale the host's already scaled replay or another player's hit. */
    if (game_connection() == _game_connection_network_client || !biped ||
        damage->owner_player_index == NONE || damage->owner_player_index != biped->unit.player_index ||
        !source_movement_player(biped->unit.player_index) || damage->definition_index == NONE) return 1.f;
    name = tag_get_name(damage->definition_index);
    if (name && (!strcmp(name, "weapons\\frag grenade\\explosion") ||
        !strcmp(name, "weapons\\plasma grenade\\explosion") ||
        !strcmp(name, "weapons\\rocket launcher\\explosion"))) return 0.2f;
    return 1.f;
}
