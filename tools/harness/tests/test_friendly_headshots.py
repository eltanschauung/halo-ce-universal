"""Production instant-damage conversion and exact-zero death boundary."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build,read,function,run,mutated,CHECK_FAILED

def generated(fault=False):
    source=function(read('source/objects/damage.c'),'object_damage_body')
    start=source.index('if (force_instant_damage)')
    end=source.index('\n\t\tobject->object.body_vitality -= actual_damage;',start)
    code=source[start:end]
    if fault:code=mutated(code,'if (instant_kill_scale < 1.f)','if (FALSE)')
    death=next(line for line in source.splitlines() if 'else if (body_vitality < 0.f ||' in line)
    condition=death.split('else if (',1)[1].split(') /* port:',1)[0]
    return (('under_test.inc',code),('lethal.inc','last_lethal = ('+condition+');'))

@pytest.mark.parametrize('fault',[False,True])
def test_headshot(fault):
    status,output=run(build('friendly_headshots',generated(fault)), 'headshot')
    assert status==(CHECK_FAILED if fault else 0),output
