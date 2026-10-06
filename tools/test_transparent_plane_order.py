"""Production planar-glass ordering, conservative fallbacks, and GL overlap probes.

Uses the enclosure harness's real sorting/dispatch/cull code with simple blend
materials. --map also validates the local Xbox pickup; no assets are committed.
"""
import test_transparent_enclosure as probe

PLANE_TESTS = r'''
static real_matrix4x3 root_matrix={1, .forward={1,0,0},.left={0,1,0},.up={0,0,1}};
static float floor_vertices[4][8]={{-4,-4,-.05f},{7,-4,-.05f},{7,4,-.05f},{-4,4,-.05f}};
static unsigned short floor_indices[]={0,1,2,0,2,3};
static void floor_scene(void){
 init_pair(0);root_matrix=(real_matrix4x3){1,.forward={1,0,0},.left={0,1,0},.up={0,0,1}};
 storage[0].node_matrices=storage[1].node_matrices=&root_matrix;
 rasterizer_transparent_geometry_model_end(0);
 memset(storage+2,0,sizeof(storage[2]));storage[2].shader=(struct shader*)&glass_material;
 storage[2].geometry_flags=FLAG(_rasterizer_geometry_no_sort_bit);
 storage[2].plane.n.k=1;storage[2].plane.d=-.05f;
 storage[2].previous_group_presorted_index=storage[2].next_group_presorted_index=NONE;
 storage[2].sorted_index=2;transparent_geometry_group_count=3;
 global_window_parameters.camera.position=(real_point3d){0,0,4};
}
static void plane_tests(void){
 short order[384],old[384];real bounds[2][3];
 floor_scene();CHECK(transparent_enclosure_world_bounds(storage+1,bounds));
 CHECK(bounds[0][2]==0&&bounds[1][2]==2);
 for(int below=0;below<2;below++)for(int flipped=0;flipped<2;flipped++){
  floor_scene();if(below)global_window_parameters.camera.position.z=-1;
  if(flipped){storage[2].plane.n.k=-1;storage[2].plane.d=.05f;}
  order[0]=0;order[1]=1;order[2]=2;
  rasterizer_transparent_geometry_order_enclosures(order,3);
  CHECK(order[below?2:0]==2);memcpy(old,order,6);
  rasterizer_transparent_geometry_order_enclosures(order,3);CHECK(!memcmp(old,order,6));
 }
 /* A grounded shell may touch the plane, without intersecting it. */
 floor_scene();storage[2].plane.d=0;order[0]=0;order[1]=1;order[2]=2;
 rasterizer_transparent_geometry_order_enclosures(order,3);CHECK(order[0]==2);
 /* Retain the complete original order for unsupported or ambiguous cases. */
 for(int c=0;c<20;c++){
  floor_scene();order[0]=0;order[1]=1;order[2]=2;memcpy(old,order,6);
  switch(c){
   case 0:storage[2].plane.d=1;break;case 1:storage[2].plane.n.k=0;break;
   case 2:storage[2].plane.n.k=NAN;break;case 3:storage[2].plane.d=NAN;break;
   case 4:global_window_parameters.camera.position.z=-.05f;break;
   case 5:storage[2].geometry_flags=0;break;case 6:storage[2].object_index=1;break;
   case 7:storage[2].node_matrix_count=1;break;case 8:storage[2].shader=NULL;break;
   case 9:storage[2].next_group_presorted_index=0;break;
   case 10:storage[1].node_matrices=NULL;break;case 11:root_matrix.scale=NAN;break;
   case 12:storage[2].cortana_hack=TRUE;break;
   case 13:storage[2].geometry_flags|=FLAG(_rasterizer_geometry_first_person_bit);break;
   case 14:storage[2].effect_type=1;break;
   case 15:glass_material.reflection_type=_shader_transparent_glass_reflection_type_dynamic_mirror;break;
   case 16:glass_material.flags|=FLAG(_shader_transparent_glass_flag_decal_bit);break;
   case 17:storage[2].source_object_index=1;break;
   case 18:global_window_parameters.camera.position.x=NAN;break;
   case 19:storage[2].active_camouflage_transparent_source_object_index=1;break;
  }
  struct transparent_geometry_group packets[3];memcpy(packets,storage,sizeof(packets));
  rasterizer_transparent_geometry_order_enclosures(order,3);
  CHECK(!memcmp(old,order,6)&&!memcmp(packets,storage,sizeof(packets)));
 }
 /* Both vertex formats and transformed bounds, including negative scale. */
 float uncompressed[5][17];for(int i=0;i<5;i++)memcpy(uncompressed[i],shell[i],12);
 struct test_buffer ub={(byte*)uncompressed};struct vertex_buffer uv={4,0,5,0,NULL,&ub};
 for(int format=0;format<2;format++)for(int a=0;a<16;a++)for(int scale=0;scale<2;scale++){
  floor_scene();if(format)storage[1].vertex_buffer=&uv;
  float angle=a*6.2831853f/16;root_matrix.forward=(real_vector3d){cosf(angle),sinf(angle),0};
  root_matrix.left=(real_vector3d){-sinf(angle),cosf(angle),0};root_matrix.scale=scale?-2:1;
  root_matrix.position=(real_point3d){10,-20,scale?5:3};
  storage[2].plane.d=0;order[0]=0;order[1]=1;order[2]=2;
  rasterizer_transparent_geometry_order_enclosures(order,3);CHECK(order[0]==2);
 }
 /* Below-floor camera and below-floor pickup: the floor is background again. */
 floor_scene();root_matrix.position.z=-3;global_window_parameters.camera.position.z=-4;
 order[0]=0;order[1]=1;order[2]=2;rasterizer_transparent_geometry_order_enclosures(order,3);CHECK(order[0]==2);
 /* Vertical/oblique planes use the transformed bounds, not the object centre. */
 for(int flipped=0;flipped<2;flipped++){
  floor_scene();storage[2].plane=(real_plane3d){{1,0,0},3};global_window_parameters.camera.position.x=4;
  if(flipped){storage[2].plane.n.i=-1;storage[2].plane.d=-3;}
  order[0]=2;order[1]=0;order[2]=1;rasterizer_transparent_geometry_order_enclosures(order,3);CHECK(order[2]==2);
 }
 /* Full queue: no allocation or added group; floor precedes both packets. */
 floor_scene();for(int i=3;i<384;i++){memset(storage+i,0,sizeof(storage[i]));storage[i].previous_group_presorted_index=storage[i].next_group_presorted_index=NONE;}
 transparent_geometry_group_count=384;for(int i=0;i<384;i++)order[i]=i;
 rasterizer_transparent_geometry_order_enclosures(order,384);CHECK(order[0]==2&&order[1]==0&&order[2]==1);
 CHECK(transparent_geometry_group_count==384);for(int i=3;i<384;i++)CHECK(order[i]==i);
 /* Two pickups on opposite sides, independently sorted for four camera views. */
 for(int window=0;window<4;window++){
  floor_scene();real_matrix4x3 lower=root_matrix;lower.position.z=-3;
  init_pair(3);storage[3].node_matrices=storage[4].node_matrices=&lower;
  rasterizer_transparent_geometry_model_end(3);
  if(window&1)global_window_parameters.camera.position.z=-4;
  if(window&2){storage[2].plane.n.k=-1;storage[2].plane.d=.05f;}
  for(int i=0;i<5;i++)order[i]=i;
  rasterizer_transparent_geometry_order_enclosures(order,5);
  CHECK(order[2]==2);CHECK(order[0]==((window&1)?0:3));
  CHECK(order[1]==((window&1)?1:4));CHECK(order[3]==((window&1)?3:0));
  memset(pending,0,sizeof(pending));nsequence=0;
  for(int i=0;i<5;i++)rasterizer_transparent_geometry_group_draw(storage+order[i],FALSE);
  CHECK(nsequence==7&&sequence[0]==1&&sequence[1]==2&&sequence[2]==3&&sequence[3]==5&&sequence[4]==1&&sequence[5]==2&&sequence[6]==3);
 }
 /* Cyclic constraints must return the original order, including any prefix. */
 unsigned long before[384][12]={{0}};order[0]=2;order[1]=1;order[2]=0;memcpy(old,order,6);
 BIT_VECTOR_SET_FLAG(before[0],1,TRUE);BIT_VECTOR_SET_FLAG(before[1],2,TRUE);BIT_VECTOR_SET_FLAG(before[2],0,TRUE);
 CHECK(!transparent_apply_plane_dependencies(order,3,before)&&!memcmp(old,order,6));
 memset(before,0,sizeof(before));order[0]=3;order[1]=2;order[2]=1;order[3]=0;memcpy(old,order,8);
 BIT_VECTOR_SET_FLAG(before[1],2,TRUE);BIT_VECTOR_SET_FLAG(before[2],3,TRUE);BIT_VECTOR_SET_FLAG(before[3],1,TRUE);
 CHECK(!transparent_apply_plane_dependencies(order,4,before)&&!memcmp(old,order,8));
 puts("PASS: plane sides/reversed normals, grounded contact, 20 unchanged fallbacks, both vertex formats, 64 transforms, full queue, four views/two pickups, idempotence and cycle rollback");
}
static void floor_render(byte *pixels,float fx,float fy,float fz,int corrected,int reference){
 floor_scene();memset(pending,0,sizeof(pending));nsequence=0;
 global_window_parameters.camera.position=(real_point3d){-10*fx,-10*fy,.7f-10*fz};
 storage[0].z_sort=storage[1].z_sort=-fz*.7f;storage[2].z_sort=-(fx*1.5f-fz*.05f);
 if(corrected){rasterizer_sort_external();for(int i=0;i<3;i++)CHECK(storage[transparent_geometry_group_sorted_indices[i]].sorted_index==i);}
 else {for(int i=0;i<3;i++)transparent_geometry_group_sorted_indices[i]=i;qsort(transparent_geometry_group_sorted_indices,3,sizeof(short),group_sorted_indices_cmpfn);}
 glDepthMask(TRUE);glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(FALSE);rendering=1;
 if(reference){rasterizer_transparent_geometry_group_draw(storage+2,FALSE);rasterizer_transparent_geometry_group_draw(storage,FALSE);rasterizer_transparent_geometry_group_draw(storage+1,FALSE);}
 else for(int i=0;i<3;i++)rasterizer_transparent_geometry_group_draw(storage+transparent_geometry_group_sorted_indices[i],FALSE);
 rendering=0;glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);CHECK(glGetError()==GL_NO_ERROR);
 CHECK(nsequence==4); /* floor + back + energy + front, exactly once */
}
'''

