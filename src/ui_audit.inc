 void startAudit(const QStringList&args){view->diagnostics=view->diagnostics||args.contains("--ui-audit")||args.contains("--media-audit")||args.contains("--lifecycle-audit");
  if(args.contains("--layout-audit")){
   const int oldSkin=skin,oldPalette=view->paletteMode;const bool oldEnglish=english,oldOriginal=view->original,oldAuto=view->autogate,oldLocked=view->locked;
   auto results=std::make_shared<QJsonArray>();auto tick=std::make_shared<int>(0);auto timer=new QTimer(this);timer->setInterval(250);
   connect(timer,&QTimer::timeout,this,[=]{int n=(*tick)++;int scene=n%15,theme=(n/15)%3,lang=n/45;
    if(lang>=2){timer->stop();english=oldEnglish;skin=oldSkin;view->paletteMode=oldPalette;view->original=oldOriginal;view->autogate=oldAuto;view->locked=oldLocked;notice->hide();applySkin();persist();goHome();QFile file("/tmp/itl612-layout-audit.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(QJsonObject{{"states",*results}}).toJson());close();return;}
    goPage("language");QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);auto langButton=findChild<QPushButton*>(lang?"setLanguage-en":"setLanguage-zh");langButton->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);skin=theme;applySkin();view->original=false;view->autogate=scene%2;view->locked=scene%2;finishing=false;notice->hide();
    if(scene==0){goHome();}
    else if(scene==1)goMenu();
    else if(scene>=2&&scene<=8){QStringList pages={L("显示"),L("实时增强"),L("测温"),L("拍摄与文件"),L("设备"),L("皮肤"),"language"};goPage(pages[scene-2]);}
    else if(scene==9)goFiles();
    else if(scene==10){view->original=true;goHome();}
    else if(scene==11){finishing=true;goHome();notice->setText(english?"Video save failed: Storage device is not available. Please reconnect the storage device and try again.":QString::fromUtf8("录像保存失败：存储设备不可用，请重新连接存储设备后重试。"));notice->show();}
    else if(scene==12){view->autogate=false;view->locked=true;goHome();}
    else if(scene==13){goHome();recordButton->setText(QString(L("停止 %1:%2")).arg(999).arg(59));}
    else {goHome();notice->setText(L("照片已保存：")+QString(100,'W')+".png");notice->show();}
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();int checked=0;auto issues=textLayoutIssues(this,checked);bool languageSaved=settings.value("ui/language").toString()==(lang?"en":"zh");results->append(QJsonObject{{"language",lang?"en":"zh"},{"skin",theme},{"scene",scene},{"checked",checked},{"languageSaved",languageSaved},{"screenFits",width()==720&&height()==720},{"width",width()},{"height",height()},{"issues",issues}});
    if(scene==0||scene==1||scene==3||!issues.empty())grab().save(QString("/tmp/itl612-layout-%1-%2-%3.png").arg(lang).arg(theme).arg(scene));
    if(scene==0){QTimer::singleShot(30,this,[]{for(auto w:QApplication::topLevelWidgets())if(auto popup=dynamic_cast<PaletteDialog*>(w)){int count=0;auto issues=textLayoutIssues(popup,count);std::cout<<"paletteLayout checked="<<count<<" issues="<<QJsonDocument(issues).toJson(QJsonDocument::Compact).toStdString()<<std::endl;popup->reject();}});palettePopup();}
   });timer->start();return;
  }
  if(args.contains("--language-media-audit")){
   auto oldLanguage=english;auto oldAlbum=album;album=QDir::temp().filePath("itl612-language-media-"+QString::number(QCoreApplication::applicationPid()));QDir().mkpath(album);auto report=std::make_shared<QJsonObject>();
   QTimer::singleShot(2500,this,[this]{takePhoto();});QTimer::singleShot(3200,this,[this]{startVideo();});
   QTimer::singleShot(4500,this,[this,oldLanguage,report]{goPage("language");QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);auto b=findChild<QPushButton*>(oldLanguage?"setLanguage-zh":"setLanguage-en");b->click();report->insert("recordingAfterSwitch",view->recording);report->insert("languageChanged",english!=oldLanguage);report->insert("persisted",settings.value("ui/language").toString()==(english?"en":"zh"));goHome();});
   QTimer::singleShot(6700,this,[this]{finishVideo();});QTimer::singleShot(9500,this,[this,oldLanguage,oldAlbum,report]{report->insert("finished",!finishing&&!view->recording);report->insert("videoFrames",qint64(delivered));report->insert("retainedBytes",qint64(recorder.retainedImageBytes()));report->insert("directory",album);report->insert("fresh",hasFrame());english=oldLanguage;persist();QFile file("/tmp/itl612-language-media.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(*report).toJson());album=oldAlbum;close();});return;
  }
  if(args.contains("--controls-audit")){
   const int oldSkin=skin;const bool oldAuto=view->autogate,oldMotion=view->motion,oldOriginal=view->original,oldCenter=center,oldExtrema=extrema;
   auto results=std::make_shared<QJsonArray>();auto step=std::make_shared<int>(0);auto timer=new QTimer(this);timer->setInterval(700);
   connect(timer,&QTimer::timeout,this,[=]{int tick=(*step)++;if(tick<4)return;int index=(tick-4)/2;bool check=(tick-4)%2;int theme=index/8;bool original=index%8>=4,motion=index%4>=2,ag=index%2;
    if(index==24){if(!check){cam.stop();lastFrameTime=0;refresh();}return;}
    if(index==25)return;
    if(index==26){if(check){refresh();view->syncOverlay();results->append(QJsonObject{{"disconnected",true},{"markersHidden",view->overlay->markers().empty()},{"railInvalid",!rail->valid}});}return;}
    if(index>=27){timer->stop();skin=oldSkin;view->autogate=oldAuto;view->motion=oldMotion;view->original=oldOriginal;center=oldCenter;extrema=oldExtrema;applySkin();persist();goHome();QFile file("/tmp/itl612-controls-audit.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(QJsonObject{{"checks",*results}}).toJson());close();return;}
    if(!check){skin=theme;applySkin();view->original=false;goPage(L("实时增强"));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);auto toggle=findChild<QCheckBox*>("autogateToggle");if(toggle->isChecked()!=ag)toggle->click();auto mode=findChild<QComboBox*>("denoiseMode");mode->setCurrentIndex(motion?0:1);
     goPage(L("测温"));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);auto temp=findChild<QCheckBox*>("extremaToggle");if(!temp->isChecked())temp->click();center=index%2;view->original=original;persist();goHome();return;}
    auto markers=view->overlay->markers();auto image=thermalImageRect(view->overlay->size(),view->overlay->devicePixelRatioF());bool labelsFit=true;for(auto m:markers)labelsFit=labelsFit&&image.contains(m.label)&&image.contains(m.point);
    bool status=original?topState->text().contains(L("增强旁路"))&&modeButton->text().contains(L("机芯原图")):topState->text().contains(ag?L("Autogate 开"):L("Autogate 关"))&&modeButton->text().contains(ag?L("Autogate 开"):L("Autogate 关"));
    goPage(L("实时增强"));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);auto toggle=findChild<QCheckBox*>("autogateToggle");auto mode=findChild<QComboBox*>("denoiseMode");bool menuFits=true;for(auto w:menuPage->findChildren<QWidget*>())if(w->isVisibleTo(menuPage)&&!menuPage->rect().contains(QRect(w->mapTo(menuPage,QPoint(0,0)),w->size())))menuFits=false;bool controls=toggle->isChecked()==ag&&toggle->text().endsWith(ag?L("开"):L("关"))&&toggle->isEnabled()==!original&&mode->isEnabled()==!original;stack->setCurrentWidget(home);refresh();
    QJsonObject item{{"skin",theme},{"original",original},{"motion",motion},{"autogate",ag},{"fresh",hasFrame()},{"validMeasurements",view->overlay->measurements.has_value()},{"markers",markers.size()},{"markerBinding",markers.size()==2},{"labelsFit",labelsFit},{"statusMatches",status},{"controlsMatch",controls},{"menuFits",menuFits},{"persisted",settings.value("ui/autogate").toBool()==ag},{"pipelineMatches",view->pipelineMatches()},{"fps",fps}};results->append(item);std::cout<<"controlsAudit "<<QJsonDocument(item).toJson(QJsonDocument::Compact).toStdString()<<std::endl;
    if(!original&&motion)grab().save(QString("/tmp/itl612-controls-skin-%1-%2.png").arg(theme).arg(ag?"on":"off"));
   });timer->start();return;
  }
  if(args.contains("--lifecycle-audit")){
   const QString oldAlbum=album;album=QDir(album).filePath(".audit-memory-"+QString::number(QCoreApplication::applicationPid()));QDir().mkpath(album);
   const int oldSkin=skin,oldPalette=view->paletteMode,oldFrames=view->observations;const bool oldMotion=view->motion,oldOriginal=view->original,oldAuto=view->autogate;
   auto samples=std::make_shared<QJsonArray>();auto produced=std::make_shared<QStringList>();auto tick=std::make_shared<int>(0);auto lastPhoto=std::make_shared<QString>();
   auto timer=new QTimer(this);timer->setInterval(1000);
   auto sample=[this,samples](int cycle){QFile status("/proc/self/status");status.open(QIODevice::ReadOnly);auto text=status.readAll();status.close();int rss=0;for(auto line:text.split('\n'))if(line.startsWith("VmRSS:"))rss=line.simplified().split(' ')[1].toInt();QJsonObject result{{"cycle",cycle},{"rssKiB",rss},{"widgets",findChildren<QWidget*>().size()},{"topLevels",QApplication::topLevelWidgets().size()},{"fds",QDir("/proc/self/fd").entryList(QDir::AllEntries|QDir::NoDotAndDotDot).size()},{"recordImageBytes",qint64(recorder.retainedImageBytes())},{"playerBytes",player.bytesAvailable()},{"recording",view->recording},{"finishing",finishing},{"healthy",view->healthy()},{"freshPreview",hasFrame()}};samples->append(result);std::cout<<"lifecycleSample "<<QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString()<<std::endl;};
   connect(timer,&QTimer::timeout,this,[=]{int step=(*tick)++;if(step<10){if(step==9)sample(-1);return;}int cycle=(step-10)/7,phase=(step-10)%7;
    if(cycle>=12){timer->stop();stopPlayer();skin=oldSkin;view->paletteMode=oldPalette;view->observations=oldFrames;view->motion=oldMotion;view->original=oldOriginal;view->autogate=oldAuto;persist();QFile report("/tmp/itl612-lifecycle-audit.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"samples",*samples},{"testDirectory",album},{"createdFiles",QJsonArray::fromStringList(QDir(album).entryList(QDir::Files|QDir::NoDotAndDotDot))}}).toJson());album=oldAlbum;close();return;}
    if(phase==0){skin=cycle%3;applySkin();view->paletteMode=cycle%9;view->observations=cycle%3==0?1:cycle%3==1?4:8;view->motion=cycle%2==0;view->original=cycle%4==3;view->autogate=true;view->pendingReset=true;goHome();}
    if(phase==1){for(QString page:{L("显示"),L("实时增强"),L("测温"),L("拍摄与文件"),L("设备"),L("皮肤")})goPage(page);goFiles();goHome();takePhoto();QTimer::singleShot(150,this,[this]{takePhoto();});}
    if(phase==2){if(lastFile.endsWith(".png")){*lastPhoto=lastFile;produced->append(lastFile);}startVideo();}
    if(phase==3){goFiles();finishVideo();if(!videoPath.isEmpty())produced->append(videoPath);}
    if(phase==4){auto dismiss=[this]{for(auto w:QApplication::topLevelWidgets())if(auto d=qobject_cast<QDialog*>(w);d&&d->parentWidget()==this)d->reject();};if(!lastPhoto->isEmpty()){QTimer::singleShot(100,this,dismiss);showPhoto(*lastPhoto);}QTimer::singleShot(100,this,dismiss);view->original=false;palettePopup();goHome();}
    if(phase==5&&!finishing&&lastFile.endsWith(".mp4"))playVideo(lastFile);
    if(phase==6){stopPlayer();goFiles();goHome();sample(cycle);}
   });timer->start();return;
  }
  for(const auto&arg:args)if(arg.startsWith("--exit-audit=")){
   const QString test=arg.mid(13);
   QTimer::singleShot(2500,this,[this,test]{
    if(test=="recording"||test=="saving"||test=="immediate"||test=="stalled"||test=="encoder-failure")startVideo();
    if(test=="playback"){auto files=QDir(album).entryInfoList({"*.mp4"},QDir::Files,QDir::Time);for(auto f:files)if(f.size()>1024){player.start("ffplay",{"-autoexit","-loop","0","-fs",f.absoluteFilePath()});break;}}
    if(test=="disconnected")cam.stop();
    QTimer::singleShot(test=="immediate"?0:3000,this,[this,test]{
     if(test=="saving")finishVideo();
     if(test=="stalled")QProcess::execute("pkill",{"-STOP","-P",QString::number(QCoreApplication::applicationPid()),"ffmpeg"});
     goMenu();auto exit=findChild<QPushButton*>("quitApplication");std::cout<<"exitAudit case="<<test.toStdString()<<" recording="<<view->recording<<" finishing="<<finishing<<" playerRunning="<<(player.state()==QProcess::Running)<<" path="<<videoPath.toStdString()<<std::endl;
     if(test=="popup"){QTimer::singleShot(100,exit,&QPushButton::click);palettePopup();}else exit->click();
    });
   });return;
  }
if(!args.contains("--ui-audit")&&!args.contains("--media-audit"))return;
  QTimer::singleShot(2500,this,[this,args]{
   bool media=args.contains("--media-audit");if(media){view->motion=true;view->observations=4;view->autogate=true;view->pendingReset=true;takePhoto();QTimer::singleShot(700,this,[this]{startVideo();});QTimer::singleShot(3700,this,[this]{finishVideo();});QTimer::singleShot(6000,this,[this]{persist();goHome();std::cout<<"mediaAudit elapsedMS="<<recordingClock.elapsed()<<" lastFile="<<lastFile.toStdString()<<" frames="<<delivered<<std::endl;});return;}
   int savedSkin=skin,savedPalette=view->paletteMode,savedFrames=view->observations;bool savedAuto=view->autogate,savedMotion=view->motion,savedOriginal=view->original;
   for(int n=0;n<3;n++){skin=n;applySkin();for(QString page:{L("菜单"),L("显示"),L("实时增强"),L("测温"),L("拍摄与文件"),L("设备"),L("皮肤")}){if(page==L("菜单"))goMenu();else goPage(page);QApplication::processEvents();bool fits=true;for(auto w:menuPage->findChildren<QWidget*>()){if(w->isVisibleTo(menuPage)&&!menuPage->rect().contains(QRect(w->mapTo(menuPage,QPoint(0,0)),w->size())))fits=false;}auditResults.append(QJsonObject{{"skin",n},{"page",page},{"fits",fits}});}}
   for(int n=0;n<9;n++){view->paletteMode=n;refresh();auditResults.append(QJsonObject{{"palette",n},{"railMatches",rail->palette==n}});}
   goPage(L("显示"));QApplication::processEvents();auto displays=menuPage->findChildren<QComboBox*>();if(displays.size()>=2){displays[0]->setCurrentIndex(1);auditResults.append(QJsonObject{{"originalView",view->original},{"paletteDisabled",!displays[1]->isEnabled()}});displays[0]->setCurrentIndex(0);}
   goPage(L("实时增强"));QApplication::processEvents();auto options=menuPage->findChildren<QComboBox*>();if(options.size()>=2){for(int n=0;n<3;n++){options[1]->setCurrentIndex(n);auditResults.append(QJsonObject{{"frameOption",n},{"frameBinding",view->observations==(n==0?1:n==1?4:8)}});}options[0]->setCurrentIndex(0);auditResults.append(QJsonObject{{"motionBinding",view->motion}});options[0]->setCurrentIndex(1);auditResults.append(QJsonObject{{"autogateModeBinding",!view->motion}});}
   goPage(L("实时增强"));auto checks=menuPage->findChildren<QCheckBox*>();if(!checks.empty()){checks.first()->setChecked(false);QApplication::processEvents();auditResults.append(QJsonObject{{"autogateOff",!view->autogate}});}
   view->original=false;goHome();for(int n=0;n<9;n++){QTimer::singleShot(100,this,[n]{for(auto widget:QApplication::topLevelWidgets())if(auto popup=dynamic_cast<PaletteDialog*>(widget)){for(auto b:popup->findChildren<QToolButton*>())if(b->text()==paletteNames()[n]){b->click();return;}}});palettePopup();auditResults.append(QJsonObject{{"popupPalette",n},{"selection",view->paletteMode==n},{"persisted",settings.value("palette").toInt()==n},{"railMatches",rail->palette==n}});}
   skin=savedSkin;view->paletteMode=savedPalette;view->observations=savedFrames;view->autogate=savedAuto;view->motion=savedMotion;view->original=savedOriginal;applySkin();persist();goHome();QFile file("/tmp/itl612-ui-audit.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(QJsonObject{{"screenWidth",width()},{"screenHeight",height()},{"checks",auditResults}}).toJson());cam.requestShutter();QTimer::singleShot(1500,this,[this]{std::cout<<"commandAudit "<<cam.commandStatus()<<std::endl;});std::cout<<"uiAudit completed"<<std::endl;
  });
 }
