#include "harness.h"
#include <stdint.h>
#include <math.h>
#include "api.inc"
typedef unsigned short word;typedef word message_header;
#define real_point3d spray_test_point
#define real_vector3d spray_test_vector
typedef union {float n[3];struct {float x,y,z;};} real_point3d;
typedef union {float n[3];struct {float i,j,k;};} real_vector3d;
struct collision_result {short type;};
struct player_datum {int quit_out_of_game;struct {long machine_index;} network_player_data;long unit_index;unsigned short name[12];};
struct data_iterator {int next;};
struct network_game_client {int unused;};
struct distributed_message_header {word header;byte type,count;int32_t game_time;};
static struct player_datum player={0,{1},7,{'a','l','i','c','e',0}};
static void *player_data=&player;
static boolean active=TRUE,menu,blocked,alive=TRUE;static long tick=120;
static struct network_game_client client;static int sends;static int last_reliable;
#define TICKS_PER_SECOND 30
#define _collision_result_structure 2
#define _collision_test_front_facing_surfaces_bit 1
#define _collision_test_environment_flags 16
#define _collision_test_objects_all_types_flags 32
#define csmemcpy memcpy
#define _game_connection_local 0
static short game_connection(void){return 2;}
static boolean game_in_progress(void){return active;}
static boolean main_menu_is_active(void){return menu;}
static long game_time_get(void){return tick;}
static short global_structure_bsp_index_get(void){return 4;}
static void data_iterator_new(struct data_iterator *i,void *data){(void)data;i->next=0;}
static void *data_iterator_next(struct data_iterator *i){return i->next++?NULL:&player;}
static long distributed_living_unit(struct player_datum *p){return alive?p->unit_index:NONE;}
static void object_get_origin(long unit,real_point3d *point){CHECK(unit==7,"wrong player");memset(point,0,sizeof(*point));}
static boolean collision_test_vector(unsigned long flags,const real_point3d *origin,const real_vector3d *direction,long unit,struct collision_result *out){
 CHECK(flags&32,"objects do not block spray");CHECK(unit==7,"wrong ray owner");(void)origin;(void)direction;out->type=2;return !blocked;
}
static struct network_game_client *global_network_game_client_get(void){return &client;}
static void *network_game_client_get_game(struct network_game_client *c){return c;}
static void *global_network_game_server_get(void){return &client;}
static short network_game_client_get_local_machine_index(void){return 0;}
static void build_message_header(word *header,word size,byte kind,byte flags){CHECK(kind==2&&!flags,"game header changed");*header=(word)(size<<4|kind<<2);}
static int record_send(void *data,word size,int reliable){unsigned char *b=data;CHECK(size==24&&b[2]==SPRAY_SHARE_TYPE&&b[13]==1,"extension payload corrupted");CHECK((*(word *)data>>4)==size,"header length wrong");sends++;last_reliable=reliable;return 1;}
boolean network_distributed_client_send(void *data,word size){return record_send(data,size,0);}
boolean network_distributed_client_send_reliably(void *data,word size){return record_send(data,size,1);}
boolean network_distributed_server_send_to_machine(long machine,void *data,word size){CHECK(machine==1,"incorrect peer");return record_send(data,size,0);}
boolean network_distributed_server_send_to_machine_reliably(long machine,void *data,word size){CHECK(machine==1,"incorrect peer");return record_send(data,size,1);}
short network_distributed_server_machines(long *indices,short capacity){CHECK(capacity>=1,"capacity");indices[0]=1;return 1;}
void posix_random_bytes(void *data,unsigned int size){memset(data,42,size);}
unsigned long system_milliseconds(void){return 1000;}
int network_spray_ready(int slot,int owner,const void *data,size_t size,const struct spray_pose *pose,const char *name,int local){(void)slot;(void)owner;(void)data;(void)size;(void)pose;(void)name;(void)local;return 1;}
int halo_spray_file_read(void **data,size_t *size){*data=NULL;*size=0;return 0;}
#include "under_test.inc"
int main(int argc,char **argv){const char *case_name=argc>1?argv[1]:"";struct spray_pose pose={{0,0,.5f},{1.5f,0,0},4};char name[32]="spoofed";
 CASE("transport"){unsigned char message[24]={0};message[2]=SPRAY_SHARE_TYPE;message[13]=1;host_role=1;CHECK(spray_send(NULL,1,message,24,1)&&last_reliable,"host stream");CHECK(spray_send(NULL,1,message,24,0)&&!last_reliable,"host datagram");host_role=0;CHECK(spray_send(NULL,0,message,24,1)&&last_reliable,"client stream");CHECK(sends==3,"wrong route");return 0;}
 CASE("owner") {CHECK(!spray_accept(NULL,2,&pose,name),"other machine's unit accepted");return 0;}
 CASE("dead")alive=FALSE;
 CASE("blocked")blocked=TRUE;
 CASE("distance")pose.origin[0]=2;
 CASE("reach")pose.direction[0]=2;
 CASE("nan")pose.direction[0]=NAN;
 CASE("bsp")pose.bsp=5;
 CASE("cooldown"){has_sprayed[1]=1;last_spray[1]=1;CHECK(!spray_accept(NULL,1,&pose,name),"cooldown short");tick=121;CHECK(spray_accept(NULL,1,&pose,name),"four second boundary rejected");return 0;}
 CASE("menu")menu=TRUE;
 int accepted=spray_accept(NULL,1,&pose,name);
 if(!strcmp(case_name,"valid")){CHECK(accepted&&!strcmp(name,"alice"),"valid request/profile failed");}else CHECK(!accepted,"bad request accepted: %s",case_name);
 return 0;
}
