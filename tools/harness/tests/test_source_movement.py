"""Source equations, per-player admission, stock fallback and damage routing."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, function, run, mutated, CHECK_FAILED


def generated(fault=None):
    strip = lambda text: re.sub(r'^#include "[^"\n]+"\n', '', text, flags=re.M)
    code = strip(read('port/linux/game/source_movement_math.c'))
    code += strip(read('port/linux/game/source_movement.c'))
    if fault == 'air':
        code = mutated(code, 'MIN(wishspeed, 30.f / SOURCE_UNITS_PER_WORLD / TICKS_PER_SECOND)', 'wishspeed')
    if fault == 'friction':
        code = mutated(code, 'speed > 0.f && !jumping', 'speed > 0.f')
    if fault == 'self':
        code = mutated(code, 'damage->owner_player_index != biped->unit.player_index', 'FALSE')
    if fault == 'replay':
        code = mutated(code, 'game_connection() == _game_connection_network_client || !biped', '!biped')
    if fault == 'peer':
        code = mutated(code, 'network_extensions_allows(player->network_player_data.machine_index, _distributed_message_source_movement)', 'TRUE')
    if fault == 'gravity':
        code = mutated(code, 'velocity->k -= SOURCE_GRAVITY * 0.5f;', 'velocity->k -= SOURCE_GRAVITY;')
    jump = function(read('source/units/bipeds.c'), 'biped_jump')
    return (('api.inc', strip(read('port/linux/game/source_movement.h'))),
            ('under_test.inc', code), ('jump.inc', jump))


CASES = ['ground', 'friction', 'stop', 'reverse', 'diagonal', 'crouch', 'stock-speed', 'modifiers', 'air-coast',
         'air-cap', 'air-strafe', 'bhop', 'gravity', 'jump', 'stock-jump',
         'solo', 'mixed-host', 'upstream-host', 'policy', 'bad-policy', 'slot-reuse',
         'reset', 'send', 'excluded', 'self-damage', 'other-damage', 'replay']


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('source_movement', generated(), flags=('-msse2', '-mfpmath=sse', '-ffp-contract=off')), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [('air', 'air-cap'), ('friction', 'bhop'),
    ('self', 'other-damage'), ('replay', 'replay'), ('peer', 'mixed-host'), ('gravity', 'gravity')])
def test_negative(fault, case):
    status, output = run(build('source_movement', generated(fault)), case)
    assert status == CHECK_FAILED, output