def main():
    core=(probe.ROOT/'source/rasterizer/rasterizer_transparent_geometry.c').read_text()
    probe.TESTS=(probe.block(core,'static void rasterizer_sort_internal(')+'\n'+
                 probe.block(core,'static void rasterizer_sort_external(\n\tvoid)\n{')+'\n'+probe.TESTS)
    # Production material body remains a simple blend probe; floor is a separate packet.
    probe.TESTS=probe.TESTS.replace('static void probe_draw(struct transparent_geometry_group const *g){', '''
static float floor_vertices[4][8];static unsigned short floor_indices[6];
static void probe_draw(struct transparent_geometry_group const *g){
 if(g==storage+2&&g->node_matrix_count==0&&g->shader&&g->shader->base.type==8){
  sequence[nsequence++]=5;if(!rendering)return;
  set_cull(D3DCULL_NONE);glUniform1i(glGetUniformLocation(gpu_program,"kind"),0);
  glBlendFunc(GL_ZERO,GL_SRC_COLOR);draw_mesh((float*)floor_vertices,32,floor_indices,6);return;
 }
''')
    probe.TESTS=probe.TESTS.replace('int main(int argc,char **argv){',PLANE_TESTS+'\nint main(int argc,char **argv){\n plane_tests();')
    probe.TESTS=probe.TESTS.replace('SDL_GL_DestroyContext(ctx);',r'''
 reflection=0;int changed=0,renders_floor=0;
 for(int pitch=-2;pitch<0;pitch++)for(int yaw=0;yaw<12;yaw++){
  float a=yaw*6.2831853f/12,p=pitch*.24f,c=cosf(p),s=sinf(p);
  uniform3(glGetUniformLocation(gpu_program,"right"),-sinf(a),cosf(a),0);
  uniform3(glGetUniformLocation(gpu_program,"up"),-s*cosf(a),-s*sinf(a),c);
  uniform3(glGetUniformLocation(gpu_program,"forward"),c*cosf(a),c*sinf(a),s);
  floor_render(result,c*cosf(a),c*sinf(a),s,TRUE,FALSE);
  floor_render(other,c*cosf(a),c*sinf(a),s,TRUE,TRUE);
  CHECK(!memcmp(result,other,sizeof(result)));
  floor_render(old1,c*cosf(a),c*sinf(a),s,FALSE,FALSE);
  int at=4*(32*64+32)+2;changed+=other[at]>old1[at]+3;renders_floor+=3;
 }
 CHECK(changed>0);printf("PASS: %d real GL floor overlap probes, correct reference pixels at all 24 views; old order loses pickup glow in %d views; depth test ON, writes OFF\n",renders_floor,changed);
 SDL_GL_DestroyContext(ctx);
''')
    probe.main()

if __name__=='__main__': main()
