#pragma once
#include "palettes.hpp"
#include "i18n.hpp"
#include "wrap_label.hpp"
#include "recorder.hpp"
#include <QStackedWidget>
#include <QGridLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QDialog>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSaveFile>
#include <QDir>
#include <QListWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QImageReader>
#include <QPixmapCache>
#ifdef ITL_ENABLE_DIAGNOSTICS
#include "layout_check.hpp"
#endif

struct Skin {
 QString bg,panel,ink,muted,line,accent,tint;
 static Skin get(int n){
  if(n==1)return {"#141618","#212427","#e1e4e6","#a4acb1","#373d42","#d2d7db","#373d42"};
  if(n==2)return {"#0e0d09","#19130c","#FF8C00","#bd7428","#62401d","#FF8C00","#30200d"};
  return {"#edf0f2","#ffffff","#19252c","#5f6e77","#dbe2e6","#086e69","#e2f1ed"};
 }
};
class TemperatureRail:public QWidget {
public:
 int skin=2,palette=0;bool valid=false,center=true,scale=true;double hot=0,cold=0,cursor=0;
 TemperatureRail(){setFixedWidth(98);}
 void paintEvent(QPaintEvent*)override{
  Skin s=Skin::get(skin);QPainter p(this);p.fillRect(rect(),QColor(s.bg));p.setPen(QColor(s.line));p.drawLine(0,0,0,height());
  p.setPen(QColor(s.ink));QFont number=font();number.setPixelSize(21);if(skin==2)number.setFamily("DejaVu Sans Mono");p.setFont(number);
  auto text=[&](double t){return valid?QString::number(t,'f',1):QString("—");};
  p.drawText(QRect(2,4,94,30),Qt::AlignCenter,text(hot));p.drawText(QRect(2,height()-34,94,30),Qt::AlignCenter,text(cold));
  if(scale){QLinearGradient g(0,height()-38,0,38);auto lut=itl::paletteLUT(palette);for(int i=0;i<256;i+=1){auto c=lut[i];g.setColorAt(i/255.,QColor(c&255,(c>>8)&255,(c>>16)&255));}p.fillRect(QRect(10,38,22,std::max(1,height()-76)),g);}
  if(center){int y=height()/2;QFont f=font();f.setPixelSize(13);p.setFont(f);p.setPen(QColor(s.muted));p.drawText(QRect(35,y-26,59,24),Qt::AlignRight|Qt::AlignVCenter,L("中心 °C"));number.setPixelSize(19);p.setFont(number);p.setPen(QColor(s.ink));auto value=text(cursor);QFontMetricsF metrics(number);if(metrics.horizontalAdvance(value)>59){number.setPixelSize(std::max(13,int(19*59/metrics.horizontalAdvance(value))));p.setFont(number);}p.drawText(QRect(35,y,59,27),Qt::AlignRight|Qt::AlignVCenter,value);}
 }
};
class PaletteDialog:public QDialog {
public:
 Skin colors;
 PaletteDialog(QWidget*parent,int skin):QDialog(parent,Qt::Dialog|Qt::FramelessWindowHint),colors(Skin::get(skin)){
  setAttribute(Qt::WA_TranslucentBackground);setObjectName("palettePopup");setStyleSheet("#palettePopup{background:transparent;}");
 }
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QColor fill(colors.panel);fill.setAlpha(232);p.setBrush(fill);p.setPen(QColor(colors.line));p.drawRoundedRect(rect().adjusted(1,1,-1,-1),5,5);}
};
class AppWindow:public QWidget {
 Camera&cam;QSettings settings{"ITL612","CM5"};View*view;QStackedWidget*stack;
 QWidget*home;QWidget*menuPage;QVBoxLayout*menuLayout;TemperatureRail*rail;
 QLabel*topState;QLabel*diag;QLabel*notice;QPushButton*modeButton;QPushButton*paletteButton;QPushButton*lockButton;QPushButton*photoButton;QPushButton*recordButton;QPushButton*shutterButton;QPushButton*filesButton;QPushButton*menuButton;
 QTimer renderTimer,uiTimer,noticeTimer,photoTimer;Recorder recorder{this};QElapsedTimer recordingClock;
 QProcess player;bool closing=false,playerStopping=false;
 QString album,videoPath,lastFile;QString commandText;bool photoPending=false,finishing=false;
 bool center=true,extrema=false,showScale=true;int skin=2;double fps=0,ms=0,lastFrameTime=0;uint64_t delivered=0;
 QJsonArray auditResults;
 QPushButton*button(QString text,QString name=""){auto b=new QPushButton(text);b->setMinimumHeight(44);b->setMinimumWidth(80);b->setObjectName(name);return b;}
 QString fileName(QString suffix){QString stem=QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"),path=QDir(album).filePath(stem+suffix);for(int i=2;QFile::exists(path);i++)path=QDir(album).filePath(stem+"-"+QString::number(i)+suffix);return path;}
 bool hasFrame()const{return lastFrameTime>0&&itl::now()-lastFrameTime<2&&cam.status().empty();}
 void message(QString text){text=L(text);QMetaObject::invokeMethod(this,[this,text]{if(closing)return;notice->setText(text);notice->setVisible(!text.isEmpty());noticeTimer.stop();if(!text.contains(L("失败"),Qt::CaseInsensitive))noticeTimer.start(5000);},Qt::QueuedConnection);}
 void clearMenu(){while(auto item=menuLayout->takeAt(0)){if(item->widget()){item->widget()->hide();item->widget()->deleteLater();}delete item;}}
 void persist(){settings.setValue("ui/language",english?"en":"zh");settings.setValue("mode",view->motion?0:1);settings.setValue("temporal",view->observations==8?0:view->observations==4?1:2);settings.setValue("palette",view->paletteMode);settings.setValue("ui/skin",skin);settings.setValue("ui/locked",view->locked);settings.setValue("ui/autogate",view->autogate);settings.setValue("ui/original",view->original);settings.setValue("ui/center",center);settings.setValue("ui/extrema",extrema);settings.setValue("ui/scale",showScale);settings.sync();}
 void applySkin(){auto s=Skin::get(skin);int radius=skin==2?3:10;
  setStyleSheet(QString("QWidget{background:%1;color:%3;font-family:'Noto Sans CJK SC';font-size:18px;} QLabel#secondary{color:%4;font-size:14px;} QLabel#brand{font-size:21px;} QLabel#status{font-size:15px;} QPushButton,QToolButton,QComboBox{background:%2;border:1px solid %5;border-radius:%8px;padding:8px;min-height:28px;} QPushButton:checked,QToolButton:checked{background:%7;color:%6;border-color:%6;} QPushButton#primary{background:%7;color:%6;border:1px solid %6;} QPushButton#primary:disabled{color:%4;background:%1;border-color:%5;} QPushButton:disabled,QComboBox:disabled{color:%4;background:%1;} QPushButton:focus,QToolButton:focus,QComboBox:focus{border:2px solid %6;} QCheckBox{spacing:12px;padding:12px 0;} QCheckBox::indicator{width:24px;height:24px;} QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%7;} QListWidget{background:%1;border:0;} QListWidget::item{padding:12px;border-bottom:1px solid %5;}")
   .arg(s.bg,s.panel,s.ink,s.muted,s.line,s.accent,s.tint).arg(radius));rail->skin=skin;rail->update();
 }
 void refresh(){
  lockButton->setText(L("锁窗"));shutterButton->setText(L("校正"));photoButton->setText(L("◎ 拍照"));filesButton->setText(L("文件"));menuButton->setText(L("菜单"));
  QString processing=view->original?L("机芯原图 · 增强旁路"):QString("%1 · Autogate %2").arg(view->motion?L("运动"):L("常规"),view->autogate?L("开"):L("关"));
  topState->setText(processing+QString(L(" · %1 帧%2\n%3 · %4 fps")).arg(view->observations).arg(view->locked?L(" · 锁窗"):"").arg(hasFrame()?L("USB 已连接"):L("等待机芯")).arg(fps,0,'f',1));
  modeButton->setText(view->original?L("机芯原图 ▾"):QString("Autogate %1 ▾").arg(view->autogate?L("开"):L("关")));paletteButton->setText(paletteNames()[view->paletteMode]+" ▾");lockButton->setChecked(view->locked);
  if(!hasFrame())rail->valid=false;rail->palette=view->original?0:view->paletteMode;rail->center=center;rail->scale=showScale;rail->update();view->centerMarker=center;view->extremaMarkers=extrema;view->syncOverlay();
  shutterButton->setEnabled(hasFrame());paletteButton->setEnabled(!view->original);lockButton->setEnabled(!view->original);photoButton->setEnabled(hasFrame()&&!photoPending&&!finishing);recordButton->setEnabled((hasFrame()||view->recording)&&!finishing);recordButton->setText(finishing?L("保存中…"):view->recording?QString(L("停止 %1:%2")).arg(recordingClock.elapsed()/60000,2,10,QChar('0')).arg(recordingClock.elapsed()/1000%60,2,10,QChar('0')):L("○ 录像"));
 }
 void menuHeader(QString title,bool root=false){auto row=new QWidget;auto h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);auto t=new QLabel(title);QFont f=t->font();f.setPixelSize(27);t->setFont(f);h->addWidget(t,1);auto back=button(root?L("关闭"):L("返回"));h->addWidget(back);connect(back,&QPushButton::clicked,this,[this,root]{if(root)goHome();else goMenu();});menuLayout->addWidget(row);}
 void note(QString text){auto l=new WrapLabel(text);l->setObjectName("secondary");l->setWordWrap(true);l->setTextFormat(Qt::PlainText);l->setMinimumWidth(0);menuLayout->addWidget(l);}
 QComboBox*choice(QString label,QStringList items,int current,std::function<void(int)> changed){auto row=new QWidget;auto h=new QHBoxLayout(row);h->setContentsMargins(0,8,0,8);h->addWidget(new QLabel(label),1);auto c=new QComboBox;c->addItems(items);c->setCurrentIndex(current);int itemWidth=0;for(const auto&item:items)itemWidth=std::max(itemWidth,c->fontMetrics().horizontalAdvance(item));c->setMinimumWidth(std::max(205,itemWidth+48));h->addWidget(c);menuLayout->addWidget(row);connect(c,&QComboBox::currentIndexChanged,this,[this,changed](int n){changed(n);view->pendingReset=true;persist();refresh();});return c;}
 QCheckBox*toggle(QString label,bool checked,std::function<void(bool)> changed){auto c=new QCheckBox(label);c->setChecked(checked);menuLayout->addWidget(c);connect(c,&QCheckBox::toggled,this,[this,changed](bool n){changed(n);view->pendingReset=true;persist();refresh();});return c;}
 void goHome(){stack->setCurrentWidget(home);view->pendingReset=true;refresh();}
 void goMenu(){clearMenu();menuHeader(L("菜单"),true);
  for(auto pair:QList<QPair<QString,QString>>{{L("显示"),L("画面来源 · 伪色")},{L("实时增强"),L("Autogate · 运动 · 多帧降噪")},{L("测温"),L("中心 · 冷热点 · 温度色带")},{L("拍摄与文件"),L("PNG · MP4 · 保存目录")},{L("设备"),L("连接 · 快门 · 机芯伪彩")},{L("皮肤"),L("白色 · 黑灰 · 琥珀仪器")}}){auto b=button(pair.first+"\n"+pair.second);b->setMinimumHeight(60);b->setStyleSheet("font-size:16px;padding:5px 8px;");connect(b,&QPushButton::clicked,this,[this,name=pair.first]{goPage(name);});menuLayout->addWidget(b);}auto lang=button("语言/lang\n中文 / ENG","languageMenu");lang->setMinimumHeight(60);lang->setStyleSheet("font-size:16px;padding:5px 8px;");connect(lang,&QPushButton::clicked,this,[this]{goPage("language");});menuLayout->addWidget(lang);auto exit=button(L("退出应用"),"quitApplication");connect(exit,&QPushButton::clicked,this,&QWidget::close);menuLayout->addWidget(exit);menuLayout->addStretch();stack->setCurrentWidget(menuPage);}
 void goPage(QString name){clearMenu();menuHeader(name=="language"?QString::fromUtf8("语言/lang"):name);stack->setCurrentWidget(menuPage);
  if(name=="language"){
   for(auto code:{QString("zh"),QString("en")}){auto b=button(code=="zh"?QString::fromUtf8("中文"):QString("ENG"),"setLanguage-"+code);b->setMinimumHeight(70);b->setCheckable(true);b->setChecked(english==(code=="en"));connect(b,&QPushButton::clicked,this,[this,code]{english=code=="en";notice->clear();notice->hide();noticeTimer.stop();persist();refresh();goPage("language");});menuLayout->addWidget(b);}
   note(english?"Language is saved on this device.":QString::fromUtf8("语言选择保存在本机。"));
  }else if(name==L("显示")){
   auto source=choice(L("画面来源"),{L("RAW 显示"),L("机芯原图（含 OSD）")},view->original?1:0,[this](int n){view->original=n==1;});
   auto p=choice(L("本地伪色"),paletteNames(),view->paletteMode,[this](int n){view->paletteMode=n;});p->setEnabled(!view->original);connect(source,&QComboBox::currentIndexChanged,p,[p](int n){p->setEnabled(n==0);});
   note(L("本地伪色用于预览、照片与录像，不改变机芯温度读数。机芯原图保留设备 OSD。"));
  }else if(name==L("实时增强")){
   auto ag=toggle(QString(L("Autogate 自适应映射：%1")).arg(view->autogate?L("开"):L("关")),view->autogate,[this](bool n){view->autogate=n;});ag->setObjectName("autogateToggle");connect(ag,&QCheckBox::toggled,ag,[ag](bool n){ag->setText(QString(L("Autogate 自适应映射：%1")).arg(n?L("开"):L("关")));});
   auto mode=choice(L("降噪模式"),{L("运动自适应"),L("常规多帧")},view->motion?0:1,[this](int n){view->motion=n==0;});mode->setObjectName("denoiseMode");mode->setEnabled(!view->original);ag->setEnabled(!view->original);
   auto temporal=choice(L("多帧降噪"),{L("单帧"),L("4 帧"),L("8 帧")},view->observations==1?0:view->observations==4?1:2,[this](int n){view->observations=n==0?1:n==1?4:8;});temporal->setEnabled(!view->original);
   auto locked=toggle(L("锁定显示窗口"),view->locked,[this](bool n){view->locked=n;});locked->setEnabled(!view->original);
   auto reset=button(L("重新适应当前画面"));reset->setEnabled(!view->original);connect(reset,&QPushButton::clicked,this,[this]{view->pendingReset=true;message(L("正在重新适应当前画面"));});menuLayout->addWidget(reset);
   note(L("Autogate 关：使用线性显示窗口，降噪仍按所选模式运行。机芯原图旁路本地增强。"));
  }else if(name==L("测温")){
   toggle(L("中心温度与位置"),center,[this](bool n){center=n;});toggle(L("最高／最低温十字与温度"),extrema,[this](bool n){extrema=n;})->setObjectName("extremaToggle");toggle(L("温度色带"),showScale,[this](bool n){showScale=n;});
   note(L("读数来自机芯参数行。右侧色条显示所选伪色；自适应显示不是线性温度标尺。测温标记也适用于机芯原图。区域估温暂未启用。"));
  }else if(name==L("拍摄与文件")){
   note(L("照片：PNG，640×512\n录像：H.264 MP4，640×512，25 fps\n不保存 RAW。"));note(L("保存目录：\n")+album);
   auto files=button(L("查看照片与录像"));connect(files,&QPushButton::clicked,this,[this]{goFiles();});menuLayout->addWidget(files);note(L("独立拍照增强暂未启用。"));
  }else if(name==L("设备")){
   auto reconnect=button(L("重新连接机芯"));connect(reconnect,&QPushButton::clicked,this,[this]{reconnectCamera();});menuLayout->addWidget(reconnect);
   auto shutter=button(L("快门校正"));shutter->setEnabled(hasFrame());connect(shutter,&QPushButton::clicked,this,[this]{cam.requestShutter();view->pendingReset=true;message(L("等待机芯确认快门校正"));});menuLayout->addWidget(shutter);
   auto devicePalette=new QComboBox;for(int i=0;i<10;i++)devicePalette->addItem(QString(L("模式 %1")).arg(i));menuLayout->addWidget(devicePalette);auto send=button(L("应用机芯伪彩"));send->setEnabled(hasFrame());connect(send,&QPushButton::clicked,this,[this,devicePalette]{cam.requestPalette(devicePalette->currentIndex());message(L("等待机芯确认伪彩命令"));});menuLayout->addWidget(send);
   auto info=new WrapLabel;info->setWordWrap(true);info->setObjectName("secondary");info->setText(diag->text()+"\n"+L(QString::fromStdString(cam.commandStatus())));menuLayout->addWidget(info);
   auto exit=button(L("退出应用"));connect(exit,&QPushButton::clicked,this,&QWidget::close);menuLayout->addWidget(exit);
  }else if(name==L("皮肤")){
   QStringList names={L("白色"),L("黑灰"),L("琥珀仪器")};for(int i=0;i<3;i++){auto b=button(names[i]+(skin==i?"  ✓":""));b->setMinimumHeight(95);connect(b,&QPushButton::clicked,this,[this,i]{skin=i;applySkin();persist();goPage(L("皮肤"));});menuLayout->addWidget(b);}note(L("主题立即应用。伪色与温度读数独立于界面皮肤。"));auto preview=button(L("查看主界面"));connect(preview,&QPushButton::clicked,this,[this]{goHome();});menuLayout->addWidget(preview);
  }
  menuLayout->addStretch();
 }
 void goFiles(){clearMenu();menuHeader(L("照片与录像"));stack->setCurrentWidget(menuPage);auto files=QDir(album).entryInfoList({"*.png","*.mp4"},QDir::Files,QDir::Time);auto list=new QListWidget;for(auto f:files){auto item=new QListWidgetItem(f.fileName(),list);item->setData(Qt::UserRole,f.absoluteFilePath());}menuLayout->addWidget(list,1);if(files.empty())note(L("暂无照片或录像"));connect(list,&QListWidget::itemActivated,this,[this](QListWidgetItem*i){QString path=i->data(Qt::UserRole).toString();if(path.endsWith(".png"))showPhoto(path);else playVideo(path);});note(L("双击文件或选中后按回车打开。"));}
 void showPhoto(const QString&path){QDialog d(this);d.setWindowTitle(L("照片"));auto l=new QVBoxLayout(&d);auto image=new QLabel;QImageReader reader(path);auto size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(640,512,Qt::KeepAspectRatio));auto decoded=reader.read();if(decoded.isNull())image->setText(L("无法打开照片：")+reader.errorString());else image->setPixmap(QPixmap::fromImage(decoded));l->addWidget(image);auto close=button(L("关闭"));l->addWidget(close);connect(close,&QPushButton::clicked,&d,&QDialog::accept);d.exec();}
 void playVideo(const QString&path){stopPlayer();player.start("ffplay",{"-hide_banner","-loglevel","error","-nostats","-autoexit","-fs",path});}
 void stopPlayer(){if(player.state()==QProcess::NotRunning)return;playerStopping=true;player.terminate();if(!player.waitForFinished(300)){player.kill();player.waitForFinished(1500);}playerStopping=false;}
 void palettePopup(){if(view->original)return;PaletteDialog d(this,skin);d.resize(530,310);auto l=new QVBoxLayout(&d);l->setContentsMargins(18,14,18,14);auto h=new QHBoxLayout;h->addWidget(new QLabel(L("伪色")),1);auto close=button(L("关闭"));h->addWidget(close);connect(close,&QPushButton::clicked,&d,&QDialog::reject);l->addLayout(h);auto grid=new QGridLayout;
  for(int i=0;i<9;i++){auto b=new QToolButton;b->setText(paletteNames()[i]);b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setCheckable(true);b->setChecked(view->paletteMode==i);b->setMinimumSize(148,65);QPixmap swatch(130,14);QPainter p(&swatch);QLinearGradient g(0,0,130,0);auto lut=itl::paletteLUT(i);for(int j=0;j<256;j++){auto c=lut[j];g.setColorAt(j/255.,QColor(c&255,(c>>8)&255,(c>>16)&255));}p.fillRect(swatch.rect(),g);p.end();b->setIcon(QIcon(swatch));b->setIconSize({130,14});connect(b,&QToolButton::clicked,&d,[this,&d,i]{view->paletteMode=i;view->pendingReset=true;persist();refresh();d.accept();});grid->addWidget(b,i/3,i%3);}l->addLayout(grid);d.move(mapToGlobal(QPoint((width()-d.width())/2,240)));d.exec();}
 bool storageReady(){QStorageInfo volume(album);if(!volume.isValid()||volume.bytesAvailable()<200*1024*1024){message(L("图片目录所在磁盘剩余空间不足 200 MB"));return false;}return true;}
 void takePhoto(){if(!hasFrame()||!storageReady()||photoPending)return;photoPending=true;view->wantImage=true;refresh();photoTimer.start(2000);}
 void finishVideo(){if(!view->recording)return;view->recording=false;finishing=true;recorder.stop();message(L("正在完成 MP4 保存"));refresh();}
 void startVideo(){if(view->recording){finishVideo();return;}if(finishing||!hasFrame()||!storageReady())return;videoPath=fileName(".mp4");delivered=0;
  if(!recorder.start(videoPath)){message(L("录像编码器尚未就绪"));return;}view->recording=true;recordingClock.start();refresh();}
 void reconnectCamera(){if(view->recording)finishVideo();cam.stop();cam.start();rail->valid=false;lastFrameTime=0;view->pendingReset=true;message(cam.status().empty()?L("正在重新连接机芯"):L(QString::fromStdString(cam.status())));refresh();}
