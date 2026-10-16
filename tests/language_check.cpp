#include "language_dialog.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <cassert>
#include <iostream>
using namespace itl;
int main(int argc,char**argv){
 QApplication app(argc,argv);QTemporaryDir dir;assert(dir.isValid());
 for(auto code:{QString("zh"),QString("en")}){
  QString path=dir.filePath(code+".ini");QSettings settings(path,QSettings::IniFormat);bool sawDialog=false;
  QTimer::singleShot(30,&app,[&]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="initialLanguage"){sawDialog=true;w->findChild<QPushButton*>("language-"+code)->click();}});
  assert(selectInitialLanguage(settings)&&sawDialog);assert(english==(code=="en"));
  QSettings reopened(path,QSettings::IniFormat);assert(reopened.value("ui/language").toString()==code);
  // A subsequent launch must return immediately, without constructing a picker.
  assert(selectInitialLanguage(reopened));assert(english==(code=="en"));
 }
 QSettings canceled(dir.filePath("cancel.ini"),QSettings::IniFormat);
 QTimer::singleShot(30,&app,[]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="initialLanguage")static_cast<QDialog*>(w)->reject();});
 assert(!selectInitialLanguage(canceled));assert(!canceled.contains("ui/language"));
 english=true;assert(L("最高／最低温十字与温度")=="Max/Min Markers");assert(L("快门校正：机芯已确认")=="FFC: confirmed by camera");assert(L("照片保存失败：disk full")=="Photo save failed: disk full");assert(paletteNames().first()=="White Hot");
 english=false;assert(paletteNames().first()==QString::fromUtf8("白热"));
 std::cout<<"First-run zh/en, disk persistence, subsequent launch, cancellation and translations passed\n";
}
