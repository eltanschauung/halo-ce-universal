#include "harness.h"
#include <math.h>
typedef unsigned short word;
#define TICKS_PER_SECOND 30
#define MAXIMUM_TRACKED_OBJECTS 64
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 4
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define SET_FLAG(a,b,c) ((a)=(c)?((a)|FLAG(b)):((a)&~FLAG(b)))
#define UNIT_WORLD_BOUND 32768.f
#define PROJECTILE_DEFINITION_TAG 1
#define EFFECT_DEFINITION_TAG 2
#define _object_mask_projectile 1
#define _object_mask_all 255
#define _object_deleted_when_deactivated_bit 2
#define _new_object_never_automatically_delete_bit 0
#define _distributed_message_projectiles 255
#define _distributed_to_clients_reliably 1
#define _game_connection_network_server 2
#define _game_connection_network_client 1
#define GET_MESSAGE_TYPE(h) (((h)>>2)&3)
struct distributed_message_header {word header;byte type,count;long game_time;};
struct object_placement_data {long definition_index,owner_object_index,owner_player_index;unsigned long flags;real_point3d position;real_vector3d forward,up;};
struct projectile_definition {struct {float initial_velocity;}projectile;} definitions[20];
struct location {long cluster;};
struct fixture_object {long owner_object_index,owner_player_index;unsigned long flags;real_point3d position;real_vector3d forward,up,translational_velocity;struct location location;};
struct object_datum {long definition_index;struct fixture_object object;};
struct projectile_datum {long definition_index;struct fixture_object object;struct {unsigned long flags;long ignore_object_index;float detonation_timer,arming_time,odometer;}projectile;};
struct object_iterator {long index;int cursor;};
static struct projectile_datum world[2][64];
static long ids[2][64];
static int role=2,active=1,coop=1,menu,now=100,support[4]={0,1,1,0},full;
static int creates,deletes,starts,stops,effects,exports,moves;
static long last_effect;
static real_point3d effect_point;
static int game_connection(void){return role;}
static int game_in_progress(void){return active;}
static int main_menu_is_active(void){return menu;}
static int network_coop_active(void){return coop;}
static long game_time_get(void){return now;}
static boolean network_extensions_allows(long machine,byte type){return machine==NONE?support[1]:machine>=0&&machine<4&&support[machine];}
static boolean distributed_tag_of_group(long tag,unsigned long group){return group==PROJECTILE_DEFINITION_TAG?tag>=10&&tag<15:tag>=100&&tag<110;}
static struct projectile_definition *projectile_definition_get(long index){return &definitions[index];}
static void *object_try_and_get_and_verify_type(long index,long mask){int slot=index&65535;return index!=NONE&&slot<64&&ids[role-1][slot]==index?&world[role-1][slot]:NULL;}
static void object_get_origin(long i,real_point3d*p){*p=((struct projectile_datum*)object_try_and_get_and_verify_type(i,1))->object.position;}
static void object_get_orientation(long i,real_vector3d*f,real_vector3d*u){struct projectile_datum*p=object_try_and_get_and_verify_type(i,1);*f=p->object.forward;*u=p->object.up;}
static void object_iterator_new(struct object_iterator *it,long mask,long flags){it->cursor=0;}
static void *object_iterator_next(struct object_iterator *it){while(it->cursor<64){int i=it->cursor++;if(ids[role-1][i]!=NONE){it->index=ids[role-1][i];return &world[role-1][i];}}return NULL;}
short network_distributed_server_machines(long*out,short max){out[0]=1;out[1]=2;return 2;}
static int distributed_real_valid(real f){return isfinite(f);}
static int distributed_point_valid(real_point3d const*p,real bound){return isfinite(p->x)&&isfinite(p->y)&&isfinite(p->z)&&fabsf(p->x)<=bound&&fabsf(p->y)<=bound&&fabsf(p->z)<=bound;}
static int distributed_object_index_valid(long i){return i!=NONE&&(i&65535)<64&&((unsigned long)i>>16)!=0;}
static int distributed_axes_make_valid(real_vector3d *f,real_vector3d*u){return distributed_point_valid((real_point3d*)f,1.1f)&&distributed_point_valid((real_point3d*)u,1.1f)&&fabsf(f->i*f->i+f->j*f->j+f->k*f->k-1.f)<.1f&&fabsf(u->i*u->i+u->j*u->j+u->k*u->k-1.f)<.1f&&fabsf(f->i*u->i+f->j*u->j+f->k*u->k)<.1f;}
static void object_placement_data_new(struct object_placement_data*p,long tag,long owner){memset(p,0,sizeof(*p));p->definition_index=tag;p->owner_object_index=owner;p->owner_player_index=NONE;}
static long object_new(struct object_placement_data*p){if(full)return NONE;for(int i=32;i<64;i++)if(ids[role-1][i]==NONE){long id=0x45670000L|i;ids[role-1][i]=id;struct projectile_datum*q=&world[role-1][i];memset(q,0,sizeof(*q));q->definition_index=p->definition_index;q->object.owner_player_index=p->owner_player_index;q->object.owner_object_index=p->owner_object_index;q->object.position=p->position;q->object.forward=p->forward;q->object.up=p->up;creates++;if(p->definition_index==10)starts++;return id;}return NONE;}
static void object_delete(long i){struct projectile_datum*p=object_try_and_get_and_verify_type(i,1);if(p){deletes++;if(p->definition_index==10)stops++;ids[role-1][i&65535]=NONE;}}
static void object_set_position(long i,real_point3d const*p,real_vector3d const*f,real_vector3d const*u){struct projectile_datum*q=object_try_and_get_and_verify_type(i,1);q->object.position=*p;q->object.forward=*f;q->object.up=*u;moves++;}
void render_interpolation_correct_object(long i,real_vector3d const*offset){}
void projectile_export_function_values(long i){exports++;}
static boolean projectile_play_flyby_sound(long i,real_vector3d const*m,struct location const*l,long ignore){return FALSE;}
static void effect_new_unattached_from_markers(long tag,long owner,void*x,short count,char const**names,real_point3d const*p,real_vector3d const*f,real scale,real material,void*y,void*z,boolean visible){effects++;last_effect=tag;effect_point=p[0];}
static boolean object_has_node(long index,short node){return node>=0&&node<4;}
static int attached;
static void effect_new_attached_from_markers(long tag,long owner,long target,short node,short count,char const**names,real_point3d const*p,real_vector3d const*f,real scale,real material,void*y,void*z){attached++;effect_new_unattached_from_markers(tag,owner,NULL,count,names,p,f,scale,material,y,z,TRUE);}

