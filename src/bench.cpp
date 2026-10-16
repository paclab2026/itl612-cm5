#include "pipeline.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
using namespace itl;
int main(int argc,char**argv){try{
 auto getDisplay=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));EGLDisplay d=getDisplay?getDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr):eglGetDisplay(EGL_DEFAULT_DISPLAY);if(!eglInitialize(d,nullptr,nullptr))throw std::runtime_error("EGL initialize failed");eglBindAPI(EGL_OPENGL_ES_API);EGLint ca[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_NONE};EGLConfig cfg;EGLint count;eglChooseConfig(d,ca,&cfg,1,&count);EGLint attrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,attrs);if(!eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,c))throw std::runtime_error("EGL make current failed");
 if(argc>1&&std::string(argv[1])=="--compile"){Pipeline p;return 0;}
 if(argc>1&&std::string(argv[1])=="--compare-stdin"){
 Pipeline reference;reference.cpuFusion=false;reference.reference=true;reference.locked=true;reference.observations=8;Pipeline optimized;optimized.locked=true;optimized.observations=8;
 float peak=0;double sum=0;uint64_t compared=0,above=0;unsigned frames=0,maxGray=0,worstFrame=0,worstPixel=0;float worstInput=0,worstExpected=0,worstActual=0;double start=now();
 while(true){auto f=std::make_shared<Frame>();std::cin.read(reinterpret_cast<char*>(f->raw.data()),N*2);if(std::cin.gcount()==0)break;if(std::cin.gcount()!=N*2)throw std::runtime_error("truncated existing RAW input");f->index=++frames;f->time=start+frames/30.0;reference.process(f);optimized.process(f);auto expected=reference.readCounts(),actual=optimized.readCounts();for(size_t i=0;i<N;i++){float e=std::abs(expected[i]-actual[i]);if(!std::isfinite(actual[i]))throw std::runtime_error("nonfinite replay output");if(e>peak){peak=e;worstFrame=frames;worstPixel=i;worstInput=f->raw[i];worstExpected=expected[i];worstActual=actual[i];}above+=e>.25;sum+=e;compared++;}auto ep=reference.readPixels(),ap=optimized.readPixels();for(size_t i=0;i<N;i++)maxGray=std::max(maxGray,unsigned(std::abs(int(ep[i]&255)-int(ap[i]&255))));}
 if(frames<8)throw std::runtime_error("comparison needs eight existing frames");std::cout<<"Existing clip comparison: frames="<<frames<<" maxCountError="<<peak<<" meanCountError="<<sum/compared<<" maxGrayError="<<maxGray<<" aboveQuarterCount="<<above<<" compared="<<compared<<" worstFrame="<<worstFrame<<" xy="<<worstPixel%640<<","<<worstPixel/640<<" input="<<worstInput<<" reference="<<worstExpected<<" optimized="<<worstActual<<"\n";if(sum/compared>.03)throw std::runtime_error("existing clip mean/reference disagreement");return 0;}
 if(argc>1&&std::string(argv[1])=="--speedtest"){
 int obs=argc>2?std::stoi(argv[2]):4;bool motion=argc>3?std::stoi(argv[3])!=0:true;Pipeline p;p.observations=obs;p.motion=motion;
 std::array<std::shared_ptr<Frame>,32> frames;for(int step=0;step<32;step++){auto f=std::make_shared<Frame>();for(int y=0;y<512;y++)for(int x=0;x<640;x++){double xx=x+step*.75;int value=32000+int(95*std::sin(xx*.09)+60*std::cos(y*.07)+70*std::sin((xx+y)*.11))+step%3;value+=((x*17+y*13+step*7)%3)-1;if(step>=5&&x>=260&&x<320&&y>=220&&y<280)value+=2000;f->raw[y*640+x]=value;}frames[step]=f;}
 std::vector<double> timing;double start=now(),origin=start;for(int step=0;step<104;step++){auto f=frames[step%32];f->index=step+1;f->time=origin+step/30.;p.process(f);if(step==7)start=now();if(step>=8)timing.push_back(p.milliseconds);}double elapsed=now()-start;std::sort(timing.begin(),timing.end());auto counts=p.readCounts();double checksum=0;for(auto value:counts)checksum+=value;
 std::cout<<"SPEEDTEST "<<"observations="<<obs<<" motion="<<motion<<" frames=96 fps="<<96/elapsed<<" p50MS="<<timing[timing.size()/2]<<" p95MS="<<timing[size_t((timing.size()-1)*.95)]<<" checksum="<<checksum<<std::endl;return 0;
 }
 if(argc>1&&std::string(argv[1])=="--selftest"){
 Pipeline p;p.locked=true;p.observations=8;auto a=std::make_shared<Frame>();a->time=now();a->index=1;
 for(size_t y=0;y<H;y++)for(size_t x=0;x<W;x++)a->raw[y*W+x]=3000+unsigned(120*std::sin(x*.09)+80*std::cos(y*.07)+60*std::sin((x+y)*.11)+260);
 p.process(a);auto out=p.readCounts();for(size_t i=0;i<N;i++)if(out[i]!=a->raw[i])throw std::runtime_error("single-frame count mismatch");
 auto b=std::make_shared<Frame>(*a);b->time+=.033;b->index=2;for(auto&v:b->raw)v+=3;p.process(b);out=p.readCounts();float maxError=0;for(size_t i=0;i<N;i++){if(!std::isfinite(out[i]))throw std::runtime_error("nonfinite motion output");maxError=std::max(maxError,std::abs(out[i]-b->raw[i]));}if(maxError>.03)throw std::runtime_error("uniform drift parity error "+std::to_string(maxError));
 auto pixels=p.readPixels();for(auto c:pixels)if((c>>24)!=255)throw std::runtime_error("alpha mismatch");
 auto c=std::make_shared<Frame>(*b);c->time+=.033;c->index=3;for(int y=220;y<280;y++)for(int x=260;x<320;x++)c->raw[y*W+x]+=2000;p.process(c);out=p.readCounts();float worst=0;for(int y=222;y<278;y++)for(int x=262;x<318;x++)worst=std::max(worst,std::abs(out[y*W+x]-c->raw[y*W+x]));if(worst>.03)throw std::runtime_error("new hot object corrupted "+std::to_string(worst));
 // Check every RAW16 bit pattern through the packed SSBO reference path.
 Pipeline fullRange;fullRange.cpuFusion=false;fullRange.motion=false;fullRange.observations=1;fullRange.autogate=false;auto range=std::make_shared<Frame>();range->time=a->time;for(size_t i=0;i<N;i++)range->raw[i]=uint16_t(i);fullRange.process(range);auto fullCounts=fullRange.readCounts();for(size_t i=0;i<N;i++)if(fullCounts[i]!=range->raw[i])throw std::runtime_error("packed RAW16 bit pattern mismatch");
 auto before=p.rendered;p.process(c);if(p.rendered!=before||!p.repeats)throw std::runtime_error("duplicate counted as new observation");
 // Compare the CM5 CPU/NEON implementation with the direct GLSL port of
 // production Metal equations on identical in-memory moving scenes.
 Pipeline reference;reference.cpuFusion=false;reference.reference=true;reference.locked=true;reference.observations=8;
 Pipeline optimized;optimized.locked=true;optimized.observations=8;float peak=0;double sumError=0;unsigned long compared=0;unsigned maxToneError=0;
 for(int step=0;step<12;step++){auto f=std::make_shared<Frame>();f->index=step+1;f->time=a->time+step*.033;
 for(int y=0;y<512;y++)for(int x=0;x<640;x++){double xx=x+step*.75;int value=32000+int(95*std::sin(xx*.09)+60*std::cos(y*.07)+70*std::sin((xx+y)*.11))+step%3;value+=((x*17+y*13+step*7)%3)-1;if(step>=5&&x>=260&&x<320&&y>=220&&y<280)value+=2000;f->raw[y*640+x]=value;}
 reference.process(f);optimized.process(f);auto expected=reference.readCounts(),actual=optimized.readCounts();for(size_t i=0;i<N;i++){float e=std::abs(expected[i]-actual[i]);if(!std::isfinite(actual[i]))throw std::runtime_error("nonfinite optimized output");peak=std::max(peak,e);sumError+=e;compared++;}auto ep=reference.readPixels(),ap=optimized.readPixels();for(size_t i=0;i<N;i++)maxToneError=std::max(maxToneError,unsigned(std::abs(int(ep[i]&255)-int(ap[i]&255))));}
 double mean=sumError/compared;std::cout<<"CPU/NEON vs direct GPU equations: 12 frames, maxCountError="<<peak<<", meanCountError="<<mean<<", maxToneError="<<maxToneError<<"\n";if(maxToneError>1)throw std::runtime_error("tone/reference disagreement");if(peak>1||mean>.03)throw std::runtime_error("CPU/GPU reference disagreement");
 // The display-only Autogate switch must not alter original/fused counts.
 Pipeline linear;linear.observations=1;linear.motion=false;linear.locked=true;linear.autogate=false;
 linear.process(a);auto white=linear.readPixels();auto exact=linear.readCounts();
 for(size_t i=0;i<N;i++)if(exact[i]!=a->raw[i])throw std::runtime_error("linear display changed counts");
 linear.reset();linear.paletteMode=1;linear.process(a);auto black=linear.readPixels();
 for(size_t i=0;i<N;i++)if(int(white[i]&255)+int(black[i]&255)!=255)throw std::runtime_error("white/black palette complement failed");
 std::cout<<"Linear display and inverse palettes passed\n";
 std::cout<<"GPU selftest passed: single-frame RAW exact, uniform drift maxError="<<maxError<<", hot foreground maxError="<<worst<<", opaque palette, duplicate cache\n";return 0;}
 double seconds=argc>1?std::stod(argv[1]):15;int obs=argc>2?std::stoi(argv[2]):8;bool motion=argc>3?std::stoi(argv[3]):true;if(obs!=1&&obs!=4&&obs!=8)throw std::runtime_error("observations must be 1, 4 or 8");Pipeline p;p.observations=obs;p.motion=motion;Camera cam;if(!cam.start())throw std::runtime_error(cam.status());double start=now();std::vector<double>ms;uint64_t shown=0,skips=0;
 while(now()-start<seconds){auto fs=cam.take();if(fs.empty()){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}skips+=fs.size()-1;auto f=fs.back();p.process(f);shown++;if(now()-start>3)ms.push_back(p.milliseconds);}
 cam.stop();double elapsed=now()-start;std::sort(ms.begin(),ms.end());std::cout<<"{\"elapsed\":"<<elapsed<<",\"usbFrames\":"<<cam.frames<<",\"usbFPS\":"<<cam.frames/elapsed<<",\"displayFrames\":"<<shown<<",\"displayFPS\":"<<shown/elapsed<<",\"skippedPreview\":"<<skips<<",\"queueDrops\":"<<cam.dropped<<",\"syncLoss\":"<<cam.syncLoss<<",\"timeouts\":"<<cam.timeouts<<",\"p50MS\":"<<(ms.empty()?0:ms[ms.size()/2])<<",\"p95MS\":"<<(ms.empty()?0:ms[size_t((ms.size()-1)*.95)])<<",\"observations\":"<<obs<<",\"motion\":"<<motion<<",\"error\":\""<<cam.status()<<"\"}\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
