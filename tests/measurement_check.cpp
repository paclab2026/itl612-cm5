#include "measurement_overlay.hpp"
#include <QApplication>
#include <QImage>
#include <array>
#include <cassert>
#include <iostream>
using namespace itl;
int main(int argc,char**argv){
 QApplication app(argc,argv);std::array<uint8_t,108> data{};
 auto set=[&](int i,int v){data[i*2]=v&255;data[i*2+1]=(v>>8)&255;};
 data[0]=0xaa;data[1]=0x55;set(44,639);set(45,511);set(46,421);set(47,0);set(48,0);set(49,-123);set(50,320);set(51,256);set(52,201);
 auto m=Measurements::decode(data.data(),data.size());assert(m&&m->hot.celsius==42.1&&m->cold.celsius==-12.3);
 assert(!Measurements::decode(data.data(),107));data[0]=0;assert(!Measurements::decode(data.data(),108));data[0]=0xaa;
 for(auto v:{-1,640,32767}){set(44,v);assert(!Measurements::decode(data.data(),108));}set(44,639);
 set(48,512);assert(!Measurements::decode(data.data(),108));set(48,0);set(46,-124);assert(!Measurements::decode(data.data(),108));set(46,421);
 set(52,10001);assert(!Measurements::decode(data.data(),108));set(52,201);
 MeasurementOverlay overlay(nullptr);overlay.measurements=m;overlay.extrema=true;overlay.fresh=true;int layouts=0;
 for(auto size:{QSize(622,528),QSize(720,720),QSize(320,256)}){
  overlay.resize(size);auto image=thermalImageRect(size);
  for(auto point:{TemperaturePoint{0,0,-100},TemperaturePoint{639,0,1000},TemperaturePoint{0,511,0},TemperaturePoint{639,511,42.1},TemperaturePoint{320,256,23}}){
   auto layout=markerLayout(point,"H ",Qt::red,image,overlay.markerFont());assert(image.contains(layout.label));assert(image.contains(layout.point));layouts++;
  }
  overlay.measurements=Measurements{{320,256,42},{320,256,20},{320,256,30}};auto marks=overlay.markers();assert(marks.size()==2&&!marks[0].label.intersects(marks[1].label));
 }
 overlay.resize(622,528);overlay.measurements=m;
 auto painted=[&]{QImage image(overlay.size(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);overlay.render(&image);int pixels=0;for(int y=0;y<image.height();y++)for(int x=0;x<image.width();x++)pixels+=qAlpha(image.pixel(x,y))>0;return pixels;};
 assert(overlay.markers().size()==2);assert(painted()>300);overlay.measurements.reset();assert(overlay.markers().empty()&&painted()==0);overlay.measurements=m;
 overlay.fresh=false;assert(overlay.markers().empty()&&painted()==0);overlay.fresh=true;overlay.extrema=false;overlay.center=false;assert(painted()==0);
 std::cout<<"measurement decode/range rejection, "<<layouts<<" edge layouts, collision, visible markers, invalid/stale/off suppression passed\n";
}