static struct {byte bytes[512];word size;long machine;boolean reliable;} sent[1024];
static int sends;
static void record(long peer,void *data,byte type,short count,word size,boolean reliable){CHECK(sends<1024&&size<=512,"wire overflow");struct distributed_message_header *h=data;h->header=8;h->type=type;h->count=count;h->game_time=now;memcpy(sent[sends].bytes,data,size);sent[sends].size=size;sent[sends].machine=peer;sent[sends++].reliable=reliable;}
static void distributed_send_to_machine_reliably(long peer,void *data,byte type,short count,word size){if(support[peer])record(peer,data,type,count,size,TRUE);}
static void distributed_send_to_machine(long peer,void *data,byte type,short count,word size){if(support[peer])record(peer,data,type,count,size,FALSE);}
static void distributed_send(void*data,byte type,short count,word size,short target){for(int i=1;i<=2;i++)if(support[i])record(i,data,type,count,size,TRUE);}

#define _unit_grenade_throw_in_hand 1
#define _unit_grenade_throw_ending 2
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&information)
struct unit_datum {long definition_index;struct {long grenade_throw_state,grenade_object_index,actor_index,player_index,grenade_throw_ticks,grenade_throw_full_power_ticks;real_vector3d aiming_vector;}unit;} throwing_unit;
struct unit_definition {struct {real grenade_velocity;}unit;} unit_definition={{12}};
struct game_globals_player_information {real_vector3d grenade_origin;} information;
static real_vector3d solver={.13f,.07f,.22f},up={0,0,1};
#define global_up3d (&up)
static struct unit_datum *unit_get(long i){return &throwing_unit;}
static struct unit_definition *unit_definition_get(long i){return &unit_definition;}
static struct object_datum *object_get(long i){return object_try_and_get_and_verify_type(i,1);}
static void object_detach(long i){}
static void actor_aim_grenade(long i,real_point3d*p,real_vector3d*v){*v=solver;}
static float normalize3d(real_vector3d*v){return 1;}
static real_vector3d *cross_product3d(real_vector3d*a,real_vector3d*b,real_vector3d*c){*c=up;return c;}
static void unit_get_camera_position(long i,real_point3d*p){*p=(real_point3d){0,0,0};}
static void *point_from_line3d(real_point3d*p,real_vector3d*v,real t,real_point3d*q){q->x=p->x+v->i*t;q->y=p->y+v->j*t;q->z=p->z+v->k*t;return q;}
static void object_translate(long i,real_point3d*p,void*x){}
static void scale_vector3d(real_vector3d*a,real t,real_vector3d*b){*b=(real_vector3d){a->i*t,a->j*t,a->k*t};}
static real real_random_range(real a,real b){return a;}
static void subtract_vectors3d(real_vector3d*a,real_vector3d*b,real_vector3d*c){*c=(real_vector3d){a->i-b->i,a->j-b->j,a->k-b->k};}
static void projectile_accelerate(long i,real_vector3d*v){struct object_datum*g=object_get(i);g->object.translational_velocity.i+=v->i;g->object.translational_velocity.j+=v->j;g->object.translational_velocity.k+=v->k;}
static int object_force_inside_bsp(long i,real_point3d*p){return 1;}
#include "release.inc"

