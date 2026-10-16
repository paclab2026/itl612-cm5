#include "mars.hpp"
#include <fstream>
#include <iostream>
#include <cassert>
#include <cstring>
#include <algorithm>
using namespace itl;
int main(int argc,char**argv){assert(argc==2);std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>s((std::istreambuf_iterator<char>(in)),{});assert(!s.empty());Parser p;std::vector<std::shared_ptr<Frame>> out;size_t at=0;
 while(at<s.size()){size_t n=std::min(size_t((at*17)%65537+1),s.size()-at);p.feed(s.data()+at,n,[&](auto f){out.push_back(f);});at+=n;assert(p.pending()<=FRAME+65537);}
 assert(out.size()>=2);for(auto&f:out){assert(f->param[0]==0xaa&&f->param[1]==0x55);size_t source=0;bool found=false;for(;source+FRAME<=s.size();source++)if(s[source]==0xaa&&s[source+1]==0x55&&std::equal(f->param.begin(),f->param.end(),s.begin()+source)){bool same=true;for(size_t y=0;y<H;y++){same&=std::memcmp(f->raw.data()+y*W,s.data()+source+ROW+y*ROW,W*2)==0;same&=std::memcmp(f->yuv.data()+y*W*2,s.data()+source+ROW+y*ROW+W*2,W*2)==0;}if(same){found=true;break;}}assert(found);}
 // Garbage, a split marker and a false candidate must not consume a valid header.
 std::vector<uint8_t> synthetic(FRAME*3+13,0);synthetic[0]=0xaa;synthetic[1]=0x55;for(int k=0;k<3;k++){size_t b=13+k*FRAME;synthetic[b]=0xaa;synthetic[b+1]=0x55;synthetic[b+ROW]=uint8_t(9+k);}
 Parser q;int count=0;for(size_t i=0;i<synthetic.size();i+=997)q.feed(synthetic.data()+i,std::min(size_t(997),synthetic.size()-i),[&](auto f){assert(f->raw[0]==9+count);count++;});assert(count==3);
 auto b=background(out[0]->raw.data(),out[0]->raw.data());assert(b.offset==0&&b.confidence==1&&b.sigma==.5f);
 std::cout<<"parser RAW/UYVY byte parity, fragmented stream and false-header checks passed: "<<out.size()<<" existing frames\n";
}
