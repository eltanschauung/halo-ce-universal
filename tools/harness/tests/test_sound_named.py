"""Named impulses select the new voice without changing shared weapon tags."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, mutated, read, run

CASES = ['second', 'third', 'missing', 'disabled', 'promoted', 'linked']
CONTROLS = {
    'wrong-variant': ('sound->permutation_index = permutation_index;', 'sound->permutation_index = 0;', 'third'),
    'missing-cache': ('_sound_cache_sound_request(permutation, FALSE, TRUE, FALSE);', ';', 'second'),
    'promoted-override': ('sound->definition_index != definition_index', 'FALSE', 'promoted'),
}

def generated(control=None):
    code = function(read('source/sound/sound_manager.c'), 'unspatialized_impulse_sound_new_named')
    if control:
        before, after, _ = CONTROLS[control]
        code = mutated(code, before, after)
    return (('under_test.inc', code),)

@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('sound_named', generated()), case)
    assert status == 0, output

@pytest.mark.parametrize('control', CONTROLS)
def test_negative_control(control):
    status, output = run(build('sound_named', generated(control)), CONTROLS[control][2])
    assert status == 1, output
