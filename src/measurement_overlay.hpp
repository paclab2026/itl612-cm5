#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <optional>
#include <cstdint>
#include <algorithm>

namespace itl {
struct TemperaturePoint { int x=0,y=0; double celsius=0; };
struct Measurements {
 TemperaturePoint hot,cold,cursor;
 static std::optional<Measurements> decode(const uint8_t* data,size_t size){
  if(size<108||data[0]!=0xaa||data[1]!=0x55)return {};
  auto word=[&](int i){return int16_t(uint16_t(data[2*i])|(uint16_t(data[2*i+1])<<8));};
  auto point=[&](int i){return TemperaturePoint{word(i),word(i+1),word(i+2)/10.};};
  Measurements m{point(44),point(47),point(50)};
  for(auto p:{m.hot,m.cold,m.cursor})if(p.x<0||p.x>=640||p.y<0||p.y>=512||p.celsius<-100||p.celsius>1000)return {};
  if(m.hot.celsius<m.cold.celsius)return {};
  return m;
 }
};
// Keep the overlay and GL viewport on the same letterboxed image rectangle.
inline QRectF thermalImageRect(QSize size,qreal dpr=1){
 int ww=qRound(size.width()*dpr),hh=qRound(size.height()*dpr),w=ww,h=w*512/640;
 if(h>hh){h=hh;w=h*640/512;}
 return QRectF((ww-w)/2/dpr,(hh-h)/2/dpr,w/dpr,h/dpr);
}
struct MarkerLayout {QPointF point;QRectF label;QString text;QColor color;};
inline MarkerLayout markerLayout(const TemperaturePoint&p,const QString&prefix,QColor color,QRectF image,const QFont&font){
 QPointF point(image.left()+(p.x+.5)*image.width()/640,image.top()+(p.y+.5)*image.height()/512);
 QString text=prefix+QString::number(p.celsius,'f',1)+QString::fromUtf8("°C");
 QFontMetricsF fm(font);QSizeF size(fm.horizontalAdvance(text)+12,fm.height()+6);
 qreal x=point.x()+(p.x<320?13:-13-size.width()),y=point.y()-size.height()-11;
 if(y<image.top()+2)y=point.y()+11;
 x=std::clamp(x,image.left()+2,std::max(image.left()+2,image.right()-size.width()-2));
 y=std::clamp(y,image.top()+2,std::max(image.top()+2,image.bottom()-size.height()-2));
 return {point,QRectF(QPointF(x,y),size),text,color};
}
class MeasurementOverlay:public QWidget {
public:
 std::optional<Measurements> measurements;bool center=true,extrema=false,fresh=false;
 explicit MeasurementOverlay(QWidget*parent):QWidget(parent){setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setAutoFillBackground(false);setStyleSheet("background:transparent;");}
 QFont markerFont()const{QFont f("DejaVu Sans Mono");f.setPixelSize(16);f.setBold(true);return f;}
 QList<MarkerLayout> markers()const{
  if(!fresh||!measurements)return {};
  auto image=thermalImageRect(size(),devicePixelRatioF());auto f=markerFont();QList<MarkerLayout> result;
  if(extrema){result.append(markerLayout(measurements->hot,"H ",QColor("#ff694d"),image,f));result.append(markerLayout(measurements->cold,"L ",QColor("#52d9ff"),image,f));
   if(result[0].label.intersects(result[1].label)){auto&r=result[1].label;auto other=result[0].label;auto moved=r.translated(0,other.bottom()+3-r.top());if(image.contains(moved))r=moved;else{moved=r.translated(0,other.top()-3-r.bottom());if(image.contains(moved))r=moved;}}
  }
  return result;
 }
 void paintEvent(QPaintEvent*)override{
  if(!fresh||!measurements)return;
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);auto image=thermalImageRect(size(),devicePixelRatioF());p.setClipRect(image);p.setFont(markerFont());
  auto cross=[&](QPointF at,QColor color){QPainterPath path;path.moveTo(at+QPointF(-9,0));path.lineTo(at+QPointF(-3,0));path.moveTo(at+QPointF(3,0));path.lineTo(at+QPointF(9,0));path.moveTo(at+QPointF(0,-9));path.lineTo(at+QPointF(0,-3));path.moveTo(at+QPointF(0,3));path.lineTo(at+QPointF(0,9));p.setPen(QPen(Qt::black,4));p.drawPath(path);p.setPen(QPen(color,2));p.drawPath(path);};
  if(center){auto c=measurements->cursor;cross({image.left()+(c.x+.5)*image.width()/640,image.top()+(c.y+.5)*image.height()/512},Qt::white);}
  for(auto m:markers()){cross(m.point,m.color);p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,195));p.drawRoundedRect(m.label,3,3);p.setPen(m.color);p.drawText(m.label,Qt::AlignCenter,m.text);}
 }
};
}
