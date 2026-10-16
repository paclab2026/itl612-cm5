#pragma once
#include "wrap_label.hpp"
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QJsonObject>
inline QJsonArray textLayoutIssues(QWidget*root,int&checked){
 QJsonArray issues;
 for(auto w:root->findChildren<QWidget*>()){
  if(!w->isVisibleTo(root)||w->window()!=root->window())continue;
  QString text;QRect area;int flags=Qt::TextShowMnemonic;bool candidate=false;
  if(auto b=qobject_cast<QAbstractButton*>(w)){
   QStyleOptionButton option;option.initFrom(b);option.text=b->text();
   text=b->text();area=b->style()->subElementRect(qobject_cast<QCheckBox*>(b)?QStyle::SE_CheckBoxContents:QStyle::SE_PushButtonContents,&option,b);candidate=true;
   if(auto tool=qobject_cast<QToolButton*>(b);tool&&tool->toolButtonStyle()==Qt::ToolButtonTextUnderIcon)area.setHeight(area.height()-tool->iconSize().height()-4);
  }else if(auto c=qobject_cast<QComboBox*>(w)){
   QStyleOptionComboBox option;option.initFrom(c);option.editable=false;option.currentText=c->currentText();text=c->currentText();for(int i=0;i<c->count();i++)if(c->fontMetrics().horizontalAdvance(c->itemText(i))>c->fontMetrics().horizontalAdvance(text))text=c->itemText(i);area=c->style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,c);candidate=true;
  }else if(auto l=qobject_cast<QLabel*>(w);l&&l->pixmap().isNull()){
   text=l->text();area=l->contentsRect().adjusted(l->margin(),l->margin(),-l->margin(),-l->margin());flags=l->wordWrap()?Qt::TextWordWrap:0;candidate=true;
  }
  if(!candidate||text.isEmpty())continue;checked++;
  QFontMetricsF fm(w->font());QRectF bound=fm.boundingRect(QRectF(0,0,area.width(),10000),flags,text);if(auto wrapped=dynamic_cast<WrapLabel*>(w))bound=QRectF(QPointF(0,0),wrapped->textSize(area.width()));
  bool geometry=root->rect().contains(QRect(w->mapTo(root,QPoint(0,0)),w->size()));
  if(bound.width()>area.width()+1||bound.height()>area.height()+1||!geometry)issues.append(QJsonObject{{"type",w->metaObject()->className()},{"name",w->objectName()},{"text",text},{"availableW",area.width()},{"availableH",area.height()},{"textW",bound.width()},{"textH",bound.height()},{"geometry",geometry}});
 }
 return issues;
}
