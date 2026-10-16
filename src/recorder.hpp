#pragma once
#include <QThread>
#include <QTimer>
#include <QProcess>
#include <QImage>
#include <QElapsedTimer>
#include <QPointer>
#include <QFile>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>

// Only rendered RGBA images cross this interface. Sensor RAW is never queued
// or written. Encoding owns its event loop so GUI paints cannot starve pipe I/O.
class Recorder {
 struct Job {
  std::mutex mutex;QImage latest;QElapsedTimer clock;
  std::atomic<bool> stopping{false};std::atomic<uint64_t> frames{0};
  QProcess*process=nullptr;QTimer*timer=nullptr;QString path,errorTail;bool overrun=false,forced=false;
 };
 QObject*owner;QPointer<QObject> worker;QThread thread;std::shared_ptr<Job> job;
 static bool pump(const std::shared_ptr<Job>&j){
  QImage image;{std::lock_guard<std::mutex> lock(j->mutex);image=j->latest;}
  if(image.isNull()||!j->clock.isValid())return true;
  const uint64_t target=j->clock.elapsed()/40;uint64_t emitted=j->frames.load();
  if(target>emitted+25){j->overrun=true;return false;}
  const qint64 frameBytes=image.sizeInBytes();int count=0;
  while(emitted<target&&count<8&&j->process->bytesToWrite()+frameBytes<=8*frameBytes){
   if(j->process->write(reinterpret_cast<const char*>(image.constBits()),frameBytes)!=frameBytes){j->overrun=true;return false;}
   j->frames=++emitted;count++;
  }return true;
 }
 static void finish(const std::shared_ptr<Job>&j,QObject*w){
  j->timer->stop();pump(j);j->process->closeWriteChannel();
  QTimer::singleShot(5000,w,[j]{if(j->process->state()!=QProcess::NotRunning){j->forced=true;j->process->kill();}});
 }
public:
 std::function<void(bool,const QString&,const QString&,uint64_t)> completed;
 explicit Recorder(QObject*o):owner(o){}
 ~Recorder(){shutdown();}
 void shutdown(){
  stop();if(thread.wait(5000))return;
  // An unresponsive encoder must not keep the windowless application alive.
  if(worker)QMetaObject::invokeMethod(worker,[j=job,t=&thread]{
   if(j&&j->process&&j->process->state()!=QProcess::NotRunning){j->forced=true;j->process->kill();j->process->waitForFinished(1500);}t->quit();
  });
  if(!thread.wait(2500)){thread.quit();if(!thread.wait(1000))qFatal("Recording worker failed to shut down");}
 }
 bool start(const QString&path){
  if(thread.isRunning()&&!thread.wait(100))return false;
  QFile reservation(path);if(!reservation.open(QIODevice::WriteOnly|QIODevice::NewOnly))return false;reservation.close();
  job=std::make_shared<Job>();job->path=path;worker=new QObject;worker->moveToThread(&thread);
  QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
  const auto j=job;auto done=completed;QPointer<QObject> guard(owner);QThread*t=&thread;
  QObject::connect(&thread,&QThread::started,worker,[j,w=worker.data(),t,guard,done]{
   j->process=new QProcess(w);j->timer=new QTimer(w);j->timer->setTimerType(Qt::PreciseTimer);j->timer->setInterval(40);
   auto report=[j,t,guard,done](bool ok,QString error){
    const QString path=j->path;const uint64_t frames=j->frames.load();
    j->stopping=true;
    {std::lock_guard<std::mutex> lock(j->mutex);j->latest=QImage();}
    if(!ok&&QFile::exists(path)&&!QFile::remove(path))error+="；无法清理失败录像："+path;
    if(guard)QMetaObject::invokeMethod(guard,[done,ok,path,error,frames]{if(done)done(ok,path,error,frames);},Qt::QueuedConnection);
    t->quit();
   };
   QObject::connect(j->process,&QProcess::started,w,[j]{j->clock.start();if(j->stopping)j->process->closeWriteChannel();else j->timer->start();});
   QObject::connect(j->timer,&QTimer::timeout,w,[j,w]{if(!pump(j)){j->stopping=true;finish(j,w);}});
   j->process->setStandardOutputFile(QProcess::nullDevice());
   QObject::connect(j->process,&QProcess::readyReadStandardError,w,[j]{j->errorTail=(j->errorTail+QString::fromUtf8(j->process->readAllStandardError())).right(512);});
   QObject::connect(j->process,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),w,[j,report](int code,QProcess::ExitStatus status){j->timer->stop();QString error=(j->errorTail+QString::fromUtf8(j->process->readAllStandardError())).right(512);if(j->overrun)error="编码过慢，录像已提前结束";if(j->forced)error="编码器结束超时，已关闭";bool ok=code==0&&status==QProcess::NormalExit&&j->frames>0;if(!ok&&error.isEmpty())error=j->frames==0?"没有收到可编码的画面":"编码器异常结束";report(ok,error);});
   QObject::connect(j->process,&QProcess::errorOccurred,w,[report](QProcess::ProcessError e){if(e==QProcess::FailedToStart)report(false,"无法启动 MP4 编码器");});
   j->process->start("ffmpeg",{"-hide_banner","-loglevel","error","-y","-f","rawvideo","-pixel_format","rgba","-video_size","640x512","-framerate","25","-i","pipe:0","-an","-c:v","libx264","-preset","ultrafast","-crf","24","-pix_fmt","yuv420p","-threads","1","-movflags","+faststart",j->path});
  });thread.start();return true;
 }
 void submit(const QImage&image){auto j=job;if(!j||j->stopping)return;std::lock_guard<std::mutex> lock(j->mutex);if(!j->stopping)j->latest=image;}
 void stop(){auto j=job;if(!j||!thread.isRunning()||j->stopping.exchange(true))return;QMetaObject::invokeMethod(worker,[j,w=worker.data()]{finish(j,w);});}
 bool wait(int milliseconds){return thread.wait(milliseconds);}
 uint64_t frames()const{return job?job->frames.load():0;}
 size_t retainedImageBytes()const{auto j=job;if(!j)return 0;std::lock_guard<std::mutex> lock(j->mutex);return j->latest.sizeInBytes();}
};
