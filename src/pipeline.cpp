#include "pipeline.hpp"
#include "fusion.hpp"
#include "palettes.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
#ifndef ITL_SHADER_DIR
#define ITL_SHADER_DIR "shaders"
#endif
namespace itl {
static bool profile=std::getenv("ITL_PROFILE")!=nullptr;
static bool diagnostics=profile||std::getenv("ITL_DIAGNOSTICS")!=nullptr;
static std::string passName;
static std::map<std::string,std::pair<double,unsigned>> timings;
GLuint Pipeline::buffer(size_t bytes){GLuint b;glGenBuffers(1,&b);glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);glBufferData(GL_SHADER_STORAGE_BUFFER,bytes,nullptr,GL_DYNAMIC_DRAW);resources.push_back(b);return b;}
void Pipeline::update(GLuint b,const void*d,size_t n,size_t off){glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);glBufferSubData(GL_SHADER_STORAGE_BUFFER,off,n,d);}
GLuint Pipeline::program(const std::string&name){double compileStart=now();const char*overridePath=std::getenv("ITL_SHADER_DIR");std::ifstream in(std::string(overridePath?overridePath:ITL_SHADER_DIR)+"/"+name+".comp");
#ifdef ITL_BUILD_SHADER_DIR
 if(!in&&!overridePath)in.open(std::string(ITL_BUILD_SHADER_DIR)+"/"+name+".comp");
#endif
 std::stringstream ss;ss<<in.rdbuf();auto s=ss.str();if(s.empty())throw std::runtime_error("missing shader "+name);const char*src=s.c_str();GLuint shader=glCreateShader(GL_COMPUTE_SHADER);glShaderSource(shader,1,&src,nullptr);glCompileShader(shader);GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);char log[16000];if(!ok){glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(name+": "+log);}GLuint p=glCreateProgram();glAttachShader(p,shader);glLinkProgram(p);glDeleteShader(shader);glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(name+": "+log);}programs[name]=p;if(diagnostics)std::cout<<"shaderCompile="<<name<<" ms="<<(now()-compileStart)*1000<<std::endl;return p;}
