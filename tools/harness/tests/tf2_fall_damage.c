#include "harness.h"
#include <math.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
enum {_damage_category_falling};
enum {_damage_bypasses_shields_bit};
static boolean enabled=TRUE;
static boolean source_movement_biped(long index){return enabled;}
static struct {boolean deathless_player;}cheat;
int main(void){
 long object_index=1;real maximum_body_vitality=100,actual_damage=.99f,damage_amount=99,partial_instant_damage=0;
 struct {int category;}def={_damage_category_falling},*damage_definition=&def;
 struct {long owner_player_index;unsigned long flags;real scale;}event={NONE,1,.15f},*damage=&event;
 #include "under_test.inc"
 CHECK(fabsf(actual_damage-.15f)<.00001f,"material damage not replaced");
 CHECK(fabsf(damage_amount-15)<.00001f,"wrong max health scale");
 real body_vitality=.15f-actual_damage;boolean lethal;
 #include "lethal.inc"
 CHECK(lethal,"exact zero fall health survived");
 maximum_body_vitality=200;actual_damage=.99;partial_instant_damage=0;
 #include "under_test.inc"
 CHECK(fabsf(actual_damage-.15f)<.00001f && fabsf(damage_amount-30)<.00001f,"health scaling differs");
 damage->owner_player_index=1;actual_damage=.77f;
 #include "under_test.inc"
 CHECK(fabsf(actual_damage-.77f)<.00001f,"player damage reclassified");
 damage->owner_player_index=NONE;enabled=FALSE;actual_damage=.66f;
 #include "under_test.inc"
 CHECK(fabsf(actual_damage-.66f)<.00001f,"stock damage changed");
 return 0;
}
