#include "harness.h"
#include <math.h>
typedef unsigned short word;
#define REAL_MAX 3.402823466e38f
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define PIN(n,a,b) MIN(MAX(n,a),b)
#define csstrcmp strcmp
struct bitmap_data {short width,height;void *base_address;};
struct weapon_definition {int unused;};
struct hud_absolute_placement_definition {int corner;};
struct meter_hud_element_definition {
 struct {struct {short x,y;}offset;struct {real i,j;}scale;unsigned flags;}placement;
 struct {long index;}meter_bitmap;short sequence_index;byte alpha_multiplier,alpha_bias,minimum_value;
 unsigned min_color,max_color,flash_color,empty_color;real opacity,fade;
};
typedef struct {real x0,x1,y0,y1;} real_rectangle2d;
static int allocations,deletions,rebuilt,attempts,fail_at=-1;
static boolean active=TRUE,has_original=TRUE;
static char const *tag_name="ui\\hud\\bitmaps\\combined\\hud_ammo_meters";
static struct bitmap_data original={256,256,NULL};
static real_rectangle2d original_clip={108.f/256,219.f/256,45.f/256,62.f/256};
static boolean needler_balance_hud_active(struct weapon_definition const *w){return active;}
static char const *tag_get_name(long index){return tag_name;}
static struct bitmap_data *bitmap_2d_new(short w,short h,short mip,short format){
 if(attempts++==fail_at)return NULL;struct bitmap_data *b=malloc(sizeof(*b));CHECK(b,"test allocation");
 *b=(struct bitmap_data){w,h,calloc(w*h,2)};CHECK(b->base_address,"test pixels");allocations++;return b;
}
static void bitmap_delete(struct bitmap_data *b){CHECK(b,"never delete NULL");free(b->base_address);free(b);deletions++;}
static void *bitmap_2d_address(struct bitmap_data *b,short x,short y,short mip){return (word*)b->base_address+y*b->width+x;}
static void bitmap_rebuild(struct bitmap_data *b){rebuilt++;}
static void hud_retrieve_bitmap_and_bounding_rect(long tag,short sequence,short frame,
 struct bitmap_data const **b,real_rectangle2d const **c){*b=has_original?&original:NULL;*c=has_original?&original_clip:NULL;}