void Pipeline::pass(const std::string&name,std::initializer_list<GLuint>bs,unsigned count){passName=name;auto found=programs.find(name);active=found==programs.end()?program(name):found->second;glUseProgram(active);unsigned i=0;for(auto b:bs)glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i++,b); // Dispatch follows configuration via explicit call below.
 (void)count;
}
void Pipeline::uniform(const char*n,float v){glUniform1f(glGetUniformLocation(active,n),v);}
void Pipeline::uniform(const char*n,std::array<float,2>v){glUniform2fv(glGetUniformLocation(active,n),1,v.data());}
void Pipeline::uniform(const char*n,std::array<float,4>v){glUniform4fv(glGetUniformLocation(active,n),1,v.data());}
void Pipeline::uniform(const char*n,std::array<unsigned,4>v){glUniform4uiv(glGetUniformLocation(active,n),1,v.data());}
static void dispatch(unsigned n){double start=now();glDispatchCompute((n+63)/64,1,1);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);if(profile){glFinish();auto&t=timings[passName];t.first+=(now()-start)*1000;t.second++;}}
Pipeline::Pipeline(){const char*r=reinterpret_cast<const char*>(glGetString(GL_RENDERER));renderer=r?r:"none";if(diagnostics)std::cout<<"GPU "<<renderer<<" | "<<glGetString(GL_VERSION)<<std::endl;
 GLint maxBlocks=0;glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,&maxBlocks);if(maxBlocks<8)throw std::runtime_error("need 8 compute SSBOs");

 warped=0;raw=buffer(N*8*2);slots=buffer(8*4);buildSlots=buffer(8*4);align=buffer(8*8);base=buffer(N*4);baseN=buffer(N*4);candidate=0;candidateN=0;fused=buffer(N*4);effective=buffer(N*4);models=buffer(8*16);costs=buffer(8*1445*8);deltas=buffer(8*16*65*4);patches=buffer(8*16*8);energy=0;hist=buffer(512*4);smooth=buffer(512*4);cdf=buffer(513*4);curves[0]=buffer(65536*4);curves[1]=buffer(65536*4);palette=buffer(256*4);pixels=buffer(N*4);
 pyramid[0]=0;for(int k=1;k<4;k++)pyramid[k]=buffer((N>>(2*k))*8*4);
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,H);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 std::vector<float> zero(65536,0);update(curves[0],zero.data(),zero.size()*4);update(curves[1],zero.data(),zero.size()*4);
}
Pipeline::~Pipeline(){if(profile)for(auto&t:timings)std::cout<<t.first<<"="<<t.second.first/t.second.second<<"ms ("<<t.second.second<<")\n";glDeleteBuffers(resources.size(),resources.data());for(auto p:programs)glDeleteProgram(p.second);glDeleteTextures(1,&texture);}
void Pipeline::reset(){history.clear();previous.reset();initialized=false;haveWindow=false;tick=0;holdTime=0;holdDirection=0;pyramidReady.fill(false);}
void Pipeline::process(const std::shared_ptr<Frame>&f){double start=now();bool same=previous&&f->raw==previous->raw;bool repeated=same;if(repeated){repeats++;milliseconds=(now()-start)*1000;return;}
 Background drift=previous&&f->time-previous->time<=.35?background(f->raw.data(),previous->raw.data()):Background{};
 if(!locked&&haveWindow&&drift.confidence>=.5){low+=drift.offset;high+=drift.offset;}
 float minimum,maximum;
#if defined(__aarch64__)
 auto mn=vdupq_n_u16(65535),mx=vdupq_n_u16(0);for(size_t i=0;i<N;i+=8){auto v=vld1q_u16(f->raw.data()+i);mn=vminq_u16(mn,v);mx=vmaxq_u16(mx,v);}minimum=vminvq_u16(mn);maximum=vmaxvq_u16(mx);
#else
 auto mm=std::minmax_element(f->raw.begin(),f->raw.end());minimum=*mm.first;maximum=*mm.second;
#endif
 if(autogate&&!curveTime)initialized=false;
 bool curveUpdate=autogate&&(!initialized||(!locked&&tick%4==0));
 if(!haveWindow||(!locked&&tick%4==0)){std::array<unsigned,65536> bins{};for(size_t y=0;y<H;y+=2)for(size_t x=0;x<W;x+=2)bins[f->raw[y*W+x]]++;unsigned total=0;float targetLow=minimum,targetHigh=maximum;bool found=false;for(int i=minimum;i<=maximum;i++){total+=bins[i];if(!found&&total>=N/4/50){targetLow=i;found=true;}if(total>=N/4*49/50){targetHigh=i;break;}}targetHigh=std::max(targetLow+1,targetHigh);
 if(!haveWindow){low=targetLow;high=targetHigh;haveWindow=true;}
 else {float span=std::max(16.f,high-low),dl=targetLow-low,dh=targetHigh-high;
 if(drift.confidence<.5&&std::abs(dl)>span*.5&&std::abs(dh)>span*.5){low=targetLow;high=targetHigh;history.clear();initialized=false;holdTime=0;}
 else {bool contract=targetHigh-targetLow<(high-low)*.85;int dir=dh>span*.25&&std::abs(dl)<span*.25?1:(dl<-span*.25&&std::abs(dh)<span*.25?-1:0);bool foreground=drift.confidence>=.5&&dir!=0&&!contract;
 if(foreground&&(!holdTime||holdDirection!=dir)){holdTime=f->time;holdDirection=dir;}if(!foreground){holdTime=0;holdDirection=0;}
 double dt=std::max(0.,f->time-windowTime);if(!holdTime||f->time-holdTime>=.3){double tau=contract?.2:(!holdTime?.4:.5);float a=1-std::exp(-dt/tau);low+=dl*a;high+=dh*a;}
 }}windowTime=f->time;}
 std::array<float,4> bounds{std::min(minimum,low),low,high,std::max(maximum,high)};
 // Find a free physical slot, keeping seven previous original observations resident.
 unsigned current=slot;while(std::any_of(history.begin(),history.end(),[&](auto&h){return h.first==current;}))current=(current+1)%8;slot=(current+1)%8;
 rawReady[current]=motion||!cpuFusion;if(rawReady[current])update(raw,f->raw.data(),N*2,current*N*2);pyramidReady[current]=false;
 std::vector<unsigned> causalSlots{current},motionSlots{current};std::vector<std::array<float,2>> aligns{{0,1}};std::vector<float> noises;std::vector<const uint16_t*> causalRaw{f->raw.data()},motionRaw{f->raw.data()};std::vector<Background> backgrounds{{0,1,.5}};
 for(auto&h:history){if(motionSlots.size()>=size_t(observations))break;if(f->time-h.second->time>.35)continue;motionSlots.push_back(h.first);motionRaw.push_back(h.second->raw.data());if(motion&&!rawReady[h.first]){update(raw,h.second->raw.data(),N*2,h.first*N*2);rawReady[h.first]=true;}auto b=background(f->raw.data(),h.second->raw.data());if(b.confidence>0&&std::abs(b.offset)<=32){causalSlots.push_back(h.first);causalRaw.push_back(h.second->raw.data());backgrounds.push_back(b);aligns.push_back({b.offset,b.confidence});noises.push_back(b.sigma);}}
 std::sort(noises.begin(),noises.end());sigma=noises.empty()?.5:std::max(.5f,noises[noises.size()/2]);update(slots,causalSlots.data(),causalSlots.size()*4);update(align,aligns.data(),aligns.size()*8);
 const Fusion*cpuBase=nullptr; if(cpuFusion){cpuBase=&causalCPU(causalRaw,backgrounds,sigma,fusionWorkspace);update(base,cpuBase->counts.data(),N*4);if(curveUpdate)update(baseN,cpuBase->ne.data(),N*4);}else {pass("rawCausalFuse",{raw,base,baseN,0,align,0,slots},N);uniform("p",std::array<unsigned,4>{N,unsigned(causalSlots.size()),W,H});uniform("sigma",sigma);dispatch(N);}
 GLuint counts=base,ne=baseN;trusted=causalSlots.size()-1;
 if(motion&&motionSlots.size()>1){unsigned n=motionSlots.size();update(slots,motionSlots.data(),n*4);std::vector<unsigned> dirty;for(auto v:motionSlots)if(!pyramidReady[v])dirty.push_back(v);update(buildSlots,dirty.data(),dirty.size()*4);unsigned dirtyCount=dirty.size();std::array<unsigned,4>p{dirtyCount,W,H,0};
 if(!cpuFusion){if(!pyramid[0])pyramid[0]=buffer(N*8*4);pass("motionCopyRAW",{raw,pyramid[0],buildSlots},dirtyCount*N);uniform("p",p);dispatch(dirtyCount*N);}
 pass("motionReduceRAW",{raw,pyramid[1],buildSlots},dirtyCount*N/4);uniform("p",p);dispatch(dirtyCount*N/4);
 for(int k=2;k<4;k++){p[1]=320>>(k-2);p[2]=256>>(k-2);pass("motionReduce",{pyramid[k-1],pyramid[k],buildSlots},dirtyCount*(N>>(k*2)));uniform("p",p);dispatch(dirtyCount*(N>>(k*2)));}
 for(auto v:dirty)pyramidReady[v]=true;
 unsigned radii[]={8,4,4,6},angles[]={2,3,2,1};float steps[]={8,4,1,.25},aSteps[]={3,.5,.125,.0625};
 for(int stage=0;stage<4;stage++){p={n,radii[stage],angles[stage],unsigned(stage)};unsigned nc=(2*p[1]+1)*(2*p[1]+1)*(2*p[2]+1);std::array<float,4>v{float(80<<stage),float(64<<stage),steps[stage],aSteps[stage]*float(M_PI)/180};
 pass(stage==3&&cpuFusion?"motionCostsRAW":"motionCosts",{stage==3&&cpuFusion?raw:pyramid[3-stage],slots,models,costs},n*nc);uniform("p",p);uniform("v",v);dispatch(n*nc);
 pass(reference?"motionSelect":"motionSelectParallel",{costs,models},n);uniform("p",p);uniform("v",v);uniform("sigma",sigma);dispatch(reference?n:n*64);}
 p={n,0,0,0};pass("motionBackgroundDeltas",{raw,slots,models,deltas},n);uniform("p",p);dispatch(n);
 pass("motionBackgroundPatches",{raw,slots,models,patches,0,deltas},n*16);uniform("p",p);dispatch(n*16);
 pass("motionBackground",{raw,slots,models,align,0,patches},n);uniform("p",p);dispatch(n);
 if(cpuFusion){glFinish();std::vector<std::array<float,4>> m(n);std::vector<std::array<float,2>> a(n);glBindBuffer(GL_SHADER_STORAGE_BUFFER,models);auto mp=glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,n*16,GL_MAP_READ_BIT);if(!mp)throw std::runtime_error("model map failed");std::memcpy(m.data(),mp,n*16);glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);glBindBuffer(GL_SHADER_STORAGE_BUFFER,align);auto ap=glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,n*8,GL_MAP_READ_BIT);if(!ap)throw std::runtime_error("alignment map failed");std::memcpy(a.data(),ap,n*8);glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);const auto&result=motionCPU(motionRaw,m,a,*cpuBase,sigma,fusionWorkspace);if(&result!=cpuBase){update(fused,result.counts.data(),N*4);if(curveUpdate)update(effective,result.ne.data(),N*4);counts=fused;ne=effective;}}
 else {if(!candidate){candidate=buffer(N*4);candidateN=buffer(N*4);energy=buffer(N*8);}if(reference){pass("motionFuse",{raw,slots,models,align,base,baseN,candidate,candidateN},N);uniform("p",p);uniform("sigma",sigma);dispatch(N);}else {
 if(!warped)warped=buffer(N*7*8);pass("motionWarp",{raw,slots,models,align,warped},(n-1)*N);glUniform1ui(glGetUniformLocation(active,"count"),n);dispatch((n-1)*N);
 pass("motionFuseCached",{raw,slots,warped,align,base,baseN,candidate,candidateN},N);glUniform1ui(glGetUniformLocation(active,"count"),n);uniform("sigma",sigma);dispatch(N);
 }
 pass("motionEnergy",{base,candidate,energy},N);dispatch(N);
 pass("motionProtect",{base,baseN,candidate,candidateN,fused,effective,energy},N);uniform("sigma",sigma);dispatch(N);counts=fused;ne=effective; }
 }
 if(curveUpdate){std::array<unsigned,512>zero{};update(hist,zero.data(),2048);pass("autogateHistogram",{counts,hist},20480);uniform("bounds",bounds);dispatch(20480);
 pass("autogateSmooth",{hist,smooth},512);uniform("noiseSpan",std::array<float,2>{sigma,high-low});dispatch(512);
 pass(reference?"autogateCDF":"autogateCDFParallel",{smooth,cdf,ne},1);uniform("noiseSpan",std::array<float,2>{sigma,high-low});dispatch(1);}
 bool migrate=autogate&&!locked&&initialized&&(curveLow!=low||curveHigh!=high);if(curveUpdate||migrate){unsigned next=1-curveIndex;pass("autogateCurve",{curves[curveIndex],curves[next],cdf},65536);uniform("b",bounds);uniform("control",std::array<float,4>{0,float(1-std::exp(-std::max(0.,f->time-curveTime)/.45)),curveUpdate?1.f:0.f,initialized?0.f:1.f});uniform("migration",std::array<float,4>{initialized?curveLow:low,initialized?curveHigh:high,low,high});dispatch(65536);curveIndex=next;}initialized=true;curveLow=low;curveHigh=high;if(autogate&&(curveUpdate||locked))curveTime=f->time;else if(!autogate)curveTime=0;
 if(uploadedPalette!=paletteMode){auto lut=paletteLUT(paletteMode);update(palette,lut.data(),1024);uploadedPalette=paletteMode;}
 pass("autogateTone",{counts,palette,pixels,curves[curveIndex]},N);
 uniform("windowBounds",std::array<float,2>{low,high});
 glUniform1i(glGetUniformLocation(active,"useAutogate"),autogate?1:0);dispatch(N);
 glMemoryBarrier(GL_PIXEL_BUFFER_BARRIER_BIT);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pixels);glBindTexture(GL_TEXTURE_2D,texture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glFinish();GLenum e=glGetError();if(e!=GL_NO_ERROR)throw std::runtime_error("GPU GL error "+std::to_string(e));
 lastCounts=counts;history.push_front({current,f});while(history.size()>size_t(observations-1))history.pop_back();previous=f;rendered++;tick++;milliseconds=(now()-start)*1000;
}
std::vector<float> Pipeline::readCounts(){GLuint b=lastCounts;glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);auto*p=static_cast<float*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,N*4,GL_MAP_READ_BIT));if(!p)throw std::runtime_error("count map failed");std::vector<float>v(p,p+N);glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);return v;}
void Pipeline::readPixels(void*destination){glBindBuffer(GL_SHADER_STORAGE_BUFFER,pixels);auto*p=glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,N*4,GL_MAP_READ_BIT);if(!p)throw std::runtime_error("pixel map failed");std::memcpy(destination,p,N*4);glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);}
std::vector<uint32_t> Pipeline::readPixels(){std::vector<uint32_t>v(N);readPixels(v.data());return v;}
}
