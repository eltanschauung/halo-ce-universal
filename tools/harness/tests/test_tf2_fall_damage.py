"""Production fall-fraction damage routing, maximum health and death boundary."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build,read,function,run

def test_damage_routing():
    body=function(read('source/objects/damage.c'),'object_damage_body')
    a=body.index('/* port: Source landing events')
    b=body.index('\n\tif (!TEST_FLAG(object->object.damage_flags',a)
    condition=next(line for line in body.splitlines() if 'else if (body_vitality < 0.f ||' in line)
    condition=condition.split('else if (',1)[1].split(') /* port:',1)[0]
    status,output=run(build('tf2_fall_damage',(('under_test.inc',body[a:b]),('lethal.inc','lethal = ('+condition+');'))),'route')
    assert status == 0, output