#include "under_test.inc"
static long host_id=0x92340003L;
static struct projectile_datum *host_projectile(void){role=2;ids[1][3]=host_id;struct projectile_datum*p=&world[1][3];memset(p,0,sizeof(*p));p->definition_index=10;p->object.owner_object_index=NONE;p->object.owner_player_index=NONE;p->object.forward=(real_vector3d){1,0,0};p->object.up=(real_vector3d){0,0,1};p->object.position=(real_point3d){10,20,30};p->object.translational_velocity=(real_vector3d){.13f,.07f,.22f};p->projectile.detonation_timer=.7f;p->projectile.arming_time=.5f;p->projectile.odometer=3;return p;}
static void deliver(int n,boolean reliable){role=1;network_projectiles_handle(NONE,(word*)sent[n].bytes,sent[n].size,reliable);}
static struct projectile_datum *replica(void){return projectile_try(projectile_clients[3].object_index);}
static void advance_state(struct projectile_datum*p){role=2;now++;p->object.position.x+=p->object.translational_velocity.i;p->object.position.z+=p->object.translational_velocity.k;p->projectile.detonation_timer+=.01f;network_projectiles_host_tick();}
static void equal(struct projectile_datum*p){struct projectile_datum*q=replica();CHECK(q,"missing replica");CHECK(memcmp(&q->object.position,&p->object.position,12)==0,"position differs");CHECK(memcmp(&q->object.translational_velocity,&p->object.translational_velocity,12)==0,"launch velocity differs");CHECK(q->projectile.detonation_timer==p->projectile.detonation_timer,"fuse differs");}
static void impact(void){role=2;real_point3d p[2]={{10,20,30},{10,20,30}};real_vector3d f[2]={{1,0,0},{0,0,-1}};network_projectiles_effect(host_id,100,NONE,NONE,2,p,f,0,0);}
int main(int argc,char**argv){CHECK(argc==2,"case");char const *case_name=argv[1];memset(ids,255,sizeof(ids));network_projectiles_reset();definitions[10].projectile.initial_velocity=.2f;definitions[11].projectile.initial_velocity=.3f;definitions[12].projectile.initial_velocity=0;definitions[13].projectile.initial_velocity=10;definitions[14].projectile.initial_velocity=.133f;struct projectile_datum*p=host_projectile();
 CASE("grenade-launch") {throwing_unit.unit.grenade_throw_state=1;throwing_unit.unit.grenade_object_index=host_id;throwing_unit.unit.actor_index=9;throwing_unit.unit.player_index=NONE;throwing_unit.unit.aiming_vector=(real_vector3d){1,0,0};p->object.translational_velocity=(real_vector3d){0,0,0};unit_throw_grenade_release(1,FALSE);CHECK(memcmp(&p->object.translational_velocity,&solver,12)==0,"host AI launch not used");network_projectiles_host_tick();deliver(0,TRUE);equal(p);struct object_placement_data d={0};d.definition_index=10;d.owner_player_index=NONE;CHECK(network_projectiles_suppress(&d),"client made its own inaccurate AI grenade");CHECK(network_projectiles_update(projectile_clients[3].object_index),"replica ran local collision/fuse");}
 else CASE("wraith-state") {p->definition_index=11;network_projectiles_host_tick();deliver(0,TRUE);equal(p);advance_state(p);deliver(2,FALSE);equal(p);}
 else CASE("in-hand") {p->object.position=(real_point3d){4,5,6};network_projectiles_host_tick();deliver(0,TRUE);equal(p);advance_state(p);deliver(2,FALSE);equal(p);CHECK(creates==1,"hand release duplicated grenade");}
 else CASE("warning-sound") {network_projectiles_host_tick();deliver(0,TRUE);CHECK(starts==1,"normal warning attachment absent");CHECK(network_projectiles_update(projectile_clients[3].object_index),"client fuse ran");for(int i=0;i<40;i++){advance_state(p);deliver(2+2*i,FALSE);}CHECK(starts==1&&!stops,"sound restarted/stopped during fuse");role=2;network_projectiles_deleted(host_id);deliver(sends-2,TRUE);CHECK(stops==1,"warning survived deletion");}
 else CASE("fuse") {network_projectiles_host_tick();deliver(0,TRUE);equal(p);p->projectile.detonation_timer=.99f;advance_state(p);deliver(2,FALSE);equal(p);}
 else CASE("bounce") {network_projectiles_host_tick();deliver(0,TRUE);p->object.translational_velocity=(real_vector3d){-.1f,.02f,.3f};p->projectile.flags=4;advance_state(p);deliver(2,FALSE);equal(p);CHECK(replica()->projectile.flags==4,"bounce flags lost");}
 else CASE("sticky") {network_projectiles_host_tick();deliver(0,TRUE);p->object.translational_velocity=(real_vector3d){0,0,0};p->projectile.flags=8;advance_state(p);deliver(2,FALSE);equal(p);CHECK(replica()->projectile.flags==8,"sticking flags lost");}
 else CASE("delete") {network_projectiles_host_tick();deliver(0,TRUE);role=2;network_projectiles_deleted(host_id);deliver(2,TRUE);CHECK(!replica()&&deletes==1&&stops==1,"client grenade/audio survived host deletion");}
 else CASE("same-tick-hit") {impact();network_projectiles_deleted(host_id);deliver(0,TRUE);CHECK(effects==1&&last_effect==100&&effect_point.x==10,"same-tick explosion vanished");deliver(2,TRUE);CHECK(!replica(),"terminal event left projectile");}
 else CASE("attached-effect") {impact();struct projectile_effect_message *m=(void*)sent[0].bytes;role=1;ids[0][5]=0x12340005L;m->count=5;m->target_index=ids[0][5];m->target_node=2;deliver(0,TRUE);CHECK(attached==1&&effects==1,"hit effect did not follow struck object");}
 else CASE("missing-effect-target") {impact();struct projectile_effect_message *m=(void*)sent[0].bytes;m->target_index=0x12340005L;m->target_node=2;deliver(0,TRUE);CHECK(!attached&&effects==1,"missing target dropped hit effect");}
 else CASE("duplicate") {impact();deliver(0,TRUE);deliver(0,TRUE);CHECK(effects==1&&creates==1&&starts==1,"duplicate effect/projectile/sound");}
 else CASE("effect-overtaken") {impact();advance_state(p);deliver(2,TRUE);real x=replica()->object.position.x;deliver(0,TRUE);CHECK(effects==1&&replica()->object.position.x==x,"late reliable effect rewound state or vanished");}
 else CASE("tombstone") {network_projectiles_host_tick();role=2;network_projectiles_deleted(host_id);deliver(2,TRUE);deliver(0,TRUE);CHECK(!replica()&&!creates,"stale spawn resurrected dead grenade");}
 else CASE("slot-reuse") {network_projectiles_host_tick();deliver(0,TRUE);role=2;network_projectiles_deleted(host_id);host_id=0x92350003L;p=host_projectile();advance_state(p);deliver(4,TRUE);CHECK(stops==1&&creates==2,"host slot reuse leaked replica");deliver(2,TRUE);CHECK(replica(),"old delete removed new object");deliver(0,TRUE);equal(p);}
 else CASE("local-slot-reuse") {network_projectiles_host_tick();deliver(0,TRUE);long local=projectile_clients[3].object_index;object_delete(local);ids[0][local&65535]=0x66660000L|(local&65535);CHECK(!network_projectiles_update(ids[0][local&65535]),"local slot reuse became authoritative projectile");}
 else CASE("pool-full") {network_projectiles_host_tick();full=1;deliver(0,TRUE);CHECK(!replica(),"pool full ignored");full=0;advance_state(p);deliver(2,FALSE);CHECK(replica(),"creation not retried");equal(p);}
 else CASE("timeout") {network_projectiles_host_tick();deliver(0,TRUE);now+=PROJECTILE_TIMEOUT+1;network_projectiles_client_tick();CHECK(!replica()&&stops==1,"orphan warning loop");advance_state(p);deliver(2,FALSE);CHECK(replica(),"state did not repair timed out replica");}
 else CASE("late-negotiation") {role=1;struct object_placement_data d={0};d.definition_index=10;d.owner_player_index=NONE;long old=object_new(&d);network_projectiles_host_tick();network_projectiles_client_tick();CHECK(!projectile_try(old),"legacy AI grenade survived capability handoff");role=2;network_projectiles_host_tick();deliver(0,TRUE);network_projectiles_client_tick();CHECK(replica(),"handoff removed host replica");}
 else CASE("late-join") {support[2]=0;network_projectiles_host_tick();CHECK(sends==1,"legacy peer got private message");support[2]=1;network_projectiles_machine_joined(2);advance_state(p);CHECK(sends==3&&sent[2].machine==2&&sent[2].reliable,"late join did not get reliable creation");deliver(2,TRUE);equal(p);}
 else CASE("stock") {support[1]=support[2]=0;network_projectiles_host_tick();impact();network_projectiles_deleted(host_id);CHECK(!sends,"stock peer traffic changed");role=1;struct object_placement_data d={0};d.definition_index=10;d.owner_player_index=NONE;CHECK(!network_projectiles_suppress(&d),"upstream host lost legacy AI projectiles");}
 else CASE("player") {p->object.owner_player_index=0x12340000;network_projectiles_host_tick();CHECK(!sends,"player prediction replaced");role=1;struct object_placement_data d={0};d.definition_index=10;d.owner_player_index=0x12340000;CHECK(!network_projectiles_suppress(&d),"player grenade suppressed");}
 else CASE("speed-boundary") {definitions[13].projectile.initial_velocity=4.f;CHECK(projectile_tag_eligible(13),"fast visible vehicle shell excluded");definitions[13].projectile.initial_velocity=4.01f;CHECK(!projectile_tag_eligible(13),"instant bullet entered stream");}
 else CASE("hitscan") {p->definition_index=13;network_projectiles_host_tick();CHECK(!sends,"instant bullet entered reliable stream");role=1;struct object_placement_data d={0};d.definition_index=13;d.owner_player_index=NONE;CHECK(!network_projectiles_suppress(&d),"instant bullet suppressed");}
 else CASE("non-coop") {coop=0;network_projectiles_host_tick();CHECK(!sends,"competitive game changed");role=1;struct object_placement_data d={0};d.definition_index=10;d.owner_player_index=NONE;CHECK(!network_projectiles_suppress(&d),"competitive shots suppressed");}
 else CASE("admission") {network_projectiles_host_tick();role=1;network_projectiles_handle(2,(word*)sent[0].bytes,sent[0].size,TRUE);CHECK(!creates,"client-origin state accepted");role=2;network_projectiles_handle(NONE,(word*)sent[0].bytes,sent[0].size,TRUE);CHECK(!creates,"server accepted client state");role=1;support[1]=0;deliver(0,TRUE);CHECK(!creates,"unnegotiated state accepted");support[1]=1;role=2;network_projectiles_deleted(host_id);deliver(2,FALSE);CHECK(!projectile_clients[3].serial,"unreliable deletion accepted");}
 else CASE("corrupt") {network_projectiles_host_tick();role=1;struct projectile_message m;memcpy(&m,sent[0].bytes,sizeof(m));for(int n=0;n<(int)sizeof(m);n++)network_projectiles_handle(NONE,(word*)&m,n,TRUE);CHECK(!creates,"short packet accepted");for(int n=0;n<10;n++){struct projectile_message bad=m;switch(n){case 0:bad.state.position.x=NAN;break;case 1:bad.state.object_index=0x123400ff;break;case 2:bad.state.definition_index=13;break;case 3:bad.state.velocity.k=INFINITY;break;case 4:bad.state.timer=NAN;break;case 5:bad.state.forward.i=2;break;case 6:bad.state.serial=0;break;case 7:bad.reserved=1;break;case 8:bad.header.count=0;break;case 9:bad.operation=10;break;}network_projectiles_handle(NONE,(word*)&bad,sizeof(bad),TRUE);}CHECK(!creates,"malformed snapshot accepted");}
 else if(!strcmp(case_name,"latency-100")||!strcmp(case_name,"latency-200")||!strcmp(case_name,"loss")){int lag=!strcmp(case_name,"latency-100")?2:3;network_projectiles_host_tick();for(int tick=1;tick<=30;tick++){advance_state(p);if(tick>=lag){int packet=2*(tick-lag);if(strcmp(case_name,"loss")||tick%4)deliver(packet,sent[packet].reliable);}}deliver(sends-2,FALSE);equal(p);CHECK(creates==1&&starts==1,"latency/loss duplicated sound");role=2;impact();network_projectiles_deleted(host_id);deliver(sends-4,TRUE);deliver(sends-2,TRUE);CHECK(effects==1&&stops==1&&!replica(),"terminal state lost under latency");}
 else return 2;return 0;
}