public:
 AppWindow(Camera&c):cam(c){
  player.setStandardOutputFile(QProcess::nullDevice());player.setStandardErrorFile(QProcess::nullDevice());
  noticeTimer.setSingleShot(true);
  photoTimer.setSingleShot(true);connect(&photoTimer,&QTimer::timeout,this,[this]{if(photoPending){photoPending=false;view->wantImage=false;message(L("没有收到可保存的画面"));refresh();}});
  setWindowTitle("iTL612 · CM5");resize(720,720);
  connect(&player,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(!closing&&e==QProcess::FailedToStart)message(L("无法打开录像播放器"));});
  album=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);if(album.isEmpty())album=QDir::home().filePath("Pictures");album=QDir(album).filePath("thermal");QDir().mkpath(album);
  view=new View(cam);view->motion=settings.value("mode",0).toInt()==0;int n=settings.value("temporal",1).toInt();view->observations=n==0?8:n==1?4:1;view->paletteMode=std::clamp(settings.value("palette",0).toInt(),0,8);skin=std::clamp(settings.value("ui/skin",2).toInt(),0,2);view->locked=settings.value("ui/locked",false).toBool();view->autogate=settings.value("ui/autogate",true).toBool();view->original=settings.value("ui/original",false).toBool();center=settings.value("ui/center",true).toBool();extrema=settings.value("ui/extrema",false).toBool();showScale=settings.value("ui/scale",true).toBool();
  auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);stack=new QStackedWidget;outer->addWidget(stack,1);notice=new WrapLabel;notice->setWordWrap(true);notice->setObjectName("secondary");notice->setContentsMargins(12,5,12,5);notice->hide();connect(&noticeTimer,&QTimer::timeout,notice,&QWidget::hide);outer->addWidget(notice);diag=new QLabel(this);diag->hide();view->status=diag;
  home=new QWidget;auto hl=new QVBoxLayout(home);hl->setContentsMargins(0,0,0,0);hl->setSpacing(0);auto header=new QWidget;header->setFixedHeight(52);auto hh=new QHBoxLayout(header);hh->setContentsMargins(16,3,16,3);auto brand=new QLabel("ITL612 / IR  CM5");brand->setObjectName("brand");hh->addWidget(brand,1);topState=new QLabel;topState->setAlignment(Qt::AlignRight|Qt::AlignVCenter);topState->setObjectName("status");hh->addWidget(topState);hl->addWidget(header);
  auto middle=new QWidget;auto ml=new QHBoxLayout(middle);ml->setContentsMargins(0,0,0,0);ml->setSpacing(0);auto surface=view;surface->setMinimumSize(320,256);ml->addWidget(surface,1);rail=new TemperatureRail;ml->addWidget(rail);hl->addWidget(middle,1);
  auto footer=new QWidget;footer->setFixedHeight(140);auto fl=new QVBoxLayout(footer);fl->setContentsMargins(14,10,14,12);fl->setSpacing(9);auto quick=new QHBoxLayout;modeButton=button(L("运动 · 4 帧"));paletteButton=button(L("白热 ▾"));lockButton=button(L("锁窗"));lockButton->setCheckable(true);auto shutter=button(L("校正"));shutterButton=shutter;quick->addWidget(modeButton,16);quick->addWidget(paletteButton,10);quick->addWidget(lockButton,10);quick->addWidget(shutter,10);fl->addLayout(quick);
  auto actions=new QHBoxLayout;photoButton=button(L("◎ 拍照"),"primary");recordButton=button(L("○ 录像"));auto files=button(L("文件"));filesButton=files;auto menu=button(L("菜单"));menuButton=menu;for(auto b:{photoButton,recordButton,files,menu}){b->setMinimumHeight(52);actions->addWidget(b,1);}fl->addLayout(actions);hl->addWidget(footer);stack->addWidget(home);
  menuPage=new QWidget;menuLayout=new QVBoxLayout(menuPage);menuLayout->setContentsMargins(20,14,20,14);menuLayout->setSpacing(8);stack->addWidget(menuPage);
  connect(modeButton,&QPushButton::clicked,this,[this]{goPage(L("实时增强"));});connect(paletteButton,&QPushButton::clicked,this,[this]{palettePopup();});connect(lockButton,&QPushButton::toggled,this,[this](bool n){view->locked=n;view->pendingReset=true;persist();refresh();});connect(shutter,&QPushButton::clicked,this,[this]{if(hasFrame()){cam.requestShutter();view->pendingReset=true;message(L("等待机芯确认快门校正"));}});connect(photoButton,&QPushButton::clicked,this,[this]{takePhoto();});connect(recordButton,&QPushButton::clicked,this,[this]{startVideo();});connect(files,&QPushButton::clicked,this,[this]{goFiles();});connect(menu,&QPushButton::clicked,this,[this]{goMenu();});
  view->measurementsReady=[this](const std::optional<Measurements>&m,double time){rail->valid=m.has_value();if(m){rail->hot=m->hot.celsius;rail->cold=m->cold.celsius;rail->cursor=m->cursor.celsius;}lastFrameTime=time;rail->update();};
  view->infoReady=[this](auto f,double fPS,double mS){if(fPS>=0)fps=fPS;if(mS>=0)ms=mS;diag->setText(QString(L("显示 %1 fps · 处理 %2 ms\nUSB %3 帧 · 丢队列 %4\n%5")).arg(fps,0,'f',1).arg(ms,0,'f',1).arg(cam.frames.load()).arg(cam.dropped.load()).arg(L(QString::fromStdString(cam.status()))));if(f){auto m=Measurements::decode(f->param.data(),f->param.size());rail->valid=m.has_value();if(m){rail->hot=m->hot.celsius;rail->cold=m->cold.celsius;rail->cursor=m->cursor.celsius;}lastFrameTime=f->time;}refresh();};
  view->imageReady=[this](const QImage&image){if(view->recording)recorder.submit(image);if(photoPending){QString path=fileName(".png");QSaveFile file(path);bool ok=file.open(QIODevice::WriteOnly)&&image.save(&file,"PNG")&&file.commit();photoPending=false;photoTimer.stop();if(ok){lastFile=path;message(L("照片已保存：")+QFileInfo(path).fileName());}else message(L("照片保存失败：")+file.errorString());refresh();}};
  recorder.completed=[this](bool ok,const QString&path,const QString&error,uint64_t frames){if(closing)return;view->recording=false;finishing=false;delivered=frames;if(ok){lastFile=path;message(L("录像已保存：")+QFileInfo(path).fileName()+(error.isEmpty()?QString():"；"+error));}else message(L("录像保存失败：")+error);refresh();};
  connect(&renderTimer,&QTimer::timeout,this,[this]{view->render();});renderTimer.start(10);connect(&uiTimer,&QTimer::timeout,this,[this]{refresh();QString cmd=L(QString::fromStdString(cam.commandStatus()));if(!cmd.isEmpty()&&cmd!=commandText){commandText=cmd;message(cmd);}if(view->recording&&!hasFrame())finishVideo();});uiTimer.start(250);
  auto escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);connect(escape,&QShortcut::activated,this,[this]{if(stack->currentWidget()!=home)goHome();else close();});applySkin();refresh();
 }
 void shutdown(){if(closing)return;closing=true;renderTimer.stop();uiTimer.stop();noticeTimer.stop();photoTimer.stop();photoPending=false;view->wantImage=false;view->recording=false;recorder.shutdown();stopPlayer();cam.stop();persist();}
 ~AppWindow(){shutdown();}
 void closeEvent(QCloseEvent*e)override{QElapsedTimer elapsed;elapsed.start();shutdown();std::cout<<"shutdownMS="<<elapsed.elapsed()<<std::endl;e->accept();QCoreApplication::quit();}

#ifdef ITL_ENABLE_DIAGNOSTICS
#include "ui_audit.inc"
#else
 void startAudit(const QStringList&){}
#endif
};
