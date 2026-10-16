#include "mars.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace itl {
void Parser::feed(const uint8_t* p,size_t n,const std::function<void(std::shared_ptr<Frame>)>& output){
 if(n>4*FRAME)throw std::runtime_error("USB chunk exceeds parser bound");
 if(head && (head>=FRAME||head*2>=bytes.size())){bytes.erase(bytes.begin(),bytes.begin()+head);head=0;}
 bytes.insert(bytes.end(),p,p+n);
 while(true){
  if(!locked){
   if(bytes.size()-head<FRAME+2)break;
   while(head+1<bytes.size()&&!(bytes[head]==0xaa&&bytes[head+1]==0x55))head++;
   if(bytes.size()-head<FRAME+2)break;
   if(bytes[head+FRAME]!=0xaa||bytes[head+FRAME+1]!=0x55){head++;continue;}locked=true;
  }
  if(bytes.size()-head<FRAME)break;
  if(bytes[head]!=0xaa||bytes[head+1]!=0x55){locked=false;syncLoss++;continue;}
  auto f=std::make_shared<Frame>();f->index=++seq;f->time=now();std::memcpy(f->param.data(),bytes.data()+head,ROW);
  for(size_t y=0;y<H;y++){auto s=bytes.data()+head+ROW+y*ROW;std::memcpy(f->raw.data()+y*W,s,W*2);std::memcpy(f->yuv.data()+y*W*2,s+W*2,W*2);}
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  for(auto& v:f->raw)v=__builtin_bswap16(v);
#endif
  head+=FRAME;output(f);
 }
}
Camera::~Camera(){stop();}
bool Camera::start(){
 stop();{std::lock_guard<std::mutex> lock(mutex);error.clear();command.clear();queue.clear();}shutter=false;devicePalette=-1;
 if(libusb_init(&ctx)<0){error="libusb initialization failed";return false;}
 handle=libusb_open_device_with_vid_pid(ctx,0x04b4,0xf7f7);
 if(!handle){error="Mars 未打开：检查连接或 udev 权限";return false;}
 libusb_config_descriptor* cfg=nullptr;bool ep81=false,ep02=false,ep83=false;
 if(libusb_get_active_config_descriptor(libusb_get_device(handle),&cfg)==0){for(int i=0;i<cfg->bNumInterfaces;i++)for(int a=0;a<cfg->interface[i].num_altsetting;a++){auto& d=cfg->interface[i].altsetting[a];if(d.bInterfaceNumber!=0||d.bAlternateSetting!=0)continue;for(int e=0;e<d.bNumEndpoints;e++){auto& ep=d.endpoint[e];if((ep.bmAttributes&3)!=LIBUSB_TRANSFER_TYPE_BULK)continue;ep81|=ep.bEndpointAddress==0x81;ep02|=ep.bEndpointAddress==0x02;ep83|=ep.bEndpointAddress==0x83;}}libusb_free_config_descriptor(cfg);}
 if(!ep81||!ep02||!ep83){error="Mars bulk endpoint layout mismatch";return false;}
 libusb_set_auto_detach_kernel_driver(handle,1);int r=libusb_claim_interface(handle,0);
 if(r<0){error=libusb_error_name(r);return false;}quit=false;thread=std::thread(&Camera::run,this);return true;
}
void Camera::stop(){quit=true;if(thread.joinable())thread.join();if(handle){libusb_release_interface(handle,0);libusb_close(handle);handle=nullptr;}if(ctx){libusb_exit(ctx);ctx=nullptr;}}
void Camera::run(){Parser parser;std::vector<uint8_t> buffer(1024*1024);while(!quit){
 auto send=[&](unsigned char*cmd,const char*name){unsigned char reply[512];int n=0;int r=libusb_bulk_transfer(handle,2,cmd,12,&n,400);bool sent=r==0&&n==12;bool received=false;if(sent){r=libusb_bulk_transfer(handle,0x83,reply,512,&n,400);received=r==0&&n>=6&&reply[0]==0x55&&reply[1]==0xaa&&reply[2]==1&&reply[3]==0&&reply[4]==1&&reply[5]==0xf0;}
 std::lock_guard<std::mutex> lock(mutex);command=std::string(name)+(received?"：机芯已确认":"：未收到成功确认");};
 if(shutter.exchange(false)){unsigned char cmd[12]={0x55,0xaa,7,2,1,8,0,0,0,1,0x0d,0xf0};send(cmd,"快门校正");}
 int choice=devicePalette.exchange(-1);if(choice>=0){unsigned char cmd[12]={0x55,0xaa,7,2,0,4,0,0,0,(unsigned char)choice,(unsigned char)(choice^1),0xf0};send(cmd,"机芯伪彩");}
 int n=0;int r=libusb_bulk_transfer(handle,0x81,buffer.data(),buffer.size(),&n,300);
 if(n>0){bytes+=n;parser.feed(buffer.data(),n,[&](auto f){frames++;std::lock_guard<std::mutex> lock(mutex);if(queue.size()==8){queue.pop_front();dropped++;}queue.push_back(f);});syncLoss=parser.syncLoss;}
 if(r==LIBUSB_ERROR_TIMEOUT){timeouts++;continue;}if(r!=0){std::lock_guard<std::mutex> lock(mutex);error=libusb_error_name(r);break;}
 }}
std::vector<std::shared_ptr<Frame>> Camera::take(){std::lock_guard<std::mutex> lock(mutex);std::vector<std::shared_ptr<Frame>> out(queue.begin(),queue.end());queue.clear();return out;}
std::string Camera::status(){std::lock_guard<std::mutex> lock(mutex);return error;}
Background background(const uint16_t*c,const uint16_t*h){std::array<float,64>d,a;std::array<float,16>centers,mads;std::array<unsigned,16>quad;int n=0;
 for(int gy=0;gy<4;gy++)for(int gx=0;gx<4;gx++){for(int y=0;y<8;y++)for(int x=0;x<8;x++){int i=(36+gy*140+y*2)*640+40+gx*176+x*2;d[y*8+x]=float(c[i])-float(h[i]);}std::sort(d.begin(),d.end());float mid=d[32];for(int i=0;i<64;i++)a[i]=std::abs(d[i]-mid);std::sort(a.begin(),a.end());if(a[32]<=3){centers[n]=mid;mads[n]=a[32];quad[n]=(gy/2)*2+gx/2;n++;}}
 if(n<8)return {};std::copy_n(centers.begin(),n,d.begin());std::sort(d.begin(),d.begin()+n);float mid=d[n/2];int count=0;unsigned bits=0;
 for(int i=0;i<n;i++)if(std::abs(centers[i]-mid)<=2){d[count]=centers[i];a[count]=mads[i];bits|=1u<<quad[i];count++;}
 if(count<8||__builtin_popcount(bits)<3)return {};std::sort(d.begin(),d.begin()+count);std::sort(a.begin(),a.begin()+count);return {d[count/2],float(count)/16,std::clamp(a[count/2]*1.4826f/std::sqrt(2.f),.5f,4.f)};
}
}
