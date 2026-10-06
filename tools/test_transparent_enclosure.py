"""Exercise production enclosure recognition and queue dispatch in a hidden GL context.

Uses synthetic closed/open meshes by default. --map optionally also checks the
actual Xbox overshield geometry; proprietary assets never enter the repository.
The material draws are simple blend probes, not substitutes for gameplay testing.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parent.parent

def block(source, marker):
    start = source.index(marker)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
typedef unsigned long DWORD,D3DCOLOR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define __HALO_LINUX_PLATFORM_H
#include "xgpu.h"
#define GL_DEFINE(name) __typeof__(name) name;
GL_FUNCTIONS(GL_DEFINE)
#undef GL_DEFINE
typedef unsigned char byte,boolean;
typedef unsigned short word;
typedef float real;
typedef struct {float i,j;} real_vector2d;
typedef struct {float i,j,k;} real_vector3d;
typedef struct {float x,y,z;} real_point3d;
typedef struct {struct {float i,j,k;} n;float d;} real_plane3d;
typedef struct {float red,green,blue;} real_rgb_color;
typedef struct {float alpha,red,green,blue;} real_argb_color;
struct tag_reference {long group_tag;char *name;long name_length,index;};
struct tag_block {long count;void *address,*definition;};
#define NONE -1
#define FLAG(b) (1u<<(b))
#define TEST_FLAG(f,b) (((f)&FLAG(b))!=0)
#define SET_FLAG(f,b,v) ((v)?((f)|=FLAG(b)):((f)&=~FLAG(b)))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define RASTERIZER_MAXIMUM_TRANSPARENT_GEOMETRY_GROUPS 384
#define BIT_VECTOR_SIZE_IN_LONGS(n) (((n)+31)/32)
#define BIT_VECTOR_SET_FLAG(v,b,s) SET_FLAG((v)[(b)/32],(b)%32,s)
#define BIT_VECTOR_TEST_FLAG(v,b) TEST_FLAG((v)[(b)/32],(b)%32)
static struct {struct {real_point3d position;} camera;} global_window_parameters;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}}while(0)
#define match_assert(file,line,c) CHECK(c)
struct test_buffer {byte *data;};
static void IDirect3DVertexBuffer8_Lock(void *p,int o,int size,byte **data,int flags){*data=p?((struct test_buffer*)p)->data+o:NULL;}
static void IDirect3DIndexBuffer8_Lock(void *p,int o,int size,byte **data,int flags){IDirect3DVertexBuffer8_Lock(p,o,size,data,flags);}
static void IDirect3DVertexBuffer8_Unlock(void *p){}
static void IDirect3DIndexBuffer8_Unlock(void *p){}
#define D3DLOCK_READONLY 128
static long rasterizer_geometry_get_vertex_size(short type){return type==4?68:32;}
'''

HARNESS = r'''
static struct transparent_geometry_group storage[384], *transparent_geometry_groups=storage;
static long transparent_geometry_group_count;
static short transparent_geometry_group_sorted_indices[384];
static unsigned long pending[12];
static int shader_is_water_decal(struct shader *p){return 0;}
static void *rasterizer_transparent_geometry_get_group_from_presorted_index(short i){CHECK(i>=0&&i<transparent_geometry_group_count);return storage+i;}
static short rasterizer_transparent_geometry_get_group_presorted_index(struct transparent_geometry_group const *g){for(int i=0;i<transparent_geometry_group_count;i++)if(g==storage+i)return i;return NONE;}
static boolean rasterizer_transparent_geometry_get_group_pending_status(struct transparent_geometry_group const *g){short i=rasterizer_transparent_geometry_get_group_presorted_index(g);return i==NONE||!(pending[i/32]&(1u<<(i%32)));}
static void rasterizer_transparent_geometry_set_group_pending_status(struct transparent_geometry_group const *g,boolean p){short i=rasterizer_transparent_geometry_get_group_presorted_index(g);if(i!=NONE){if(p)pending[i/32]&=~(1u<<(i%32));else pending[i/32]|=1u<<(i%32);}}
static void rasterizer_transparent_geometry_group_draw(struct transparent_geometry_group*,boolean);
static void probe_draw(struct transparent_geometry_group const*);
'''

TESTS = r'''
static struct shader_transparent_generic_definition energy_material;
static struct shader_transparent_glass_definition glass_material;
static float shell[5][8]={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{0,0,2}};
static unsigned short shell_indices[]={0,1,2,0,2,3,0,4,1,1,4,2,2,4,3,3,4,0};
static float sphere[6][8]={{-.22f,0,.7f},{.22f,0,.7f},{0,-.22f,.7f},{0,.22f,.7f},{0,0,.48f},{0,0,.92f}};
static unsigned short sphere_indices[]={0,2,4,2,1,4,1,3,4,3,0,4,2,0,5,1,2,5,3,1,5,0,3,5};
static struct test_buffer ovb={(byte*)shell},ivb={(byte*)sphere},itb={(byte*)shell_indices};
static struct vertex_buffer outer={5,0,5,0,NULL,&ovb},inner={5,0,6,0,NULL,&ivb};
static struct triangle_buffer tb={0,0,6,NULL,&itb};
static int sequence[8],nsequence,rendering,view,reflection;
static GLuint gpu_program,vao,vbo;
static void init_pair(int first){
 memset(&energy_material,0,sizeof(energy_material));memset(&glass_material,0,sizeof(glass_material));
 energy_material.shader.type=5;energy_material.generic.flags=4;
 energy_material.generic.framebuffer_blend_function=3;energy_material.generic.lens_flare.index=NONE;
 glass_material.shader.base.type=8;glass_material.flags=4;glass_material.reflection_type=1;
 for(int i=0;i<2;i++){
  struct transparent_geometry_group *g=storage+first+i;memset(g,0,sizeof(*g));
  g->object_index=first+1;g->node_matrix_count=1;g->previous_group_presorted_index=g->next_group_presorted_index=NONE;
  g->sorted_index=first+i;g->z_sort=i?1.0f:2.0f;g->dynamic_vertex_buffer_index=NONE;
 }
 storage[first].shader=(struct shader*)&energy_material;storage[first].vertex_buffer=&inner;
 storage[first+1].shader=(struct shader*)&glass_material;storage[first+1].vertex_buffer=&outer;
 storage[first+1].triangle_buffer=&tb;storage[first+1].triangle_count=tb.count;
 transparent_geometry_group_count=first+2;
}
static void draw_mesh(float *vertices,int stride,const unsigned short *indices,int count){
 float positions[3*384];CHECK(count<=384);for(int i=0;i<count;i++)memcpy(positions+3*i,(byte*)vertices+stride*indices[i],12);
 glBufferData(GL_ARRAY_BUFFER,count*12,positions,GL_STREAM_DRAW);glDrawArrays(GL_TRIANGLES,0,count);
}
static struct {BOOL cull_face;GLenum front_face,cull_mode;} gl_state;
static void state_enable(BOOL *state,GLenum cap,BOOL enabled){if(*state!=enabled){*state=enabled;if(enabled)glEnable(cap);else glDisable(cap);}}
static void set_cull(DWORD mode){
 DWORD rs[D3DRS_MAX]={0};rs[D3DRS_CULLMODE]=mode;rs[D3DRS_FRONTFACE]=D3DFRONT_CW;
 CULL_TRANSLATION
}
static void probe_draw(struct transparent_geometry_group const *g){
 if(g->shader->base.type==8){
  DWORD c=transparent_glass_cull_mode(g,glass_material.flags);
  sequence[nsequence++]=c==D3DCULL_CW?1:c==D3DCULL_CCW?3:4;
  if(!rendering)return;set_cull(c);glUniform1i(glGetUniformLocation(gpu_program,"kind"),0);
  glBlendFunc(GL_ZERO,GL_SRC_COLOR);draw_mesh((float*)shell,32,shell_indices,18);
  if(reflection){glUniform1i(glGetUniformLocation(gpu_program,"kind"),2);glBlendFunc(GL_ONE,GL_ONE);draw_mesh((float*)shell,32,shell_indices,18);}
 }else{
  sequence[nsequence++]=2;if(!rendering)return;set_cull(D3DCULL_NONE);
  glUniform1i(glGetUniformLocation(gpu_program,"kind"),1);glBlendFunc(GL_ONE,GL_ONE);
  draw_mesh((float*)sphere,32,sphere_indices,24);
 }
}
static GLuint compile(GLenum type,const char *text){GLuint s=glCreateShader(type);glShaderSource(s,1,&text,NULL);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);CHECK(ok);return s;}
static void uniform3(GLint location,float x,float y,float z){float v[4]={x,y,z,0};glUniform4fv(location,1,v);}
static void render(unsigned char *pixels,int fixed,int start){
 init_pair(0);memset(pending,0,sizeof(pending));nsequence=0;
 if(fixed)rasterizer_transparent_geometry_model_end(0);
 storage[0].z_sort=fixed?storage[1].z_sort:(start?0:2);
 short a=0,b=1;int cmp=group_sorted_indices_cmpfn(&a,&b);
 glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT);rendering=1;
 rasterizer_transparent_geometry_group_draw(storage+(cmp<0?0:1),FALSE);
 rasterizer_transparent_geometry_group_draw(storage+(cmp<0?1:0),FALSE);
 rendering=0;glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);CHECK(glGetError()==GL_NO_ERROR);
 if(fixed)CHECK(nsequence==3&&sequence[0]==1&&sequence[1]==2&&sequence[2]==3);
}
static void tests(void){
 CHECK(rasterizer_transparent_encloses((byte*)shell,5,32,shell_indices,6,FALSE,(byte*)sphere,6,32));
 CHECK(!rasterizer_transparent_encloses((byte*)shell,5,32,shell_indices,5,FALSE,(byte*)sphere,6,32));
 float old=sphere[0][0];sphere[0][0]=-2;
 CHECK(!rasterizer_transparent_encloses((byte*)shell,5,32,shell_indices,6,FALSE,(byte*)sphere,6,32));sphere[0][0]=old;
 sphere[0][0]=NAN;CHECK(!rasterizer_transparent_encloses((byte*)shell,5,32,shell_indices,6,FALSE,(byte*)sphere,6,32));sphere[0][0]=old;
 unsigned short tmp[18];memcpy(tmp,shell_indices,sizeof(tmp));tmp[0]=65535;
 CHECK(!rasterizer_transparent_encloses((byte*)shell,5,32,tmp,6,FALSE,(byte*)sphere,6,32));
 memcpy(tmp,shell_indices,sizeof(tmp));for(int i=0;i<18;i+=3){unsigned short s=tmp[i];tmp[i]=tmp[i+1];tmp[i+1]=s;}
 CHECK(!rasterizer_transparent_encloses((byte*)shell,5,32,tmp,6,FALSE,(byte*)sphere,6,32));
 for(int start=0;start<2;start++){
  init_pair(0);rasterizer_transparent_geometry_model_end(0);CHECK(transparent_geometry_group_count==2);
  memset(pending,0,sizeof(pending));nsequence=0;rasterizer_transparent_geometry_group_draw(storage+start,FALSE);
  CHECK(nsequence==3&&sequence[0]==1&&sequence[1]==2&&sequence[2]==3);
  rasterizer_transparent_geometry_group_draw(storage+(1-start),FALSE);CHECK(nsequence==3);
 }
 /* Eligibility failures must leave both complete packets byte-identical. */
 for(int c=0;c<22;c++){
  init_pair(0);
  switch(c){
   case 0:storage[0].node_matrix_count=2;break;case 1:storage[0].effect_type=1;break;
   case 2:storage[0].previous_group_presorted_index=1;break;case 3:storage[1].next_group_presorted_index=0;break;
   case 4:storage[0].geometry_flags=FLAG(7);break;case 5:energy_material.generic.extra_layers.count=1;break;
   case 6:energy_material.generic.framebuffer_blend_function=0;break;case 7:energy_material.generic.framebuffer_fade_mode=1;break;
   case 8:energy_material.shader.radiosity.flags=4;break;case 9:glass_material.flags=0;break;
   case 10:glass_material.reflection_type=2;break;case 11:storage[0].object_index=999;break;
   case 12:storage[0].node_matrices=(void*)1;break;case 13:storage[0].vertex_buffer=NULL;break;
   case 14:storage[1].cortana_hack=TRUE;break;case 15:storage[0].geometry_flags=FLAG(8);break;
   case 16:storage[0].geometry_flags=FLAG(1);break;case 17:energy_material.generic.lens_flare.index=0;break;
   case 18:storage[0].shader=storage[1].shader;break;case 19:energy_material.generic.flags|=2;break;
   case 20:storage[1].triangle_count=5;break;case 21:transparent_geometry_group_count=3;break;
  }
  struct transparent_geometry_group before[2];memcpy(before,storage,sizeof(before));
  rasterizer_transparent_geometry_model_end(0);CHECK(!memcmp(before,storage,sizeof(before)));
 }
 /* No extra queue slots, even when full; independent objects/co-op views. */
 init_pair(382);rasterizer_transparent_geometry_model_end(382);CHECK(transparent_geometry_group_count==384);
 for(int window=0;window<4;window++){
  memset(pending,0,sizeof(pending));init_pair(0);rasterizer_transparent_geometry_model_end(0);
  init_pair(2);rasterizer_transparent_geometry_model_end(2);nsequence=0;
  rasterizer_transparent_geometry_group_draw(storage+3,FALSE);rasterizer_transparent_geometry_group_draw(storage,FALSE);
  CHECK(nsequence==6&&sequence[0]==1&&sequence[3]==1);
 }
 struct transparent_geometry_group ordinary={0};CHECK(transparent_glass_cull_mode(&ordinary,4)==D3DCULL_NONE);
 CHECK(transparent_glass_cull_mode(&ordinary,0)==D3DCULL_CCW);
 puts("PASS: enclosure geometry, open/outside/reversed/invalid rejection, 22 unchanged fallback paths, pending/order, full queue, independent objects and four views");
}
static void asset_test(const char *path){
 FILE *f=fopen(path,"rb");CHECK(f);unsigned int n[3];CHECK(fread(n,sizeof(n),1,f)==1);
 byte *v=malloc(n[0]*32),*inner=malloc(n[1]*32);word *ix=malloc((n[2]+2)*2);
 CHECK(fread(v,32,n[0],f)==n[0]);CHECK(fread(inner,32,n[1],f)==n[1]);CHECK(fread(ix,2,n[2]+2,f)==n[2]+2);
 CHECK(rasterizer_transparent_encloses(v,n[0],32,ix,n[2],TRUE,inner,n[1],32));
 init_pair(0);
 CHECK(fread(&energy_material,sizeof(energy_material),1,f)==1);CHECK(fread(&glass_material,sizeof(glass_material),1,f)==1);
 struct test_buffer av={v},bv={inner},ib={(byte*)ix};
 struct vertex_buffer a={5,0,n[0],0,NULL,&av},b={5,0,n[1],0,NULL,&bv};
 struct triangle_buffer t={1,0,n[2],NULL,&ib};
 storage[0].vertex_buffer=&b;storage[1].vertex_buffer=&a;storage[1].triangle_buffer=&t;storage[1].triangle_count=n[2];
 rasterizer_transparent_geometry_model_end(0);CHECK(TEST_FLAG(storage[0].geometry_flags,_rasterizer_geometry_enclosed_energy_bit));
 free(v);free(inner);free(ix);fclose(f);puts("PASS: actual Xbox overshield materials and strip meshes qualify for the production enclosure path");
}
int main(int argc,char **argv){
 tests();if(argc>1)asset_test(argv[1]);
 CHECK(SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 SDL_Window *window=SDL_CreateWindow("enclosure regression",64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
 SDL_GLContext ctx=SDL_GL_CreateContext(window);CHECK(ctx);
 #define LOAD(name) name=(__typeof__(name))SDL_GL_GetProcAddress(#name);
 GL_FUNCTIONS(LOAD)
 #undef LOAD
 glClipControl(GL_UPPER_LEFT,GL_NEGATIVE_ONE_TO_ONE);glDisable(GL_DITHER);glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glViewport(0,0,64,64);
 const char *vs="#version 450 core\nlayout(location=0)in vec3 p;uniform vec4 right,up,forward;out vec3 point;void main(){vec3 q=p-vec3(0,0,.7);point=p;gl_Position=vec4(dot(q,right.xyz)*.7,dot(q,up.xyz)*.7,dot(q,forward.xyz)*.3,1);}";
 const char *fs="#version 450 core\nin vec3 point;uniform int kind;out vec4 color;void main(){color=kind==0?vec4(.5,.6,.8,1):kind==2?vec4(.015,.025,.03,0):vec4(.3+.18*point.x,.35+.18*point.y,.4+.1*point.z,0);}";
 GLuint v=compile(GL_VERTEX_SHADER,vs),f=compile(GL_FRAGMENT_SHADER,fs);gpu_program=glCreateProgram();glAttachShader(gpu_program,v);glAttachShader(gpu_program,f);glLinkProgram(gpu_program);
 GLint ok;glGetProgramiv(gpu_program,GL_LINK_STATUS,&ok);CHECK(ok);glUseProgram(gpu_program);
 glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,FALSE,12,NULL);
 byte result[64*64*4],other[64*64*4],old1[64*64*4],old2[64*64*4];int differences=0,renders=0;
 for(int pitch=-2;pitch<=2;pitch++)for(int yaw=0;yaw<12;yaw++)for(reflection=0;reflection<2;reflection++){
  float a=yaw*6.2831853f/12,p=pitch*.24f,c=cosf(p),s=sinf(p);
  uniform3(glGetUniformLocation(gpu_program,"right"),-sinf(a),cosf(a),0);
  uniform3(glGetUniformLocation(gpu_program,"up"),-s*cosf(a),-s*sinf(a),c);
  uniform3(glGetUniformLocation(gpu_program,"forward"),c*cosf(a),c*sinf(a),s);
  render(result,TRUE,0);render(other,TRUE,1);CHECK(!memcmp(result,other,sizeof(result)));
  render(old1,FALSE,0);render(old2,FALSE,1);
  for(int i=0;i<sizeof(old1);i++)differences+=old1[i]!=old2[i];
  /* One front tint must attenuate the additive sphere. At the centre
     pyramid/octahedron each contribute exactly two visible surfaces. */
  int at=4*(32*64+32);float expected=255*(.1f*.5f*.5f+2*.3f*.5f+(reflection?.015f*(1+.5f):0));
  CHECK(fabsf(result[at]-expected)<3.5f);CHECK(old1[at]>result[at]);
  renders+=4;
 }
 CHECK(differences>0);printf("PASS: %d real GL blend probes, 60 camera angles, tint/reflection passes; both queue entry orders identical; legacy order reproduces brightness reversal\n",renders);
 SDL_GL_DestroyContext(ctx);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
'''

def asset_fixture(path):
    raw = path.read_bytes()
    data = raw if struct.unpack_from('<I', raw, 8)[0] == len(raw) else raw[:2048] + zlib.decompressobj().decompress(raw[2048:])
    tag_offset = struct.unpack_from('<I', data, 16)[0]
    u = lambda p: struct.unpack_from('<I', data, p)[0]
    base = u(tag_offset) - 36
    off = lambda p: p - base + tag_offset
    model = None
    tags={}
    for i in range(u(tag_offset+12)):
        o=tag_offset+36+32*i
        tags[u(o+12)]=off(u(o+20))
        if data[o:o+4] != b'edom': continue
        name=off(u(o+16))
        if data[name:data.index(b'\0',name)] == b'powerups\\over shield\\over shield': model=off(u(o+20))
    assert model is not None, 'overshield model missing'
    geometry=off(u(model+212)); parts=off(u(geometry+40))
    sphere,glass=parts,parts+104
    vertices=lambda p: data[off(u(off(u(p+100))+4)):off(u(off(u(p+100))+4))+u(p+88)*32]
    indices=off(u(off(u(glass+80))+4)); count=u(glass+72)
    shaders=off(u(model+224))
    def material(p,size):
        index=struct.unpack_from('<h',data,p+4)[0]
        offset=tags[u(shaders+32*index+12)]
        return data[offset:offset+size]
    return struct.pack('<III',u(glass+88),u(sphere+88),count)+vertices(glass)+vertices(sphere)+data[indices:indices+2*(count+2)]+material(sphere,108)+material(glass,480)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc',default='clang');parser.add_argument('--map',type=Path)
    args=parser.parse_args()
    source=(ROOT/'source/rasterizer/xbox/rasterizer_xbox_transparent_geometry.c').read_text()
    # Every glass pass must honor the face restriction, not just its tint.
    assert source.count('transparent_glass_cull_mode(group, glass->flags)')==3
    enum_names=['_rasterizer_geometry_no_sort_bit','_shader_type_screen','_render_model_effect_type_none',
                '_shader_radiosity_FILTHY_transparent_lit_bit','_shader_transparent_flag_alpha_tested_bit',
                '_shader_transparent_glass_flag_alpha_tested_bit','_shader_transparent_glass_reflection_type_bumped_cube_map',
                '_framebuffer_fade_mode_none','_framebuffer_blend_function_alpha_blend']
    enums=(ROOT/'port/include/xdk/xdk_pdb.h').read_text()
    unit=PRELUDE.replace('#include "xgpu.h"',block(enums,'enum _D3DRENDERSTATETYPE {')+';\n#include "xgpu.h"')+block(enums,'enum _D3DCULL {')+';\n'+block(enums,'enum _D3DFRONT {')+';\n'
    for name in enum_names:
        unit+=block(source,'enum\n{\n\t'+name)+';\n'
    header=(ROOT/'source/rasterizer/rasterizer_transparent_geometry.h').read_text()
    unit+=block(header,'enum')+';\n'
    geo=(ROOT/'source/rasterizer/rasterizer_geometry.h').read_text()
    unit+=block(geo,'enum')+';\n'+block(geo,'struct vertex_buffer\n')+';\n'+block(geo,'struct triangle_buffer\n')+';\n'
    unit+=block(geo,'enum\n{\n\t_triangle_buffer_type_triangles')+';\n'
    shaders=(ROOT/'source/shaders/shader_definitions.h').read_text()
    for name in ['shader_radiosity_properties','shader_physics_properties','shader_base','shader']:
        unit+=block(shaders,'struct '+name+'\n')+';\n'
    for name in ['shader_transparent_generic','shader_transparent_generic_definition','shader_transparent_glass_definition','transparent_geometry_group']:
        unit+=block(source,'struct '+name+'\n')+';\n'
    unit+=HARNESS
    core=(ROOT/'source/rasterizer/rasterizer_transparent_geometry.c').read_text()
    unit+=block(core,'long rasterizer_transparent_geometry_model_begin(')+'\n'
    math=(ROOT/'source/math/real_math.h').read_text()
    unit+=block(math,'struct real_matrix4x3\n')+';\ntypedef struct real_matrix4x3 real_matrix4x3;\n'
    unit+=block((ROOT/'source/math/matrix_math.c').read_text(),'real_point3d *matrix4x3_transform_point(')+'\n'
    unit+=(ROOT/'source/rasterizer/rasterizer_transparent_enclosure.h').read_text()+'\n'
    for marker in ['static boolean transparent_group_encloses(', 'void rasterizer_transparent_geometry_model_end(',
                   'static boolean transparent_enclosure_world_bounds(', 'static short transparent_plane_enclosure_order(',
                   'static boolean transparent_apply_plane_dependencies(', 'void rasterizer_transparent_geometry_order_enclosures(',
                   'static DWORD transparent_glass_cull_mode(', 'static void transparent_enclosure_draw_back(']:
        unit+=block(source,marker)+'\n'
    # Production dispatch/pending/link code, with only the material body replaced
    # by GL blend probes. Keep both recursive edges and the back-pass invocation.
    draw=block(source,'void rasterizer_transparent_geometry_group_draw(\n')
    head=draw[draw.index('\tif ((!group->active_camouflage'):draw.index('\n\t\tif (rasterizer_debug_options.debug_transparent_geometry_enabled)')]
    tail=draw[draw.index('\n\t\tif (group->next_group_presorted_index != NONE)'):draw.index('\n\t\tif (draw_active_camouflage_groups2)')]
    unit+='static void rasterizer_transparent_geometry_group_draw(struct transparent_geometry_group *group,boolean dirty){\n'+head+'\nprobe_draw(group);'+tail+'\n}}\n'
    renderer=(ROOT/'port/linux/src/d3d8_gl.c').read_text()
    cull=renderer[renderer.index('\tstate_enable(&gl_state.cull_face,'):renderer.index('#ifndef HALO_ANDROID\n\t/* ES draws filled polygons')]
    unit+=block(core,'static int __cdecl group_sorted_indices_cmpfn(')+'\n'+TESTS.replace('CULL_TRANSLATION',cull)
    compiler=[args.cc,'-std=gnu11','-O2','-fuse-ld=lld','-I'+str(ROOT/'port/linux/src')]
    env=dict(os.environ)
    if sys.platform=='win32':
        sdl=next((ROOT/'build/windows/third_party').glob('SDL3-*/include')).parent
        compiler+=['--target=i686-pc-windows-msvc','-I'+str(sdl/'include')]
        libraries=[str(sdl/'lib/x86/SDL3.lib')];env['PATH']=str(sdl/'lib/x86')+os.pathsep+env['PATH']
    else:
        compiler+=subprocess.check_output(['pkg-config','--cflags','sdl3'],text=True).split()
        libraries=subprocess.check_output(['pkg-config','--libs','sdl3'],text=True).split()+['-lm']
    with tempfile.TemporaryDirectory(prefix='halo-enclosure-test-') as directory:
        path=Path(directory);(path/'test.c').write_text(unit)
        command=[str(path/'test.exe')]
        if args.map:
            (path/'asset.bin').write_bytes(asset_fixture(args.map));command.append(str(path/'asset.bin'))
        subprocess.run([*compiler,str(path/'test.c'),*libraries,'-o',str(path/'test.exe')],check=True)
        subprocess.run(command,env=env,check=True,timeout=45)

if __name__=='__main__': main()
