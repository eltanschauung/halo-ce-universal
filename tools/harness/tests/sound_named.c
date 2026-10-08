#include "harness.h"
#define csmemset memset
struct tag_block {long count;void *address;};
struct sound_permutation {char name[32];};
struct sound_pitch_range {short actual_permutation_count;short forced_permutation_index;struct tag_block permutations;};
struct sound_definition {struct tag_block pitch_ranges;};
struct sound_source {short spatialization_mode;float scale,gain;};
struct sound_datum {long definition_index;short pitch_range_index,permutation_index;};
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&((t *)(b)->address)[i])
#define _sound_spatialization_mode_none 0
static struct sound_permutation permutations[4]={ {"overheat1"},{"overheat2"},{"overheat3"},{"continuation"} };
static struct sound_pitch_range ranges[2];
static struct sound_definition definition;
static struct sound_datum voice={7,0,0}, existing={7,0,0};
static int starts,stops,requests,disabled,promoted;
static struct sound_permutation *cached;
static struct sound_definition *sound_definition_get(long index){CHECK(index==7,"wrong definition");return &definition;}
static long sound_new_impulse(long index,struct sound_source *source,long id,void *track,void *data,short size){
 CHECK(index==7&&source->spatialization_mode==0&&source->scale==1&&source->gain==1,"bad source");
 CHECK(id==NONE&&!track&&!data&&!size,"not independent impulse");starts++;
 if(disabled)return NONE;voice.definition_index=promoted?8:7;return 9;
}
static struct sound_datum *sound_get(long index){CHECK(index==9,"wrong voice");return &voice;}
static void sound_stop_impulse(long index){CHECK(index==9,"wrong stop");stops++;}
static void _sound_cache_sound_request(struct sound_permutation *p,int a,int b,int c){CHECK(!a&&b&&!c,"bad cache flags");requests++;cached=p;}
#include "under_test.inc"
int main(int argc,char **argv){
 const char *case_name=argc>1?argv[1]:"";
 ranges[0]=(struct sound_pitch_range){3,2,{4,permutations}};
 ranges[1]=(struct sound_pitch_range){0,NONE,{0,NULL}};
 definition.pitch_ranges=(struct tag_block){2,ranges};
 char const *name="overheat2";int selected=1;
 CASE("third"){name="overheat3";selected=2;}
 CASE("missing") name="absent";
 CASE("linked") name="continuation";
 CASE("disabled") disabled=1;
 CASE("promoted") promoted=1;
 long result=unspatialized_impulse_sound_new_named(7,name);
 CHECK(ranges[0].forced_permutation_index==2&&existing.permutation_index==0,"shared weapon state changed");
 if(!strcmp(case_name,"missing")||!strcmp(case_name,"linked"))CHECK(result==NONE&&starts==0&&!requests,"unknown/root continuation played");
 else if(disabled)CHECK(result==NONE&&!requests&&!stops,"disabled voice accessed");
 else if(promoted)CHECK(result==NONE&&stops==1&&!requests,"promoted voice overwritten");
 else CHECK(result==9&&voice.pitch_range_index==0&&voice.permutation_index==selected&&requests==1&&cached==&permutations[selected],"wrong selected clip/cache");
 return 0;
}
