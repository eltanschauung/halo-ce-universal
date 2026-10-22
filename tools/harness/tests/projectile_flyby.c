#include "harness.h"
#include <math.h>
#define MAXIMUM_LOCAL_PLAYERS 2
struct location {short cluster;};
struct sound_location {real_point3d position;real_vector3d forward,translational_velocity;struct location game_location;};
struct projectile_datum {long definition_index;struct {real_point3d position;}object;} projectile;
struct projectile_definition {struct {struct {long index;}flyby_sound;}projectile;} definition;
struct object_datum {struct {real_point3d bounding_sphere_center;}object;} units[2];
struct player_datum {long unit_index;} players[2]={{0},{1}};
static long locals[2]={0,NONE};
static int sounds;
static struct sound_location heard;
static real_vector3d zero;
#define global_zero_vector3d (&zero)
static struct projectile_datum *projectile_get(long i){return &projectile;}
static struct projectile_definition *projectile_definition_get(long i){return &definition;}
static long local_player_get_player_index(short i){return locals[i];}
static struct player_datum *player_get(long i){return &players[i];}
static struct object_datum *object_get(long i){return &units[i];}
static real sound_definition_get_maximum_distance(long tag){return 1.f;}
static real dot_product3d(real_vector3d const*a,real_vector3d const*b){return a->i*b->i+a->j*b->j+a->k*b->k;}
static real magnitude_squared3d(real_vector3d const*a){return dot_product3d(a,a);}
static void component_vectors_from_direction3d(real_vector3d const*a,real_vector3d const*d,real_vector3d*p,real_vector3d*n){float dd=dot_product3d(d,d),t=dd?dot_product3d(a,d)/dd:0;*p=(real_vector3d){d->i*t,d->j*t,d->k*t};*n=(real_vector3d){a->i-p->i,a->j-p->j,a->k-p->k};}
static void point_from_line3d(real_point3d const*p,real_vector3d const*v,real t,real_point3d*q){*q=(real_point3d){p->x+t*v->i,p->y+t*v->j,p->z+t*v->k};}
static real normalize3d(real_vector3d*v){float n=sqrtf(dot_product3d(v,v));if(n){v->i/=n;v->j/=n;v->k/=n;}return n;}
static void unattached_impulse_sound_new(long tag,struct sound_location const*l,real volume){CHECK(tag==5&&volume==1,"changed original flyby sound");sounds++;heard=*l;}
#include "under_test.inc"
int main(int argc,char**argv){CHECK(argc==2,"case");char const *case_name=argv[1];definition.projectile.flyby_sound.index=5;units[0].object.bounding_sphere_center=(real_point3d){1,.2f,0};struct location where={17};real_vector3d movement={2,0,0};long ignored=NONE;
 CASE("near"){}else CASE("far"){units[0].object.bounding_sphere_center.y=2;}else CASE("past"){units[0].object.bounding_sphere_center.x=-1;}else CASE("ahead"){units[0].object.bounding_sphere_center.x=3;}else CASE("zero"){movement=(real_vector3d){0};}else CASE("owner"){ignored=0;}else CASE("silent"){definition.projectile.flyby_sound.index=NONE;}else CASE("no-player"){locals[0]=NONE;}else CASE("no-unit"){players[0].unit_index=NONE;}else CASE("split-screen"){locals[1]=1;units[1].object.bounding_sphere_center=(real_point3d){1,-.2f,0};}else return 2;
 boolean result=projectile_play_flyby_sound(0,&movement,&where,ignored);int expected=!strcmp(case_name,"near")?1:!strcmp(case_name,"split-screen")?2:0;CHECK(sounds==expected&&result==(expected!=0),"flybys %d expected %d",sounds,expected);if(expected)CHECK(heard.game_location.cluster==17&&heard.forward.i==1&&heard.position.x==1&&heard.position.y==0,"positional sound differs");return 0;
}