static int draws;static struct bitmap_data const *drawn;static struct meter_hud_element_definition chosen;
static real_rectangle2d chosen_clip;static short drawn_flags,drawn_player;static real drawn_time;
static void hud_draw_meter_bitmap(short player,struct hud_absolute_placement_definition const *a,
 struct meter_hud_element_definition const *m,byte lo,byte hi,short flags,real time,real ref,
 struct bitmap_data const *b,real_rectangle2d const *c,boolean interface_bitmap){
 CHECK(lo==128&&hi==128&&!ref&&!interface_bitmap,"stable binary fill threshold");
 draws++;drawn=b;chosen=*m;chosen_clip=*c;drawn_flags=flags;drawn_player=player;drawn_time=time;
}
#include "under_test.inc"
static int segments(struct bitmap_data const *b){
 static byte seen[NEEDLER_HUD_PIXELS];static long queue[NEEDLER_HUD_PIXELS];
 word const *p=b->base_address;memset(seen,0,sizeof(seen));int count=0;
 for(long i=0;i<NEEDLER_HUD_PIXELS;i++)if(!seen[i]&&(p[i]>>8)>127&&(p[i]&255)<128){
  count++;long first=0,last=0;queue[last++]=i;seen[i]=1;
  while(first<last){long at=queue[first++];long around[4]={at-1,at+1,at-NEEDLER_HUD_WIDTH,at+NEEDLER_HUD_WIDTH};
   for(int n=0;n<4;n++){long j=around[n];if(j<0||j>=NEEDLER_HUD_PIXELS||seen[j]||
    ((n<2)&&j/NEEDLER_HUD_WIDTH!=at/NEEDLER_HUD_WIDTH))continue;
    if((p[j]>>8)>127&&(p[j]&255)<128){seen[j]=1;queue[last++]=j;}}
  }
 }
 return count;
}
int main(int argc,char **argv){char const *case_name=argc>1?argv[1]:"";
 struct weapon_definition weapon={0};struct hud_absolute_placement_definition a={0};
 struct meter_hud_element_definition m={0};m.meter_bitmap.index=1;m.alpha_multiplier=12;
 m.placement.offset.x=42;m.placement.offset.y=21;m.placement.scale.i=1;m.placement.scale.j=.8f;
 m.min_color=0x12345678;m.empty_color=0x01020304;m.opacity=.7f;m.fade=.3f;
 if(!strcmp(case_name,"allocation"))fail_at=8;
 needler_hud_loaded();
 CASE("allocation"){CHECK(allocations==deletions,"partial allocation leaked");for(int i=0;i<=30;i++)CHECK(!needler_meters[i],"partial cache survived");}
 else CASE("counts"){
  CHECK(allocations==31&&rebuilt==31,"prebuild all states during loading");
  for(int i=0;i<=30;i++)CHECK(segments(needler_meters[i])==i,"count %d rendered %d needles",i,segments(needler_meters[i]));
  for(long i=0;i<NEEDLER_HUD_PIXELS;i++){
   word first=((word*)needler_meters[0]->base_address)[i],last=((word*)needler_meters[30]->base_address)[i];
   CHECK((first&255)==255&&(last&255)==0,"binary threshold");CHECK((first>>8)==(last>>8),"coverage changed");
  }
 }
 else CASE("draw"){
  struct meter_hud_element_definition before=m;
  for(int player=0;player<4;player++)for(int count=-2;count<=32;count++){
   CHECK(needler_hud_draw(&weapon,1,player,&a,&m,count,7,.75f),"draw rejected");
   CHECK(drawn==needler_meters[PIN(count,0,30)],"wrong count texture");
   CHECK(!memcmp(&before,&m,sizeof(m)),"mutated shared map tag");
   CHECK(chosen.placement.offset.x==42&&chosen.placement.offset.y==21,"placement moved");
   CHECK(fabs(chosen_clip.x1*drawn->width*chosen.placement.scale.i-111)<.001f,"wrong width");
   CHECK(fabs(chosen_clip.y1*drawn->height*chosen.placement.scale.j-17*.8f)<.001f,"wrong height");
   CHECK(chosen.min_color==m.min_color&&chosen.empty_color==m.empty_color&&chosen.opacity==m.opacity&&chosen.fade==m.fade,"color or fade changed");
   CHECK(drawn_player==player&&drawn_flags==7&&drawn_time==.75f,"flash, split-screen or flags changed");
  }
  CHECK(allocations==31&&rebuilt==31,"per-frame allocations or uploads");
 }
 else CASE("fallback"){
  active=FALSE;CHECK(!needler_hud_draw(&weapon,1,0,&a,&m,20,0,0),"stock rules replaced");active=TRUE;
  for(int state=0;state<8;state++)if(state!=1)CHECK(!needler_hud_draw(&weapon,state,0,&a,&m,30,0,0),"another HUD state replaced");
  tag_name="custom meter";CHECK(!needler_hud_draw(&weapon,1,0,&a,&m,30,0,0),"custom bitmap replaced");tag_name="ui\\hud\\bitmaps\\combined\\hud_ammo_meters";
  original_clip.x1+=.1f;CHECK(!needler_hud_draw(&weapon,1,0,&a,&m,30,0,0),"custom layout replaced");original_clip.x1-=.1f;
  has_original=FALSE;CHECK(!needler_hud_draw(&weapon,1,0,&a,&m,30,0,0),"missing sprite used");
  CHECK(!draws,"fallback emitted a quad");
 }
 else CASE("reload"){needler_hud_loaded();CHECK(allocations==62&&deletions==31,"old map retained");}
 else return 2;
 needler_hud_unloaded();needler_hud_unloaded();CHECK(allocations==deletions,"map cleanup leaked");return 0;
}
