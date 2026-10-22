"""Host-authored enemy shots: controls, launch state, fuse/sounds and wire admission."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, function, run, mutated, CHECK_FAILED


def generated(fault=None):
    code = re.sub(r'^#include "[^"\n]+"\n', '', read('port/linux/game/network_projectiles.c'), flags=re.M)
    if fault == 'suppress':
        code = mutated(code, '!projectile_receiving && game_connection()', 'FALSE && game_connection()')
    if fault == 'velocity':
        code = mutated(code, 'state->velocity = projectile->object.translational_velocity;', 'state->velocity = (real_vector3d){0,0,0};')
    if fault == 'tombstone':
        code = mutated(code, 'if (client->ended) return;', 'if (FALSE) return;')
    if fault == 'sender':
        code = mutated(code, 'machine != NONE ||', 'FALSE ||')
    if fault == 'timer':
        code = mutated(code, 'projectile->projectile.detonation_timer = state->timer;', 'projectile->projectile.detonation_timer = 0;')
    if fault == 'delete':
        code = mutated(code, 'projectile_client_remove(client);\n        client->ended = TRUE;', 'client->ended = TRUE;')
    if fault == 'reliable':
        code = mutated(code, '(base->operation != PROJECTILE_STATE && !reliable)', 'FALSE')
    return (('under_test.inc', code), ('release.inc', function(read('source/units/units.c'), 'unit_throw_grenade_release')))


CASES = ['grenade-launch', 'wraith-state', 'in-hand', 'warning-sound', 'fuse', 'bounce',
         'sticky', 'delete', 'same-tick-hit', 'attached-effect', 'missing-effect-target', 'duplicate', 'effect-overtaken', 'tombstone',
         'slot-reuse', 'local-slot-reuse', 'pool-full', 'timeout', 'late-negotiation', 'late-join', 'stock',
         'player', 'speed-boundary', 'hitscan', 'non-coop', 'admission', 'corrupt', 'latency-100', 'latency-200', 'loss']


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('network_projectiles', generated(), flags=('-msse2', '-mfpmath=sse', '-ffp-contract=off')), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [('suppress', 'grenade-launch'), ('velocity', 'wraith-state'),
    ('tombstone', 'tombstone'), ('sender', 'admission'), ('timer', 'fuse'), ('delete', 'delete'),
    ('reliable', 'admission')])
def test_negative(fault, case):
    status, output = run(build('network_projectiles', generated(fault)), case)
    assert status == CHECK_FAILED, output


def test_engine_hooks():
    objects = function(read('source/objects/objects.c'), 'object_new')
    assert objects.index('network_projectiles_suppress(data)') < objects.index('object_header_new(')
    projectiles = read('source/items/projectiles.c')
    update = function(projectiles, 'projectile_update')
    assert update.index('network_projectiles_update(') < update.index('projectile->projectile.arming_time +=')
    assert 'network_projectiles_deleted(projectile_index)' in function(projectiles, 'projectile_delete')
    assert 'network_projectiles_effect(' in function(projectiles, 'projectile_detonate')
    assert 'network_projectiles_effect(' in function(projectiles, 'projectile_effect_new')
    # No alternative silent proxy: the engine's ordinary constructor creates
    # the tag's looping sound, particles, lights and model for the replica.
    module = read('port/linux/game/network_projectiles.c')
    assert 'object_new(&placement)' in function(module, 'projectile_client_apply')
    assert 'game_looping_sound_new(' in function(read('source/objects/objects.c'), 'object_create_attachments') or 'game_looping_sound_new(' in read('source/objects/objects.c')


@pytest.mark.parametrize('case', ['fire', 'player-driven'])
def test_vehicle_combat(case):
    code = function(read('port/linux/game/network_actors.c'), 'network_actors_drive')
    result, output = run(build('projectile_vehicle', (('under_test.inc', code),)), case)
    assert result == 0, output
    vehicles = function(read('source/units/vehicles.c'), 'vehicle_update')
    assert vehicles.index('network_objects_vehicle_follows_host(') < vehicles.index('update_human_tank_physics(')
