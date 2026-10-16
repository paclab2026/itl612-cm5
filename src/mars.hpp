#pragma once
#include <array>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <deque>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <libusb.h>
namespace itl {
constexpr size_t W=640,H=512,N=W*H,ROW=2560,FRAME=513*ROW;
inline double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Frame {std::array<uint16_t,N> raw;std::array<uint8_t,N*2> yuv;std::array<uint8_t,ROW> param;uint64_t index=0;double time=0;};
class Parser {
 std::vector<uint8_t> bytes;size_t head=0;bool locked=false;uint64_t seq=0;
 public:uint64_t syncLoss=0;
 void feed(const uint8_t* p,size_t n,const std::function<void(std::shared_ptr<Frame>)>& output);
 size_t pending()const{return bytes.size()-head;}
};
class Camera {
 libusb_context* ctx=nullptr;libusb_device_handle* handle=nullptr;std::thread thread;
 std::mutex mutex;std::deque<std::shared_ptr<Frame>> queue;std::atomic<bool> quit{false},shutter{false};
 std::string error;void run();
 std::atomic<int> devicePalette{-1};std::string command;
 public:std::atomic<uint64_t> frames{0},bytes{0},dropped{0},timeouts{0},syncLoss{0};
 ~Camera();bool start();void stop();void requestShutter(){shutter=true;}
 void requestPalette(int index){if(index>=0&&index<=9)devicePalette=index;}
 std::string commandStatus(){std::lock_guard<std::mutex> lock(mutex);return command;}
 bool hasPending(){std::lock_guard<std::mutex> lock(mutex);return !queue.empty();}
 std::vector<std::shared_ptr<Frame>> take();std::string status();
};
struct Background {float offset=0,confidence=0,sigma=.5;};
Background background(const uint16_t* current,const uint16_t* history);
}
