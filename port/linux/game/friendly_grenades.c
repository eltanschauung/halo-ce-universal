/* Thrown grenades ignore teammates before impact on every supporting peer.
   Older/mixed lobbies keep stock flight, as with the fork's weapon rules. */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "items/projectiles.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/object_types.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "units/units.h"
#include "network_coop.h"
#include "network_distributed.h"
#include "network_extensions.h"
#include "friendly_grenades.h"
#include <string.h>

unsigned long system_milliseconds(void);
static boolean enabled, client_enabled;
static short sent_policy = NONE;
static unsigned long last_sent;
struct grenade_collision_policy
{
    struct distributed_message_header header;
    word enabled, reserved;
};

void friendly_grenades_reset(void)
{
    enabled = client_enabled = FALSE;
    sent_policy = NONE;
    last_sent = 0;
}

void friendly_grenades_update(void)
{
    struct network_game *game;
    struct grenade_collision_policy message;
    unsigned long now;
    if (!game_in_progress() || main_menu_is_active()) return;
    game = network_game_get_game();
    if (game_connection() == _game_connection_network_server)
        enabled = game && game->variant_options.friendly_fire != _friendly_fire_on &&
            network_extensions_balance_matches(_distributed_message_grenade_collision_policy);
    else if (game_connection() == _game_connection_network_client)
        enabled = client_enabled && network_extensions_allows(NONE, _distributed_message_grenade_collision_policy);
    else
        enabled = game_variant_options_get()->friendly_fire != _friendly_fire_on;
    if (game_connection() != _game_connection_network_server) return;
    now = system_milliseconds();
    if (sent_policy == enabled && now - last_sent < 500) return;
    sent_policy = enabled;
    last_sent = now;
    memset(&message, 0, sizeof(message));
    message.enabled = enabled;
    distributed_send(&message, _distributed_message_grenade_collision_policy, 1, sizeof(message),
        _distributed_to_clients_reliably);
}

void friendly_grenades_handle(long machine, word const *data, word size, boolean reliable)
{
    struct grenade_collision_policy message;
    if (machine != NONE || !reliable || size != sizeof(message) ||
        game_connection() != _game_connection_network_client || !game_in_progress() || main_menu_is_active() ||
        !network_extensions_allows(NONE, _distributed_message_grenade_collision_policy)) return;
    memcpy(&message, data, sizeof(message));
    if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.count != 1 ||
        message.header.type != _distributed_message_grenade_collision_policy || message.enabled > 1 || message.reserved) return;
    client_enabled = message.enabled != 0;
    friendly_grenades_update();
}

struct grenade_collision_context
{
    long owner;
    long team;
};

static boolean grenade_collision_object_allowed(long object_index, void const *context)
{
    struct grenade_collision_context const *grenade = context;
    short depth;
    /* An independently listed held weapon must not put the collision back.
       A vehicle stays solid even when its player's biped is ignored. */
    for (depth = 0; object_index != NONE && depth < 32; depth++)
    {
        struct object_datum *object = object_try_and_get(object_index);
        if (!object) break;
        if (object->object.type == _object_type_biped)
        {
            struct unit_datum *unit = (struct unit_datum *)object;
            struct player_datum *player = unit->unit.player_index == NONE ? NULL :
                player_try_and_get(unit->unit.player_index);
            if (player && player->unit_index == object_index && unit->unit.player_index != grenade->owner &&
                (network_coop_active() || (game_engine_has_teams() &&
                    grenade->team != NONE && player->team_index == grenade->team)))
                return FALSE;
        }
        object_index = object->object.parent_object_index;
    }
    return TRUE;
}

boolean friendly_grenades_collision_test_line(long projectile_index, unsigned long flags,
    real_point3d const *from, real_point3d const *to, long ignored, struct collision_result *hit)
{
    struct projectile_datum *projectile;
    struct player_datum *owner;
    struct game_globals *globals;
    short index;
    if (!enabled) return collision_test_line(flags, from, to, ignored, hit);
    projectile = projectile_get(projectile_index);
    owner = projectile->object.owner_player_index == NONE ? NULL :
        player_try_and_get(projectile->object.owner_player_index);
    globals = scenario_get_game_globals();
    if (owner && globals)
    {
        for (index = 0; index < MIN(globals->grenades.count, NUMBER_OF_UNIT_GRENADE_TYPES); index++)
        {
            struct game_globals_grenade *grenade = TAG_BLOCK_GET_ELEMENT(&globals->grenades, index, struct game_globals_grenade);
            if (grenade->projectile.index == projectile->definition_index)
            {
                struct grenade_collision_context context;
                real_vector3d vector;
                context.owner = projectile->object.owner_player_index;
                context.team = owner->team_index;
                vector_from_points3d(from, to, &vector);
                return collision_test_vector_filtered(flags, from, &vector, ignored, hit,
                    grenade_collision_object_allowed, &context);
            }
        }
    }
    return collision_test_line(flags, from, to, ignored, hit);
}
