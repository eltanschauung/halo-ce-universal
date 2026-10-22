#include "harness.h"
#include <math.h>
typedef unsigned short word;
typedef struct {real i,j;} real_vector2d;
#define TICKS_PER_SECOND 30
#define SOURCE_MOVE_SPEED (2.25f / TICKS_PER_SECOND)
#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 128
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SET_FLAG(f,b,v) ((v)?((f)|=FLAG(b)):((f)&=~FLAG(b)))
#define CLOSE(a,b) CHECK(fabs((a)-(b))<0.00001,"%g != %g",(double)(a),(double)(b))
enum {_game_connection_local, _game_connection_network_client, _game_connection_network_server};
enum {_biped_airborne_bit, _biped_absolute_movement_bit, _biped_no_collision_bit};
enum {_object_dead_bit, _unit_suspended_bit, _biped_flying_bit, _biped_climbs_anything_bit, _unit_control_jump_bit};
enum {_distributed_message_source_movement=241, _distributed_to_clients_reliably};
enum {_material_effect_biped_jump, _game_difficulty_value_infection_forms};
struct distributed_message_header {word header; byte type,count;long game_time;};
#define GET_MESSAGE_TYPE(h) (((h)>>2)&3)
struct player_datum {short identifier,local_player_index;long unit_index;boolean quit_out_of_game;real speed_multiplier;struct {short machine_index;} network_player_data;};
struct biped_datum {long definition_index;struct {long parent_object_index;unsigned long damage_flags;real_point3d position;real_vector3d translational_velocity,up;} object;struct {long player_index,actor_index,swarm_actor_index;unsigned long flags,control_flags;real body_stun;real_vector3d throttle;struct {int state;}animation;}unit;struct {unsigned long flags;int landing,jump_recovery_timer,support_surface_index;real crouch;}biped;};
enum {_unit_state_leap_start, _unit_state_leap_airborne};
struct biped_definition {struct {unsigned long flags;real jump_velocity;}biped;};
struct game_globals_player_information {real stun_turning_penalty;};
struct {struct game_globals_player_information player_information;} globals;
#define TAG_BLOCK_GET_ELEMENT(block,i,type) (block)
#define scenario_get_game_globals() (&globals)
enum {_damage_area_of_effect_bit};
struct damage_data {long definition_index,owner_player_index;unsigned long flags;real multiplier;real_point3d epicenter;};
struct damage_effect_definition {real cutoff_radius;struct {real damage_upper_bound;}damage;};
static struct damage_effect_definition blast_effect={2.f,{120.f}};
#define damage_effect_definition_get(i) (&blast_effect)
static boolean blast_support=1;
static boolean network_extensions_source_blast(long m){return blast_support;}
static struct {int super_jump,jetpack;}cheat;
static int role,playing=1,menu,cinematic,setting=1,sends;
static unsigned long now=1000;
static boolean support[128];
static struct player_datum roster[4];
static long handles[4]={0x10000,0x20001,0x30002,NONE};
static struct biped_datum body;
static struct biped_definition definition={{0,0.12f}};
static struct data_array *player_data;
struct data_iterator {long index,datum_index;};
static byte wire[1024];static word wire_size;
static int game_connection(void){return role;}
static boolean game_in_progress(void){return playing;}
static boolean main_menu_is_active(void){return menu;}
static boolean cinematic_in_progress(void){return cinematic;}
static int config_boolean(char const *n){return setting;}
static unsigned long system_milliseconds(void){return now;}
static long local_player_get_player_index(int i){return handles[0];}
static struct player_datum *player_try_and_get(long h){for(int i=0;i<3;i++)if(h==handles[i])return &roster[i];return NULL;}
static struct player_datum *player_get(long h){struct player_datum *p=player_try_and_get(h);CHECK(p,"bad player");return p;}
static struct biped_datum *biped_try_and_get(long i){return i==10?&body:NULL;}
static struct biped_datum *biped_get(long i){CHECK(i==10,"bad biped");return &body;}
static struct biped_definition *biped_definition_get(long i){return &definition;}
static boolean network_extensions_allows(long m,byte t){return m==NONE?support[127]:m>=0&&m<128&&support[m];}
static void distributed_send(void *m,byte t,short n,word size,int to){memcpy(wire,m,size);wire_size=size;sends++;}
static void data_iterator_new(struct data_iterator *it,struct data_array *a){memset(it,0,sizeof(*it));it->index=0;}
static void *data_iterator_next(struct data_iterator *it){int i=it->index++;if(i>=3)return NULL;it->datum_index=handles[i];return &roster[i];}
static real normalize3d(real_vector3d *v){real s=sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);if(s>0){v->i/=s;v->j/=s;v->k/=s;}return s;}
static real normalize2d(real_vector2d *v){real s=sqrtf(v->i*v->i+v->j*v->j);if(s>0){v->i/=s;v->j/=s;}return s;}
static real magnitude_squared2d(real_vector2d const *v){return v->i*v->i+v->j*v->j;}
static char const *tag_get_name(long i){static char const *names[]={"weapons\\frag grenade\\explosion","weapons\\plasma grenade\\explosion","weapons\\rocket launcher\\explosion","weapons\\pistol\\bullet","vehicles\\wraith\\explosion"};return i>=0&&i<5?names[i]:NULL;}
static void point_from_line3d(real_point3d const *p,real_vector3d const *d,real t,real_point3d *out){out->x=p->x+d->i*t;out->y=p->y+d->j*t;out->z=p->z+d->k*t;}
static boolean actor_aim_jump(long a,long b,boolean l,real m,real_vector3d *v){return TRUE;}
static void biped_make_footstep(long i,int a,int b){}
#include "api.inc"
#include "under_test.inc"
#include "jump.inc"
static struct source_movement_policy policy(void){struct source_movement_policy p;memset(&p,0,sizeof(p));memset(p.machines,0xFF,sizeof(p.machines));p.header.header=8;p.header.type=241;p.header.count=1;return p;}
int main(int argc,char **argv){CHECK(argc==2,"case");char const *case_name=argv[1];
 for(int i=0;i<3;i++){roster[i].speed_multiplier=1;roster[i].network_player_data.machine_index=i+1;roster[i].unit_index=10;}
 body.unit.player_index=handles[0];body.unit.actor_index=body.unit.swarm_actor_index=NONE;body.object.parent_object_index=NONE;body.object.up.k=1;body.biped.landing=NONE;
 source_movement_reset();real_vector3d v={0},wish={SOURCE_MOVE_SPEED,0,0},normal={0,0,1},forward={1,0,0};
 CASE("ground"){for(int i=0;i<30;i++)source_movement_step(&v,&wish,&normal,FALSE,FALSE);CLOSE(v.i,SOURCE_MOVE_SPEED);}
 else CASE("friction"){v.i=SOURCE_MOVE_SPEED;wish.i=0;source_movement_step(&v,&wish,&normal,FALSE,FALSE);CLOSE(v.i,SOURCE_MOVE_SPEED*(1.f-4.f/30));}
 else CASE("stop"){v.i=.001f;wish.i=0;source_movement_step(&v,&wish,&normal,FALSE,FALSE);CLOSE(v.i,0);}
 else CASE("reverse"){v.i=SOURCE_MOVE_SPEED;wish.i=-wish.i;for(int i=0;i<8;i++)source_movement_step(&v,&wish,&normal,FALSE,FALSE);CHECK(v.i<-.07,"reverse failed");}
 else CASE("diagonal"){real_vector3d desired={.05f,.04f,0};source_movement_wish(&desired,&forward,&wish);CLOSE(wish.i,.05);CLOSE(wish.j,.04);}
 else CASE("crouch"){real_vector3d desired={.025f,0,0};source_movement_wish(&desired,&forward,&wish);CLOSE(wish.i,.025);}
 else CASE("stock-speed"){real_vector3d desired={2.25f/30,0,0};source_movement_wish(&desired,&forward,&wish);for(int i=0;i<90;i++)source_movement_step(&v,&wish,&normal,FALSE,FALSE);CLOSE(v.i,2.25f/30);desired.i=-2.f/30;source_movement_wish(&desired,&forward,&wish);for(int i=0;i<90;i++)source_movement_step(&v,&wish,&normal,FALSE,FALSE);CLOSE(v.i,-2.f/30);}
 else CASE("modifiers"){real_vector3d desired={.15f,.02f,0};forward.i=0;forward.j=1;source_movement_wish(&desired,&forward,&wish);CLOSE(wish.i,-.02);CLOSE(wish.j,.15);desired.i=desired.j=0;source_movement_wish(&desired,&forward,&wish);CLOSE(wish.i,0);CLOSE(wish.j,0);}
 else CASE("air-coast"){v.i=.25;v.j=.1;wish.i=0;source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,.25);CLOSE(v.j,.1);}
 else CASE("air-cap"){source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,30.f/120/30);v.i=.2;source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,.2);}
 else CASE("air-strafe"){v.i=SOURCE_MOVE_SPEED;wish.i=0;wish.j=SOURCE_MOVE_SPEED;source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,SOURCE_MOVE_SPEED);CHECK(v.j>0,"no air strafe");}
 else CASE("bhop"){v.i=.15;source_movement_step(&v,&wish,&normal,FALSE,TRUE);CLOSE(v.i,.15);}
 else CASE("blast-air-control"){v.i=.24f;v.k=.15f;wish.i=0;wish.j=2.25f/30;source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,.24f);CLOSE(v.j,30.f/120/30);CLOSE(v.k,.15f-SOURCE_GRAVITY*.5f);wish.j=0;source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.i,.24f);}
 else CASE("blast-formula"){real_point3d feet={0},explosion={0};source_movement_blast_impulse(&feet,&explosion,0,FALSE,FALSE,100,2,&v);CLOSE(v.k,675.f/3600);CLOSE(v.i,0);source_movement_blast_impulse(&feet,&explosion,1,FALSE,FALSE,100,2,&v);CLOSE(v.k,1000.f/3600);}
 else CASE("blast-crouch"){real_point3d feet={0},explosion={.5f,0,0};source_movement_blast_impulse(&feet,&explosion,0,TRUE,FALSE,80,2,&v);real stand=v.k;source_movement_blast_impulse(&feet,&explosion,1,TRUE,FALSE,80,2,&v);CHECK(v.k>stand,"crouch failed to boost blast");CHECK(v.i<0,"wrong push direction");}
 else CASE("blast-rocket"){real_point3d feet={0},explosion={0};source_movement_blast_impulse(&feet,&explosion,0,FALSE,TRUE,90,2,&v);CLOSE(v.k,450.f/3600);source_movement_blast_impulse(&feet,&explosion,0,TRUE,TRUE,90,2,&v);CLOSE(v.k,900.f/3600);}
 else CASE("blast-falloff"){real_point3d feet={0},explosion={0};source_movement_blast_impulse(&feet,&explosion,0,TRUE,TRUE,50,2,&v);real full=v.k;explosion.z=-2;source_movement_blast_impulse(&feet,&explosion,0,TRUE,TRUE,50,2,&v);CLOSE(v.k,full*.5f);explosion.z=-3;source_movement_blast_impulse(&feet,&explosion,0,TRUE,TRUE,50,2,&v);CLOSE(v.k,0);}
 else CASE("blast-admission"){struct damage_data d={0,handles[0],1,1,{0}};CHECK(source_movement_blast(&d,10,&v),"solo self blast missing");d.owner_player_index=handles[1];CHECK(!source_movement_blast(&d,10,&v),"other player blast changed");d.owner_player_index=handles[0];d.flags=0;CHECK(!source_movement_blast(&d,10,&v),"direct hit changed");d.flags=1;d.definition_index=3;CHECK(!source_movement_blast(&d,10,&v),"bullet changed");}
 else CASE("blast-legacy"){role=2;support[2]=TRUE;body.unit.player_index=handles[1];struct damage_data d={0,handles[1],1,1,{0}};blast_support=0;CHECK(!source_movement_blast(&d,10,&v),"old fork got new blast");blast_support=1;CHECK(source_movement_blast(&d,10,&v),"new fork missing blast");}
 else CASE("fall-distance"){real stock=.003f;real height=12.f;real source_speed=sqrtf(2*SOURCE_GRAVITY*height);real stock_speed=sqrtf(2*stock*height);CHECK(source_speed>stock_speed,"no gravity difference");CLOSE(source_movement_fall_velocity(10,-source_speed,stock),-stock_speed);CLOSE(source_movement_fall_velocity(10,source_speed,stock),stock_speed);CLOSE(source_movement_fall_velocity(10,-source_speed,0),-source_speed);}
 else CASE("stock-fall"){setting=0;CLOSE(source_movement_fall_velocity(10,-.5f,.003f),-.5f);setting=1;body.unit.player_index=NONE;CLOSE(source_movement_fall_velocity(10,-.5f,.003f),-.5f);}
 else CASE("gravity"){source_movement_step(&v,&wish,&normal,TRUE,FALSE);CLOSE(v.k,-SOURCE_GRAVITY*.5);CLOSE(source_movement_player_gravity(handles[0],.001),SOURCE_GRAVITY);}
 else CASE("jump"){body.object.translational_velocity.i=.2;CHECK(biped_jump(10),"no jump");CLOSE(body.object.translational_velocity.k,SOURCE_JUMP_SPEED);CLOSE(body.object.translational_velocity.i,.2);CHECK(!biped_jump(10),"air jump");}
 else CASE("stock-jump"){setting=0;CHECK(biped_jump(10),"no stock jump");CLOSE(body.object.translational_velocity.k,.12);}
 else CASE("solo"){CHECK(source_movement_biped(10),"solo off");setting=0;CHECK(!source_movement_biped(10),"disabled on");}
 else CASE("mixed-host"){role=2;support[2]=TRUE;CHECK(source_movement_player(handles[0])&&source_movement_player(handles[1]),"fork off");CHECK(!source_movement_player(handles[2]),"stock peer changed");}
 else CASE("upstream-host"){role=1;CHECK(!source_movement_player(handles[0]),"upstream host enabled source");}
 else CASE("policy"){role=1;support[127]=TRUE;struct source_movement_policy p=policy();p.machines[0]=1;source_movement_handle(NONE,(word*)&p,sizeof(p),TRUE);CHECK(source_movement_player(handles[0]),"policy ignored");CHECK(!source_movement_player(handles[1]),"stock roster changed");}
 else CASE("bad-policy"){role=1;support[127]=TRUE;struct source_movement_policy p=policy();p.machines[0]=1;source_movement_handle(2,(word*)&p,sizeof(p),TRUE);source_movement_handle(NONE,(word*)&p,sizeof(p)-1,TRUE);source_movement_handle(NONE,(word*)&p,sizeof(p),FALSE);p.machines[3]=128;source_movement_handle(NONE,(word*)&p,sizeof(p),TRUE);CHECK(!source_movement_player(handles[0]),"bad policy accepted");}
 else CASE("slot-reuse"){role=1;support[127]=TRUE;struct source_movement_policy p=policy();p.machines[0]=1;source_movement_handle(NONE,(word*)&p,sizeof(p),TRUE);handles[0]=0x50000;CHECK(!source_movement_player(handles[0]),"reused slot inherited movement");}
 else CASE("reset"){role=1;support[127]=TRUE;struct source_movement_policy p=policy();p.machines[0]=1;source_movement_handle(NONE,(word*)&p,sizeof(p),TRUE);source_movement_reset();CHECK(!source_movement_player(handles[0]),"reset retained policy");}
 else CASE("send"){role=2;support[2]=TRUE;source_movement_update();CHECK(sends==1,"no policy");struct source_movement_policy *p=(void*)wire;CHECK(p->machines[0]==1&&p->machines[1]==2&&p->machines[2]==NONE,"bad roster");source_movement_update();CHECK(sends==1,"policy spam");}
 else CASE("excluded"){cheat.jetpack=1;CHECK(!source_movement_biped(10),"jetpack changed");cheat.jetpack=0;cinematic=1;CHECK(!source_movement_biped(10),"cinematic changed");cinematic=0;body.object.parent_object_index=20;CHECK(!source_movement_biped(10),"vehicle changed");body.object.parent_object_index=NONE;body.object.damage_flags=FLAG(_object_dead_bit);CHECK(!source_movement_biped(10),"corpse changed");body.object.damage_flags=0;body.unit.player_index=NONE;CHECK(!source_movement_biped(10),"AI changed");}
 else CASE("self-damage"){struct damage_data d={0,handles[0]};for(int i=0;i<3;i++){d.definition_index=i;CLOSE(source_movement_self_damage(&d,10),.6);}}
 else CASE("other-damage"){struct damage_data d={0,handles[1]};CLOSE(source_movement_self_damage(&d,10),1);d.owner_player_index=handles[0];d.definition_index=3;CLOSE(source_movement_self_damage(&d,10),1);d.definition_index=4;CLOSE(source_movement_self_damage(&d,10),1);role=2;body.unit.player_index=handles[2];d.definition_index=0;d.owner_player_index=handles[2];CLOSE(source_movement_self_damage(&d,10),1);}
 else CASE("replay"){role=1;support[127]=TRUE;struct source_movement_policy p=policy();p.machines[0]=1;source_movement_handle(NONE,(word*)&p,sizeof(p),TRUE);struct damage_data d={0,handles[0]};CLOSE(source_movement_self_damage(&d,10),1);}
 else return 2;return 0;
}
