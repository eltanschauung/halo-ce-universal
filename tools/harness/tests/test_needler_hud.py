"""All 31 HUD images, exact segment counts, stock fallback and map lifetime."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build, read, run, mutated, CHECK_FAILED

def generated(fault=None):
    source=re.sub(r'^#include[^\n]*\n','',read('port/linux/game/needler_hud.c'),flags=re.M)
    if fault=='count':source=mutated(source,'needle_owner[offset] < count','needle_owner[offset] <= count')
    if fault=='policy':source=mutated(source,'!needler_balance_hud_active(weapon)','FALSE')
    if fault=='cache':source=mutated(source,'needler_meters[PIN(rounds, 0, NEEDLER_HUD_ROUNDS)]','needler_meters[NEEDLER_HUD_ROUNDS]')
    return (('under_test.inc',source),)

@pytest.mark.parametrize('case',['counts','draw','fallback','allocation','reload'])
def test_case(case):
    status,output=run(build('needler_hud',generated()),case)
    assert status==0,output

@pytest.mark.parametrize('fault,case',[('count','counts'),('policy','fallback'),('cache','draw')])
def test_negative(fault,case):
    status,output=run(build('needler_hud',generated(fault)),case)
    assert status==CHECK_FAILED,output
