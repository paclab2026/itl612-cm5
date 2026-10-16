#include "fusion.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
using namespace itl;
namespace itl {Fusion causalBaseline(const std::vector<const uint16_t*>&,const std::vector<Background>&,float);Fusion motionBaseline(const std::vector<const uint16_t*>&,const std::vector<std::array<float,4>>&,const std::vector<std::array<float,2>>&,const Fusion&,float);}
int main(){try{
 std::array<std::vector<uint16_t>,8> frames;for(int k=0;k<8;k++){frames[k].resize(N);for(size_t y=0;y<H;y++)for(size_t x=0;x<W;x++){int v=32000+int(90*std::sin((x+k*.7)*.07)+65*std::cos(y*.08))+((x*13+y*7+k)%3)-1;if(x>240&&x<300&&y>220&&y<280)v+=2000;frames[k][y*W+x]=v;}}
 float peak=0,peakNe=0;double total=0;uint64_t count=0;
 for(int scenario=0;scenario<4;scenario++)for(int n:{1,4,8}){
  std::vector<const uint16_t*> raw;std::vector<Background>b;std::vector<std::array<float,4>> m;std::vector<std::array<float,2>>a;
  for(int k=0;k<n;k++){raw.push_back(frames[k].data());b.push_back({float(k%3),k?0.75f:1,.7f});m.push_back({float(k)*-.7f,float(k%2)*.3f,scenario==1?float(k)*.005f:0,1});a.push_back({float(k%3),scenario==2&&k?0.f:0.75f});}
  if(scenario==3){for(int k=0;k<n;k++)m[k][0]=630;}
  float sigma=scenario==0?.5f:2.f;auto cb=causalBaseline(raw,b,sigma);FusionWorkspace w;const auto&ca=causalCPU(raw,b,sigma,w);auto mb=motionBaseline(raw,m,a,cb,sigma);const auto&ma=motionCPU(raw,m,a,ca,sigma,w);
  for(size_t i=0;i<N;i++){if(!std::isfinite(ma.counts[i])||!std::isfinite(ma.ne[i]))throw std::runtime_error("nonfinite result");float e=std::abs(mb.counts[i]-ma.counts[i]);peak=std::max({peak,e,std::abs(cb.counts[i]-ca.counts[i])});peakNe=std::max({peakNe,std::abs(mb.ne[i]-ma.ne[i]),std::abs(cb.ne[i]-ca.ne[i])});total+=e;count++;}
  if(scenario==1&&n==4){double t=now();for(int k=0;k<20;k++){auto old=motionBaseline(raw,m,a,cb,sigma);(void)old;}double oldMS=(now()-t)*50;t=now();for(int k=0;k<20;k++)motionCPU(raw,m,a,ca,sigma,w);double newMS=(now()-t)*50;std::cout<<"motionCPU baselineMS="<<oldMS<<" optimizedMS="<<newMS<<" speedup="<<oldMS/newMS<<std::endl;}
 }
 std::cout<<"fusion parity scenarios=12 maxCount="<<peak<<" meanCount="<<total/count<<" maxNe="<<peakNe<<std::endl;
 if(peak>.01f||peakNe>.0001f)throw std::runtime_error("CPU equations diverged");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
