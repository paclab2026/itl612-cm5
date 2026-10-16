#pragma once
#include "mars.hpp"
#include "fusion.hpp"
#include <GLES3/gl31.h>
#include <map>
namespace itl {
class Pipeline {
 std::map<std::string,GLuint> programs;std::vector<GLuint> resources;
 int uploadedPalette=-1;
 GLuint warped;GLuint raw,slots,buildSlots,align,base,baseN,candidate,candidateN,fused,effective,models,costs,deltas,patches,energy,hist,smooth,cdf,curves[2],palette,pixels,pyramid[4];
 FusionWorkspace fusionWorkspace;
std::deque<std::pair<unsigned,std::shared_ptr<Frame>>> history;
 unsigned slot=0,tick=0,curveIndex=0;bool initialized=false,haveWindow=false;double windowTime=0,curveTime=0,holdTime=0;int holdDirection=0;
 float low=0,high=1,curveLow=0,curveHigh=1;std::shared_ptr<Frame> previous;GLuint texture=0;
 GLuint buffer(size_t bytes);GLuint program(const std::string&name);void pass(const std::string&name,std::initializer_list<GLuint> buffers,unsigned count);
 void uniform(const char*name,float v);void uniform(const char*name,std::array<float,2>v);void uniform(const char*name,std::array<float,4>v);void uniform(const char*name,std::array<unsigned,4>v);
 std::array<bool,8> pyramidReady{},rawReady{};GLuint lastCounts=0;GLuint active=0;void update(GLuint b,const void*data,size_t size,size_t offset=0);
 public:bool cpuFusion=true,reference=false;bool motion=true,locked=false,autogate=true;int observations=8,paletteMode=0;double milliseconds=0;float sigma=.5;uint64_t rendered=0,repeats=0;std::string renderer;unsigned trusted=0;
 Pipeline();~Pipeline();void reset();void process(const std::shared_ptr<Frame>& f);GLuint outputTexture()const{return texture;}
 std::vector<float> readCounts();std::vector<uint32_t> readPixels();
 void readPixels(void*destination);
};
}
