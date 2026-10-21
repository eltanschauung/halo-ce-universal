"""Production grenade policy, object filtering and all three projectile rays."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, function, run, mutated, CHECK_FAILED


def generated(fault=None):
    policy = re.sub(r'^#include "[^"\n]+"\n', '', read('port/linux/game/friendly_grenades.c'), flags=re.M)
    objects = function(read('source/physics/collisions.c'), 'object_test_vector')
    rays = function(read('source/items/projectiles.c'), 'projectile_collision_test_line')
    if fault == 'filter':
        objects = mutated(objects, '(!filter || filter(object_index, context))', 'TRUE')
    if fault == 'team':
        policy = mutated(policy, 'player->team_index == grenade->team', 'TRUE')
    if fault == 'owner':
        policy = mutated(policy, 'unit->unit.player_index != grenade->owner', 'TRUE')
    if fault == 'gate':
        policy = mutated(policy, 'if (!enabled) return collision_test_line(flags, from, to, ignored, hit);', '')
    return (('policy.inc', policy), ('objects.inc', objects), ('rays.inc', rays))


@pytest.mark.parametrize('case', ['settings', 'teams', 'coop', 'classify', 'hierarchy',
                                 'wall', 'rays', 'mixed', 'admission', 'latency'])
def test_case(case):
    status, output = run(build('friendly_grenades', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [('filter', 'coop'), ('team', 'teams'),
                                      ('owner', 'teams'), ('gate', 'settings')])
def test_negative(fault, case):
    status, output = run(build('friendly_grenades', generated(fault)), case)
    assert status == CHECK_FAILED, output


def test_filter_wiring():
    source = read('source/physics/collisions.c')
    wrapper = function(source, 'collision_test_vector')
    filtered = function(source, 'collision_test_vector_filtered')
    assert 'collision, NULL, NULL' in wrapper
    assert 'collision, filter, context' in filtered
    # The only projectile sweeps must all share the policy, including the fat rays.
    rays = function(read('source/items/projectiles.c'), 'projectile_collision_test_line')
    assert rays.count('friendly_grenades_collision_test_line(') == 3
    assert not re.search(r'(?<!_)\bcollision_test_line\(', rays)
