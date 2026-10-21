"""A model must not report success after native texture allocation fails."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import read,function,build,run,mutated,CHECK_FAILED

def executable(fault=False):
    source=function(read('port/linux/game/custom_models.c'),'bitmap_from_image')
    if fault:source=mutated(source,' || !bitmap->hardware_format','')
    return build('custom_models_bitmap',(('bitmap.inc',source),('halo_custom_models.h',read('port/linux/include/halo_custom_models.h'))))

@pytest.mark.parametrize('case',['0','1','2','3'])
def test_allocation_and_upload(case):
    status,output=run(executable(),case)
    assert status==0,output

def test_negative_missing_upload_check():
    assert run(executable(True),'3')[0]==CHECK_FAILED
