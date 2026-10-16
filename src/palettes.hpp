#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
namespace itl {
inline std::array<uint32_t,256> paletteLUT(int mode){
 using S=std::array<double,4>;std::vector<S> stops;
 switch(mode){
 case 2:stops={{0,.01,.01,.03},{.1,.1,0,.25},{.25,.35,0,.55},{.4,.65,0,.6},{.55,.95,.1,.3},{.7,1,.45,.1},{.85,1,.8,.25},{1,1,1,.9}};break;
 case 3:stops={{0,.05,0,.1},{.2,0,0,.6},{.4,0,.5,1},{.55,0,1,.4},{.72,1,1,0},{.9,1,.35,0},{1,1,.05,.05}};break;
 case 4:stops={{0,0,0,0},{.4,.5,0,0},{.7,1,.3,0},{1,1,1,.6}};break;
 case 5:stops={{0,0,0,.4},{.5,0,.8,1},{1,1,1,1}};break;
 case 6:stops={{0,0,0,0},{.3,.2,0,.1},{.6,1,.4,0},{1,1,.9,.5}};break;
 case 7:stops={{0,.1,0,.2},{.35,.8,.1,.6},{.6,1,.5,0},{.8,1,1,.3},{1,1,1,1}};break;
 case 8:stops={{0,0,0,0},{.5,.4,.2,0},{1,1,.8,.3}};break;
 }
 std::array<uint32_t,256> out{};
 for(int i=0;i<256;i++){unsigned r=i,g=i,b=i;if(mode==1)r=g=b=255-i;
 else if(!stops.empty()){double t=i/255.;size_t j=0;while(j+2<stops.size()&&t>stops[j+1][0])j++;double f=(t-stops[j][0])/(stops[j+1][0]-stops[j][0]);auto c=[&](int k){return unsigned(std::clamp((stops[j][k]+(stops[j+1][k]-stops[j][k])*f)*255.,0.,255.));};r=c(1);g=c(2);b=c(3);}
 out[i]=0xff000000u|(b<<16)|(g<<8)|r;
 }return out;
}
}
