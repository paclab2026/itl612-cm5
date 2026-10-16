#include "fusion.hpp"
// Frozen pre-optimization CPU equations, used only by fusion-check.
#include <cmath>
#include <algorithm>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
namespace itl {
Fusion causalBaseline(const std::vector<const uint16_t*>&raw,const std::vector<Background>&a,float sigma){Fusion f;f.counts.resize(N);f.ne.resize(N);float denom=9*36*sigma*sigma;auto current=raw[0];
#pragma omp parallel for schedule(static)
 for(int y=0;y<512;y++){
 int x=0;
#if defined(__aarch64__)
 for(int j=0;j<640;j++){f.counts[y*640+j]=current[y*640+j];f.ne[y*640+j]=1;}
 if(y>0&&y<511){
 auto load=[](const uint16_t* p){return vcvtq_f32_u32(vmovl_u16(vld1_u16(p)));};
 for(x=1;x<637;x+=4){int i=y*640+x;float32x4_t ref=load(current+i),sum=vdupq_n_f32(0),sw=vdupq_n_f32(1),sw2=sw;float32x4_t native[9];for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++)native[(yy+1)*3+xx+1]=load(current+i+yy*640+xx);
 for(size_t k=1;k<raw.size();k++){auto h=raw[k];auto offset=vdupq_n_f32(a[k].offset);auto mse=vdupq_n_f32(0);for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){auto d=vaddq_f32(vsubq_f32(load(h+i+yy*640+xx),native[(yy+1)*3+xx+1]),offset);mse=vaddq_f32(mse,vmulq_f32(d,d));}
 auto robust=vmaxq_f32(vdupq_n_f32(0),vsubq_f32(vdupq_n_f32(1),vdivq_f32(mse,vdupq_n_f32(denom))));auto w=vmulq_n_f32(vmulq_f32(robust,robust),a[k].confidence);auto d=vaddq_f32(vsubq_f32(load(h+i),ref),offset);sum=vaddq_f32(sum,vmulq_f32(w,d));sw=vaddq_f32(sw,w);sw2=vaddq_f32(sw2,vmulq_f32(w,w));}
 vst1q_f32(f.counts.data()+i,vaddq_f32(ref,vdivq_f32(sum,sw)));vst1q_f32(f.ne.data()+i,vdivq_f32(vmulq_f32(sw,sw),sw2));}
 }else continue;
#endif
 for(;x<640;x++){int i=y*640+x;float sum=0,sw=1,sw2=1;if(x>0&&x<639&&y>0&&y<511)for(size_t k=1;k<raw.size();k++){auto h=raw[k];float mse=0;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){int j=i+yy*640+xx;float d=float(int(h[j])-int(current[j]))+a[k].offset;mse+=d*d;}float robust=std::max(0.f,1-mse/denom),w=robust*robust*a[k].confidence;sum+=w*(float(int(h[i])-int(current[i]))+a[k].offset);sw+=w;sw2+=w*w;}f.counts[i]=float(current[i])+sum/sw;f.ne[i]=sw*sw/sw2;}}return f;}
Fusion motionBaseline(const std::vector<const uint16_t*>&raw,const std::vector<std::array<float,4>>&models,const std::vector<std::array<float,2>>&a,const Fusion&base,float sigma){size_t n=raw.size();std::vector<std::vector<float>>delta(n);std::vector<std::vector<uint8_t>>valid(n);auto current=raw[0];
 for(size_t k=1;k<n;k++){if(a[k][1]==0)continue;delta[k].resize(N);valid[k].resize(N);auto h=raw[k];float cs=std::cos(models[k][2]),sn=std::sin(models[k][2]),dx=models[k][0],dy=models[k][1];
#pragma omp parallel for schedule(static)
 for(int y=0;y<512;y++)for(int x=0;x<640;x++){int i=y*640+x;float vx=x-319.5f,vy=y-255.5f;float rx=cs*vx-sn*vy+319.5f+dx,ry=sn*vx+cs*vy+255.5f+dy;valid[k][i]=rx>=1&&ry>=1&&rx<638&&ry<510;rx=std::clamp(rx,0.f,639.f);ry=std::clamp(ry,0.f,511.f);int xx=rx,yy=ry;float fx=rx-xx,fy=ry-yy;int xx1=std::min(xx+1,639),yy1=std::min(yy+1,511);float top=float(h[yy*640+xx])*(1-fx)+float(h[yy*640+xx1])*fx,bot=float(h[yy1*640+xx])*(1-fx)+float(h[yy1*640+xx1])*fx;delta[k][i]=(top*(1-fy)+bot*fy)+a[k][0]-float(current[i]);}}
 Fusion f;f.counts.resize(N);f.ne.resize(N);float denom=9*36*sigma*sigma;
#pragma omp parallel for schedule(static)
 for(int y=0;y<512;y++)for(int x=0;x<640;x++){int i=y*640+x;float sum=0,sw=1,sw2=1;bool used=false;if(x>0&&x<639&&y>0&&y<511)for(size_t k=1;k<n;k++){if(a[k][1]==0)continue;float mse=0;bool good=true;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){int j=i+yy*640+xx;float d=delta[k][j];good=good&&valid[k][j];mse+=d*d;}if(!good)continue;float r=std::max(0.f,1-mse/denom),w=r*r*a[k][1];used|=w>0;sum+=w*delta[k][i];sw+=w;sw2+=w*w;}float ne=sw*sw/sw2;bool gain=used&&ne>base.ne[i]+.05f;f.counts[i]=gain?float(current[i])+sum/sw:base.counts[i];f.ne[i]=gain?ne:base.ne[i];}
 std::vector<std::array<float,2>>energy(N);
