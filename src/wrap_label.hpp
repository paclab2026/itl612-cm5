#pragma once
#include <QLabel>
#include <QTextLayout>
#include <QPainter>
#include <QStyleOption>
// Long filenames and OS errors must wrap without increasing the window width.
class WrapLabel:public QLabel {
 QSizeF layoutText(qreal width,QPainter*p=nullptr)const{
  QTextLayout layout(QString(text()).replace('\n',QChar(0x2028)),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);layout.beginLayout();qreal height=0,maxWidth=0;
  while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(std::max(qreal(1),width));line.setPosition({0,height});height+=line.height();maxWidth=std::max(maxWidth,line.naturalTextWidth());}layout.endLayout();if(p)layout.draw(p,contentsRect().topLeft()+QPoint(margin(),margin()));return {maxWidth,height};
 }
public:
 explicit WrapLabel(const QString&text={},QWidget*parent=nullptr):QLabel(text,parent){setWordWrap(true);setTextFormat(Qt::PlainText);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);}
 QSizeF textSize(qreal width)const{return layoutText(width);}
 bool hasHeightForWidth()const override{return true;}
 int heightForWidth(int width)const override{return qCeil(layoutText(width-contentsMargins().left()-contentsMargins().right()-2*margin()).height())+contentsMargins().top()+contentsMargins().bottom()+2*margin();}
 QSize sizeHint()const override{return {320,heightForWidth(320)};}
 QSize minimumSizeHint()const override{return {0,fontMetrics().height()+contentsMargins().top()+contentsMargins().bottom()+2*margin()};}
 void paintEvent(QPaintEvent*)override{QPainter p(this);QStyleOption option;option.initFrom(this);style()->drawPrimitive(QStyle::PE_Widget,&option,&p,this);p.setClipRect(contentsRect());p.setPen(palette().color(QPalette::WindowText));layoutText(contentsRect().width()-2*margin(),&p);}
};
