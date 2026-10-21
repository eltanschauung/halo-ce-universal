#include "harness.h"
#include <math.h>
#include <limits.h>
typedef unsigned short word;
#define MIN(a,b) ((a)<(b)?(a):(b))
enum { _game_connection_local, _game_connection_network_client, _game_connection_network_server };
enum { _friendly_fire_on, _friendly_fire_half, _friendly_fire_quarter, _friendly_fire_off };
enum { _distributed_message_grenade_collision_policy=249, _distributed_to_clients_reliably=1 };
enum { _object_type_biped, _object_type_vehicle, _object_type_weapon, _object_type_projectile };
enum { _object_invisible_bit, _collision_test_objects_first_type_bit=4, _collision_test_use_vehicle_physics_bit=10 };
enum { _object_mask_vehicle=2, _collision_result_object=1, NUMBER_OF_UNIT_GRENADE_TYPES=2 };
#define _collision_test_for_projectiles_flags 0x00f0UL
#define _collision_test_for_projectiles_fat_flags 0x00f0UL
#define _real_epsilon 0.00001f
#define GET_MESSAGE_TYPE(h) (((h)>>2)&3)
struct distributed_message_header { word header; byte type,count; long game_time; };
struct game_variant_options { short friendly_fire; } options;
struct network_game { struct game_variant_options variant_options; } game;
struct player_datum { long team_index,unit_index; } players[4];
struct object_data {
 short type; unsigned flags; real_point3d bounding_sphere_center; real bounding_sphere_radius;
 long next_object_index,first_child_object_index,parent_object_index,owner_player_index;
 real_point3d position;
};
struct object_datum { long definition_index; struct object_data object; };
struct unit_datum { long definition_index; struct object_data object; struct {long player_index;}unit; } objects[8];
struct projectile_datum { long definition_index; struct object_data object; struct {long ignore_object_index;}projectile; } projectile;
struct projectile_definition { struct {real collision_radius;}projectile; } definition;
struct game_globals_grenade {struct {long index;}projectile;} grenade_tags[2]={{{100}},{{101}}};
struct game_globals {struct {long count;struct game_globals_grenade *address;}grenades;} globals={2,grenade_tags};
#define TAG_BLOCK_GET_ELEMENT(b,i,t) ((b)->address+(i))
typedef struct {real n,d;} real_plane3d;
struct collision_result { int type; real t; real_plane3d plane; long material_type,object_index,region_index,node_index,bsp_index,surface_index,plane_designator,flags,breakable_surface_index,material_index; };
typedef boolean (*collision_object_filter)(long,void const*);
struct physics_instance {long object;};
struct physics_test_vector_result {real t;real_plane3d plane;};
struct collision_model_instance {long object,model; int matrices[1];};
struct collision_model_test_vector_result {struct {real t;real_plane3d *plane;long plane_designator,material_index,surface_index,flags,breakable_surface_index;}bsp_result;long node_index,region_index,bsp_index;};
static int role,playing=1,menu,coop=1,teams=1,matching=1,capable=1,sends,sweeps,geometry;
static unsigned long now=1000;
static real world_wall=1000.f;
static int count=3;
static int game_connection(void){return role;}
static boolean game_in_progress(void){return playing;}
static boolean main_menu_is_active(void){return menu;}
static boolean network_coop_active(void){return coop;}
static boolean game_engine_has_teams(void){return teams;}
static struct network_game *network_game_get_game(void){return &game;}
static struct game_variant_options *game_variant_options_get(void){return &options;}
static struct game_globals *scenario_get_game_globals(void){return &globals;}
static struct player_datum *player_try_and_get(long p){return p>=0&&p<4?players+p:NULL;}
static struct object_datum *object_get(long o){CHECK(o>=0&&o<count,"bad object");return (struct object_datum*)(objects+o);}
static struct object_datum *object_try_and_get(long o){return o>=0&&o<count?object_get(o):NULL;}
static struct projectile_datum *projectile_get(long p){CHECK(p==10,"bad projectile");return &projectile;}
static struct projectile_definition *projectile_definition_get(long d){return &definition;}
static boolean network_extensions_allows(long m,byte t){return capable;}
static boolean network_extensions_balance_matches(byte t){return matching;}
unsigned long system_milliseconds(void){return now;}
static void distributed_send(void *m,byte t,int c,word n,int route){sends++;}
static void vector_from_points3d(real_point3d const*a,real_point3d const*b,real_vector3d*v){v->i=b->x-a->x;v->j=b->y-a->y;v->k=b->z-a->z;}
static void point_from_line3d(real_point3d const*p,real_vector3d const*v,real t,real_point3d*q){*q=(real_point3d){p->x+t*v->i,p->y+t*v->j,p->z+t*v->k};}
static real_vector3d up={0,0,1},left={0,1,0};
static real_vector3d const *global_up3d=&up,*global_left3d=&left;
static real_vector3d *cross_product3d(real_vector3d const*a,real_vector3d const*b,real_vector3d*c){*c=(real_vector3d){a->j*b->k-a->k*b->j,a->k*b->i-a->i*b->k,a->i*b->j-a->j*b->i};return c;}
static real normalize3d(real_vector3d*v){real n=sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);if(n){v->i/=n;v->j/=n;v->k/=n;}return n;}
static boolean fast_vector_intersects_sphere(real_point3d const*p,real_vector3d const*v,real_point3d const*c,real r){return TRUE;}
static boolean physics_instance_new(struct physics_instance*i,long o){i->object=o;return TRUE;}
static boolean physics_test_vector(struct physics_instance*i,real_point3d const*p,real_vector3d const*v,struct physics_test_vector_result*r){return FALSE;}
static boolean collision_model_instance_new(struct collision_model_instance*i,long o){i->object=o;geometry++;return TRUE;}
static boolean collision_model_test_vector(struct collision_model_instance*i,unsigned long f,real_point3d const*p,real_vector3d const*v,struct collision_model_test_vector_result*r){
 static real_plane3d plane;
 real t=(objects[i->object].object.position.x-p->x)/v->i;
 if(t<0||t>1||fabsf(p->y-objects[i->object].object.position.y)>.05f)return FALSE;
 memset(r,0,sizeof(*r));r->bsp_result.t=t;r->bsp_result.plane=&plane;return TRUE;
}
static void matrix4x3_transform_plane(int const*m,real_plane3d const*p,real_plane3d*r){*r=*p;}
static void plane3d_negate(real_plane3d const*p,real_plane3d*r){*r=*p;}
static long collision_model_get_material_type(long m,long i){return 0;}
#include "objects.inc"
static boolean collision_test_vector_filtered(unsigned long f,real_point3d const*p,real_vector3d const*v,long ignore,struct collision_result*r,collision_object_filter filter,void const*context){
 int hit=0; sweeps++;memset(r,0,sizeof(*r));r->t=1;r->object_index=NONE;
 real t=(world_wall-p->x)/v->i;
 if(t>=0&&t<r->t){r->t=t;r->type=2;hit=1;}
 // Broadphase lists each root; actual production recursion handles children.
 for(int i=0;i<count;i++)if(objects[i].object.parent_object_index==NONE)
  if(object_test_vector(i,f,0,p,v,ignore,r,filter,context))hit=1;
 return hit;
}
static boolean collision_test_line(unsigned long f,real_point3d const*p,real_point3d const*q,long ignore,struct collision_result*r){real_vector3d v;vector_from_points3d(p,q,&v);return collision_test_vector_filtered(f,p,&v,ignore,r,NULL,NULL);}
#include "policy.inc"
#include "rays.inc"
static void setup(void){
 friendly_grenades_reset();
 options.friendly_fire=game.variant_options.friendly_fire=_friendly_fire_half;
 for(int i=0;i<8;i++){
  objects[i].object=(struct object_data){.type=_object_type_biped,.position={2.f+i,0,0},.next_object_index=NONE,.first_child_object_index=NONE,.parent_object_index=NONE};
  objects[i].unit.player_index=i<3?i:NONE;
 }
 for(int i=0;i<4;i++)players[i]=(struct player_datum){.team_index=i<2?0:1,.unit_index=i};
 projectile.definition_index=100;projectile.object.owner_player_index=0;projectile.object.position=(real_point3d){0,0,0};projectile.projectile.ignore_object_index=0;
 friendly_grenades_update();
}
static int sweep(struct collision_result*r){real_point3d end={10,0,0};return friendly_grenades_collision_test_line(10,_collision_test_for_projectiles_flags,&projectile.object.position,&end,0,r);}
static struct grenade_collision_policy policy(int state){return (struct grenade_collision_policy){{8,249,1,0},state,0};}
int main(int argc,char**argv){
 CHECK(argc==2,"case");char const*case_name=argv[1];struct collision_result hit;setup();
 CASE("settings"){
  for(int mode=0;mode<4;mode++){
   options.friendly_fire=mode;friendly_grenades_update();sweeps=0;
   int h=sweep(&hit);CHECK(sweeps==1,"retried sweep");
   CHECK(mode==0?(h&&hit.object_index==1):!h,"wrong friendly fire setting %d",mode);
  }
 }else CASE("teams"){
  coop=0;CHECK(sweep(&hit)&&hit.object_index==2,"enemy ignored");
  teams=0;CHECK(sweep(&hit)&&hit.object_index==1,"FFA opponent ignored");
  teams=1;projectile.projectile.ignore_object_index=NONE;
  real_point3d end={10,0,0};
  CHECK(friendly_grenades_collision_test_line(10,0xf0,&projectile.object.position,&end,NONE,&hit)&&hit.object_index==0,"owner immunity changed");
 }else CASE("coop"){
  CHECK(!sweep(&hit)&&hit.t==1&&geometry==0,"ally bounced or reached geometry");
  objects[2].unit.player_index=NONE;
  CHECK(sweep(&hit)&&hit.object_index==2,"AI should stay solid");
  objects[2].unit.player_index=2;players[2].unit_index=7;
  CHECK(sweep(&hit)&&hit.object_index==2,"corpse should stay solid");
 }else CASE("classify"){
  projectile.definition_index=101;CHECK(!sweep(&hit),"plasma collision");
  projectile.definition_index=102;CHECK(sweep(&hit)&&hit.object_index==1,"non-grenade filtered");
  projectile.definition_index=100;projectile.object.owner_player_index=NONE;
  CHECK(sweep(&hit),"unowned grenade filtered");
  projectile.object.owner_player_index=99;CHECK(sweep(&hit),"stale owner filtered");
 }else CASE("hierarchy"){
  count=4;objects[1].object.first_child_object_index=3;objects[3].object.type=_object_type_weapon;objects[3].object.parent_object_index=1;
  CHECK(!sweep(&hit),"held gun collided");
  objects[1].object.first_child_object_index=NONE;objects[3].object.parent_object_index=NONE;
  CHECK(sweep(&hit)&&hit.object_index==3,"dropped gun ignored");
  objects[3].object.type=_object_type_vehicle;objects[3].object.first_child_object_index=1;objects[1].object.parent_object_index=3;
  CHECK(sweep(&hit)&&hit.object_index==3,"vehicle ignored");
 }else CASE("wall"){
  world_wall=3.1f;CHECK(sweep(&hit)&&hit.type==2&&fabsf(hit.t-.31f)<.00001f,"wall behind ally skipped");
  world_wall=1.f;CHECK(sweep(&hit)&&hit.t==.1f,"closer wall lost");
  world_wall=1000;coop=0;CHECK(sweep(&hit)&&hit.object_index==2,"enemy behind ally skipped");
 }else CASE("rays"){
  real_point3d end={10,0,0};definition.projectile.collision_radius=.2f;
  objects[1].object.position.y=.2f;objects[2].object.position.y=-.2f;sweeps=0;
  CHECK(!projectile_collision_test_line(10,&end,&hit)&&sweeps==3,"fat ray hit ally");
  coop=0;CHECK(projectile_collision_test_line(10,&end,&hit)&&hit.object_index==2,"fat ray lost enemy");
 }else CASE("mixed"){
  role=2;matching=0;friendly_grenades_update();CHECK(sweep(&hit),"upstream peer filtered");
  matching=1;friendly_grenades_update();CHECK(!sweep(&hit),"matching peers collided");
  game.variant_options.friendly_fire=_friendly_fire_on;friendly_grenades_update();CHECK(sweep(&hit),"host On filtered");
  role=1;friendly_grenades_reset();friendly_grenades_update();CHECK(sweep(&hit),"client filtered before policy");
  struct grenade_collision_policy m=policy(1);friendly_grenades_handle(NONE,(word*)&m,sizeof(m),TRUE);CHECK(!sweep(&hit),"client ignored host policy");
  capable=0;friendly_grenades_update();CHECK(sweep(&hit),"client retained revoked policy");
 }else CASE("admission"){
  role=1;friendly_grenades_reset();struct grenade_collision_policy m=policy(1);
  for(int n=0;n<sizeof(m);n++)friendly_grenades_handle(NONE,(word*)&m,n,TRUE);
  friendly_grenades_handle(1,(word*)&m,sizeof(m),TRUE);friendly_grenades_handle(NONE,(word*)&m,sizeof(m),FALSE);
  CHECK(!enabled,"invalid sender/size accepted");
  m.enabled=2;friendly_grenades_handle(NONE,(word*)&m,sizeof(m),TRUE);CHECK(!enabled,"bad boolean accepted");
  m=policy(1);m.reserved=1;friendly_grenades_handle(NONE,(word*)&m,sizeof(m),TRUE);CHECK(!enabled,"reserved accepted");
  m=policy(1);friendly_grenades_handle(NONE,(word*)&m,sizeof(m),TRUE);CHECK(enabled,"valid policy rejected");
  friendly_grenades_reset();CHECK(!enabled&&!client_enabled,"reset leaked");
 }else CASE("latency"){
  // Once negotiated, collision prediction never needs another packet. Model
  // host and client independently for 0/100/200ms-delayed remote input ticks.
  for(int delay=0;delay<=6;delay+=3){
   for(int peer=0;peer<2;peer++){
    role=peer?1:2;friendly_grenades_reset();game.variant_options.friendly_fire=_friendly_fire_half;matching=capable=1;
    if(peer){struct grenade_collision_policy m=policy(1);friendly_grenades_handle(NONE,(word*)&m,sizeof(m),TRUE);}else friendly_grenades_update();
    real x=0;int begin=sends;world_wall=1000;
    for(int tick=0;tick<60+delay;tick++){
     if(tick<delay)continue;
     real_point3d from={x,0,0},to={x+.1f,0,0};sweeps=0;
     CHECK(!friendly_grenades_collision_test_line(10,0xf0,&from,&to,0,&hit),"latency collision");
     CHECK(sweeps==1&&sends==begin,"impact retry/network wait");x=to.x;
    }
    CHECK(fabsf(x-6.f)<.0001f,"stalled grenade");
   }
  }
 }else return 2;return 0;
}
