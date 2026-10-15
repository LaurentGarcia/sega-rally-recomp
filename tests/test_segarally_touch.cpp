#include "app/segarally_touch.h"
#include <cstdio>
#include <limits>
#include <cstdlib>
using namespace rally;
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void down(TouchControls &t,Button b,uint64_t id=1,uint64_t dev=1){const auto r=t.boxes[b];CHECK(t.down(dev,id,r.x+r.w/2,r.y+r.h/2));}
int main(){
    TouchControls t;t.layout(844,390);
    for(Button b:{Gas,Brake,Coin,Start,View,GearDown,GearUp,Handbrake}) {
        down(t,b);CHECK(t.pressed(b));t.up(1,1);CHECK(t.pressed(b));
        t.consume();CHECK(!t.pressed(b));
        down(t,b);t.up(1,1,true);CHECK(!t.pressed(b));
    }
    down(t,Gas,7,1);down(t,Brake,7,2);CHECK(t.pressed(Gas)&&t.pressed(Brake));
    t.up(1,7,true);CHECK(!t.pressed(Gas)&&t.pressed(Brake));t.up(2,7,true);
    down(t,Gas);auto brake=t.boxes[Brake];t.move(1,1,brake.x+1,brake.y+1);
    CHECK(!t.pressed(Gas)&&!t.pressed(Brake));
    CHECK(!t.down(1,1,brake.x+1,brake.y+1));CHECK(!t.pressed(Brake));
    auto gas=t.boxes[Gas];t.move(1,1,gas.x+1,gas.y+1);CHECK(t.pressed(Gas));t.up(1,1);t.consume();
    CHECK(!t.down(1,2,-1,-1));t.move(1,2,gas.x+1,gas.y+1);CHECK(!t.pressed(Gas));t.up(1,2);
    t.move(1,99,gas.x+1,gas.y+1);CHECK(!t.pressed(Gas));
    down(t,Steering);CHECK(t.steering_active());CHECK(t.steering_adc()==128);
    auto s=t.boxes[Steering];t.move(1,1,s.x,s.y);CHECK(t.steering_adc()==0);
    t.move(1,1,s.x+s.w,s.y);CHECK(t.steering_adc()==255);
    t.move(1,1,s.x+s.w*.53f,s.y);CHECK(t.steering_adc()==128);
    t.move(1,1,s.x-100,s.y);CHECK(t.steering_adc()==0);
    down(t,Steering,2);t.up(1,1);CHECK(!t.steering_active());
    t.move(1,2,s.x+s.w,s.y);CHECK(t.steering_adc()==128);t.up(1,2);
    down(t,Steering);t.move(1,1,std::numeric_limits<float>::quiet_NaN(),0);CHECK(t.steering_adc()==128);t.up(1,1,true);
    // Cancel one finger without losing another finger's completed tap.
    down(t,Coin,1);down(t,Coin,2);t.up(1,1);t.up(1,2,true);CHECK(t.pressed(Coin));t.consume();CHECK(!t.pressed(Coin));
    // A sustained shift produces one rising edge; a later tap produces another.
    bool prior=false;int shifts=0;down(t,GearUp);
    for(int i=0;i<10;++i){const bool now=t.pressed(GearUp);shifts+=now&&!prior;prior=now;t.consume();}
    CHECK(shifts==1);t.up(1,1);CHECK(!t.pressed(GearUp));prior=false;
    down(t,GearUp);shifts+=t.pressed(GearUp)&&!prior;CHECK(shifts==2);t.up(1,1,true);
    // Pause/replay isolation retains ownership until release, but never values.
    down(t,Gas);down(t,Steering,2);t.clear();CHECK(t.blocked_contacts());
    t.move(1,1,gas.x+1,gas.y+1);t.move(1,2,s.x+s.w,s.y);
    CHECK(!t.pressed(Gas)&&!t.steering_active()&&t.steering_adc()==128);
    CHECK(!t.down(1,1,gas.x+1,gas.y+1));t.up(1,1);t.up(1,2);CHECK(!t.blocked_contacts());
    down(t,Gas);t.cancel_all();down(t,Gas);CHECK(t.pressed(Gas));t.cancel_all();
    CHECK(!t.down(1,1,gas.x+1,gas.y+1,false));CHECK(!t.pressed(Gas));t.up(1,1);
    down(t,Gas);down(t,Settings,2);CHECK(t.take_settings_request());CHECK(!t.take_settings_request());CHECK(!t.pressed(Gas));t.cancel_all();
    for(auto size:std::array<std::array<float,2>,7>{{{320,240},{480,272},{568,320},{844,390},{1024,768},{393,852},{2400,1080}}}) {
        const TouchRect safe{10,8,size[0]-20,size[1]-16};t.layout(size[0],size[1],safe);
        if(safe.w<320||safe.h<220){CHECK(t.boxes[Gas].w==0);continue;}
        for(int i=0;i<Count;++i){const auto b=t.boxes[size_t(i)];CHECK(b.w>=44&&b.h>=44);CHECK(b.x>=safe.x&&b.y>=safe.y&&b.x+b.w<=safe.x+safe.w&&b.y+b.h<=safe.y+safe.h);
            for(int j=i+1;j<Count;++j)CHECK(!TouchControls::overlap(b,t.boxes[size_t(j)]));}
    }
    t.layout(844,390);down(t,Gas);t.layout(992,768);CHECK(!t.pressed(Gas));t.move(1,1,900,700);CHECK(!t.pressed(Gas));t.cancel_all();
    t.layout(844,390,{-1,0,99,99});CHECK(t.boxes[Gas].w>0);
    t.layout(std::numeric_limits<float>::infinity(),390);CHECK(t.boxes[Gas].w==0);
    t.layout(844,390);for(uint64_t i=0;i<16;++i)down(t,Gas,i);CHECK(!t.down(1,17,gas.x+1,gas.y+1));
    for(uint64_t i=0;i<16;++i)t.up(1,i,true);CHECK(!t.pressed(Gas));
    std::printf("PASS %d touch checks\n",checks);
}
