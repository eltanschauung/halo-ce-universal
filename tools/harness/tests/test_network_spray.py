"""Real host admission: owner, live unit, range, BSP, cooldown, finite vectors."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run

@pytest.mark.parametrize('case',['valid','transport','owner','dead','blocked','distance','reach','nan','bsp','cooldown','menu'])
def test_host_admission(case):
    headers=read('port/linux/include/spray_share.h')+read('port/third_party/monocypher/monocypher.h')
    core=re.sub(r'^#include "[^\n]+\n','',read('port/linux/src/spray_share.c'),flags=re.M)
    crypto=read('port/third_party/monocypher/monocypher.c').replace('#include "monocypher.h"','')
    adapter=re.sub(r'^#include[^\n]+\n','',read('port/linux/game/network_spray.c'),flags=re.M)
    status,output=run(build('network_spray',(('api.inc',headers),('under_test.inc',crypto+core+adapter))),case)
    assert status==0,output
