#include "harness.h"
typedef struct {float i,j;} real_vector2d;
#define CONTROL_HELD_TICKS 60
#define _object_mask_unit 3
#define _object_type_vehicle 1
#define _actor_action_melee 1
#define _actor_action_leap 2
struct control {int fire;};
struct client_actor {long unit_index,received_time;boolean driven;struct control control;int impulse,impulse_number,played_impulse_number;boolean impulse_aligned;real_vector2d impulse_alignment;long speech_sound;int speech_number,played_speech_number;long user_animation_graph;int user_animation,user_animation_interpolate,user_animation_number,played_user_animation_number;};
static struct client_actor client_actors[2];static int client_actor_count=2,calls[2];
static struct obj {struct {int type;} object;} units[2];
static boolean follows=TRUE;
static boolean network_objects_vehicle_follows_host(long index){return follows;}
static long game_time_get(void){return 100;}
static int actor_unit_valid(long i){return 1;}
static void *object_try_and_get_and_verify_type(long i,long m){return &units[i];}
static struct obj *object_get(long i){return &units[i];}
static void unit_set_actively_controlled(long i,boolean active){}
static void unit_control(long i,struct control *c){if(c->fire)calls[i]++;}
static void unit_melee_attack_begin(long i,boolean x,void *v){}
static void unit_leap_begin(long i,void *v){}
static void unit_start_animation_impulse(long i,int impulse,void *v){}
static void speech_play(long i,long s){}
static void unit_port_play_user_animation(long i,long g,int a,int interp,int z){}
#include "under_test.inc"
int main(int argc,char**argv){
 for(int i=0;i<2;i++){client_actors[i].unit_index=i;client_actors[i].received_time=99;client_actors[i].control.fire=1;client_actors[i].impulse=NONE;client_actors[i].speech_sound=NONE;client_actors[i].user_animation_graph=NONE;}
 units[0].object.type=0;units[1].object.type=1;
 if(argc>1&&!strcmp(argv[1],"player-driven"))follows=FALSE;
 network_actors_drive();
 CHECK(calls[0]==1 && calls[1]==(follows?1:0),"biped %d, AI vehicle %d",calls[0],calls[1]);return 0;
}
