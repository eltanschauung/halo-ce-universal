#include "harness.h"
#include <math.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAXIMUM_PREDICTED_SPEED 2.f
#define PREDICTION_CEILING_LEAD_TICKS 15
#define PREDICTION_JITTER_TICKS 6
static real rtt;
static real distributed_machine_round_trip_ticks(int i){return rtt;}
static int distributed_player_machine(short i){return 0;}
#include "under_test.inc"
int main(int argc,char **argv)
{
    char const *case_name=argc>1?argv[1]:"";
    struct distributed_on_foot_bound b={0};
    b.gravity=.0035f;b.speed=.1f;b.top_height=10;b.rise=10;
    b.rise_speed=.2f;b.rise_base=.5f;b.airborne_ticks=20;
    CASE("stock")
    {
        real expected=.1f+sqrtf(2.f*.0035f*2.f);
        CHECK(fabsf(distributed_on_foot_fall_speed(&b,8)-expected)<.00001,"stock fall changed");
    }
    else
    {
        b.gravity=800.f/120.f/900.f;
        rtt=!strcmp(case_name,"source-100ms")?3.f:6.f;
        real ticks=b.airborne_ticks-rtt-6;
        real expected=b.rise_base+b.rise_speed*ticks-.5f*b.gravity*ticks*ticks;
        CHECK(fabsf(distributed_on_foot_ceiling(0,&b)-expected)<.00001,"Source ceiling disagrees at latency");
        CHECK(fabsf(distributed_on_foot_fall_speed(&b,8)-(.1f+sqrtf(2.f*b.gravity*2.f)))<.00001,"Source fall uses stock gravity");
        if(!strcmp(case_name,"hover")){b.airborne_ticks=100;CHECK(distributed_on_foot_ceiling(0,&b)<0,"hovering is unbounded");}
    }
    return 0;
}
