"""Production fall/ceiling bounds use each player's selected gravity at latency."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, function, structure, run, mutated, CHECK_FAILED


def generated(fault=False):
    source = read('port/linux/game/network_distributed.c')
    code = structure(source, 'distributed_on_foot_bound') + '\n'
    code += function(source, 'distributed_on_foot_fall_speed') + '\n'
    code += function(source, 'distributed_on_foot_ceiling')
    if fault:
        code = mutated(code, '0.5f * bound->gravity * ticks * ticks', '0.f')
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['stock', 'source-100ms', 'source-200ms', 'hover'])
def test_case(case):
    status, output = run(build('source_prediction', generated()), case)
    assert status == 0, output


def test_negative():
    status, output = run(build('source_prediction', generated(True)), 'hover')
    assert status == CHECK_FAILED, output
