"""Exact JMS vertex sharing retains authored seams, skinning and materials."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build,read,run,mutated,CHECK_FAILED


def executable(fault=False):
    source=read('port/linux/src/custom_model_jms.c')
    if fault:
        source=mutated(source,'j<(int)sizeof(*vertex)','j<(int)sizeof(vertex->position)')
        source=mutated(source,'memcmp(vertex,&mesh->vertices[table[slot]-1],sizeof(*vertex))','memcmp(vertex,&mesh->vertices[table[slot]-1],sizeof(vertex->position))')
    return build('custom_models_deduplicate',(('halo_custom_models.h',read('port/linux/include/halo_custom_models.h')),('jms.inc',source)))


@pytest.mark.parametrize('case',['exact','seams','large','invalid'])
def test_deduplicate(case):
    status,output=run(executable(),case)
    assert status==0,output


def test_negative_positions_only():
    assert run(executable(True),'seams')[0]==CHECK_FAILED
