from pathlib import Path
import sys
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build,read,function,run,mutated,CHECK_FAILED


def generated(fault=None):
    code=function(read('source/items/projectiles.c'),'projectile_play_flyby_sound')
    if fault:
        code=mutated(code,'unit_index != NONE && unit_index != ignore_object_index','unit_index != NONE')
    return (('under_test.inc',code),)


@pytest.mark.parametrize('case',['near','far','past','ahead','zero','owner','silent','no-player','no-unit','split-screen'])
def test_flyby(case):
    status,output=run(build('projectile_flyby',generated()),case)
    assert status==0,output


def test_negative():
    status,output=run(build('projectile_flyby',generated(True)),'owner')
    assert status==CHECK_FAILED,output
