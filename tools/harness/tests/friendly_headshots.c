#include "harness.h"
#include <math.h>
#define SET_FLAG(v,b,on) ((v)=((v)&~FLAG(b))|((on)?FLAG(b):0))
enum {_damage_material_head_bit,_damage_can_cause_headshots_bit,_damage_can_cause_multiplayer_headshots_bit,_object_being_damaged_killed_instantly_bit,_object_being_damaged_force_hard_ping_bit};
enum {_object_type_biped};
static boolean engine, last_lethal;
static struct {boolean deathless_player;} cheat;
static boolean game_engine_running(void){return engine;}
static struct {struct {long player_index;}unit;}unit;
#define unit_get(index) (&unit)
static void hit(real instant_kill_scale,boolean force_instant_damage,boolean player,boolean multiplayer,
 real scale,boolean instakill,boolean double_headshot,real health,real *body,real *dealt,unsigned long *being_damaged_flags)
{
 struct {struct {int type;real body_vitality;}object;}obj,*object=&obj;
 struct {unsigned long flags;}mat,*damage_material=&mat,def,*damage_definition=&def;
 long object_index=1;
 real maximum_body_vitality=100,damage_amount=10*scale,actual_damage=.1f*scale;
 real partial_instant_damage=0;
 engine=multiplayer;unit.unit.player_index=player?1:NONE;
 object->object.type=0;object->object.body_vitality=health;*being_damaged_flags=0;
 damage_material->flags=force_instant_damage?0:FLAG(_damage_material_head_bit);
 damage_definition->flags=(instakill?FLAG(_damage_can_cause_headshots_bit):0)|(double_headshot?FLAG(_damage_can_cause_multiplayer_headshots_bit):0);
 #include "under_test.inc"
 *body=object->object.body_vitality-actual_damage;*dealt=actual_damage;
 real body_vitality=*body;
 #include "lethal.inc"
}
int main(void)
{
 real body,dealt;unsigned long flags;
 for(int scale=1;scale<=4;scale*=2){
  hit(1.f/scale,FALSE,TRUE,TRUE,1.f/scale,TRUE,FALSE,1,&body,&dealt,&flags);
  CHECK(fabsf(body-(scale==1?-.1f:1.f-1.f/scale))<.000001f,"friendly headshot fraction %d",scale);
  CHECK(TEST_FLAG(flags,_object_being_damaged_killed_instantly_bit)==(scale==1),"surviving headshot flagged killed");
 }
 hit(1,FALSE,FALSE,FALSE,1,TRUE,FALSE,1,&body,&dealt,&flags);CHECK(fabsf(body+.1f)<.000001f,"AI headshot changed");
 hit(.5f,FALSE,TRUE,FALSE,.5f,TRUE,FALSE,1,&body,&dealt,&flags);CHECK(fabsf(body-.95f)<.000001f,"co-op player gained instant headshots");
 hit(0,FALSE,TRUE,TRUE,.5f,FALSE,TRUE,1,&body,&dealt,&flags);CHECK(fabsf(body-.9f)<.000001f&&fabsf(dealt-.1f)<.000001f,"numerical headshot bonus lost");
 for(int divisor=2;divisor<=4;divisor*=2){
  real health=1,step=1.f/divisor;
  for(int i=0;i<divisor;i++){
   hit(step,TRUE,TRUE,FALSE,.01f,FALSE,FALSE,health,&body,&dealt,&flags);
   CHECK(body==health-step&&dealt==step,"force kill added weapon damage");
   CHECK(last_lethal==(i==divisor-1),"exact zero did not kill at hit %d",i+1);
   health=body;
  }
 }
 hit(.5f,TRUE,TRUE,FALSE,.01f,FALSE,FALSE,.25f,&body,&dealt,&flags);CHECK(last_lethal&&body==-.25f,"low-health teammate survived");
 cheat.deathless_player=TRUE;
 hit(.5f,TRUE,TRUE,FALSE,.01f,FALSE,FALSE,.5f,&body,&dealt,&flags);CHECK(!last_lethal,"deathless cheat lost at zero");
 return 0;
}
