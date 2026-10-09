"""Real host admission: owner, live unit, range, BSP, cooldown, finite vectors."""
import re
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run, function, mutated, CHECK_FAILED

def generated(old_free=False):
    headers=read('port/linux/include/spray_share.h')+read('port/third_party/monocypher/monocypher.h')
    core=re.sub(r'^#include "[^\n]+\n','',read('port/linux/src/spray_share.c'),flags=re.M)
    crypto=read('port/third_party/monocypher/monocypher.c').replace('#include "monocypher.h"','')
    adapter=re.sub(r'^#include[^\n]+\n','',read('port/linux/game/network_spray.c'),flags=re.M)
    if old_free:
        adapter=mutated(adapter,'halo_spray_file_free(data);','free(data);')
    provider=read('port/linux/src/spray_image.c')
    code=function(provider,'halo_spray_file_read')+'\n'
    code+=function(provider,'halo_spray_file_free')+'\n'
    # Retain the real cseries free macros at the engine/platform boundary.
    macros='\n'.join(re.findall(r'^#define (?:match_free\(|free\().*$',read('source/cseries/cseries.h'),re.M))
    return (('api.inc',headers),('provider.inc',code),
            ('under_test.inc',crypto+core+'\n'+macros+'\n'+adapter+'\n#undef free\n'))

@pytest.mark.parametrize('case',['valid','transport','owner','dead','blocked','distance','reach','nan','bsp','cooldown','menu'])
def test_host_admission(case):
    status,output=run(build('network_spray',generated()),case)
    assert status==0,output

@pytest.mark.parametrize('case',['publish-host','publish-client','publish-rejected',
                                'publish-ready-failed','publish-missing','publish-unavailable'])
def test_publish_ownership(case):
    status,output=run(build('network_spray',generated()),case)
    assert status==0,output

def test_original_wrong_allocator_detected():
    status,output=run(build('network_spray',generated(True)),'publish-host')
    assert status==CHECK_FAILED and 'engine allocator' in output,output