#pragma omp parallel for schedule(static)
 for(int y=0;y<512;y++){
 auto scalar=[&](int x){int qx=std::clamp(x,2,637),qy=std::clamp(y,2,509);float bx=0,by=0,cx=0,cy=0;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){int j=(qy+yy)*640+qx+xx;bx+=base.counts[j+1]-base.counts[j-1];by+=base.counts[j+640]-base.counts[j-640];cx+=f.counts[j+1]-f.counts[j-1];cy+=f.counts[j+640]-f.counts[j-640];}float scale=4.f/18;bx*=scale;by*=scale;cx*=scale;cy*=scale;energy[y*640+x]={bx*bx+by*by,cx*cx+cy*cy};};
 int x=0;
#if defined(__aarch64__)
 scalar(0);scalar(1);int qy=std::clamp(y,2,509);
 for(x=2;x<635;x+=4){auto bx=vdupq_n_f32(0),by=bx,cx=bx,cy=bx;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){int j=(qy+yy)*640+x+xx;const float* b=base.counts.data()+j;const float* c=f.counts.data()+j;bx=vaddq_f32(bx,vsubq_f32(vld1q_f32(b+1),vld1q_f32(b-1)));by=vaddq_f32(by,vsubq_f32(vld1q_f32(b+640),vld1q_f32(b-640)));cx=vaddq_f32(cx,vsubq_f32(vld1q_f32(c+1),vld1q_f32(c-1)));cy=vaddq_f32(cy,vsubq_f32(vld1q_f32(c+640),vld1q_f32(c-640)));}bx=vmulq_n_f32(bx,4.f/18);by=vmulq_n_f32(by,4.f/18);cx=vmulq_n_f32(cx,4.f/18);cy=vmulq_n_f32(cy,4.f/18);float32x4x2_t value{{vaddq_f32(vmulq_f32(bx,bx),vmulq_f32(by,by)),vaddq_f32(vmulq_f32(cx,cx),vmulq_f32(cy,cy))}};vst2q_f32(reinterpret_cast<float*>(energy.data()+y*640+x),value);}
#endif
 for(;x<640;x++)scalar(x);}
 Fusion result;result.counts.resize(N);result.ne.resize(N);
#pragma omp parallel for schedule(static)
 for(int y=0;y<512;y++){
 auto scalar=[&](int x){int i=y*640+x;float eb=0,ed=0;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){auto e=energy[std::clamp(y+yy,0,511)*640+std::clamp(x+xx,0,639)];eb+=e[0];ed+=e[1];}eb/=9;ed/=9;float blend=eb>9*sigma*sigma?std::clamp((.98f-std::sqrt(ed/std::max(eb,1e-6f)))/.08f,0.f,1.f):0;result.counts[i]=f.counts[i]*(1-blend)+base.counts[i]*blend;result.ne[i]=blend==0?f.ne[i]:(blend==1?base.ne[i]:std::min(f.ne[i],base.ne[i]));};
 int x=0;
#if defined(__aarch64__)
 if(y>0&&y<511){scalar(0);for(x=1;x<637;x+=4){int i=y*640+x;auto eb=vdupq_n_f32(0),ed=eb;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){auto e=vld2q_f32(reinterpret_cast<const float*>(energy.data()+i+yy*640+xx));eb=vaddq_f32(eb,e.val[0]);ed=vaddq_f32(ed,e.val[1]);}eb=vdivq_f32(eb,vdupq_n_f32(9));ed=vdivq_f32(ed,vdupq_n_f32(9));auto ratio=vsqrtq_f32(vdivq_f32(ed,vmaxq_f32(eb,vdupq_n_f32(1e-6f))));auto blend=vminq_f32(vdupq_n_f32(1),vmaxq_f32(vdupq_n_f32(0),vdivq_f32(vsubq_f32(vdupq_n_f32(.98f),ratio),vdupq_n_f32(.08f))));blend=vbslq_f32(vcgtq_f32(eb,vdupq_n_f32(9*sigma*sigma)),blend,vdupq_n_f32(0));auto b=vld1q_f32(base.counts.data()+i),c=vld1q_f32(f.counts.data()+i);vst1q_f32(result.counts.data()+i,vaddq_f32(vmulq_f32(c,vsubq_f32(vdupq_n_f32(1),blend)),vmulq_f32(b,blend)));auto bn=vld1q_f32(base.ne.data()+i),cn=vld1q_f32(f.ne.data()+i);auto ne=vbslq_f32(vceqq_f32(blend,vdupq_n_f32(0)),cn,vbslq_f32(vceqq_f32(blend,vdupq_n_f32(1)),bn,vminq_f32(bn,cn)));vst1q_f32(result.ne.data()+i,ne);}}
#endif
 for(;x<640;x++)scalar(x);}
 return result;}
}
