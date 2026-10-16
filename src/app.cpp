#include "pipeline.hpp"
#include "measurement_overlay.hpp"
#include <QPainterPath>
#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QShortcut>
#include <QScreen>
#include <QSettings>
#include <QFile>
#include <QImage>
#include <QDateTime>
#include <functional>
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace itl;
class View:public QOpenGLWidget {
 std::unique_ptr<Pipeline> pipe;GLuint drawProgram=0,vao=0,originalTexture=0;Camera&cam;bool failed=false;double statsTime=now();uint64_t statsFrames=0,shown=0;std::shared_ptr<Frame>latest;int lastWidth=0,lastHeight=0;double thermalReadTime=0,cpuTemperature=0;
 GLuint shader(GLenum kind,const char*s){GLuint b=glCreateShader(kind);glShaderSource(b,1,&s,nullptr);glCompileShader(b);GLint ok;glGetShaderiv(b,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(b,sizeof(log),nullptr,log);throw std::runtime_error(std::string("display shader failed: ")+log);}return b;}
 public:bool diagnostics=std::getenv("ITL_DIAGNOSTICS")!=nullptr;MeasurementOverlay*overlay=nullptr;QLabel*status=nullptr;std::function<void(const QImage&)> imageReady;std::function<void(const std::optional<Measurements>&,double)> measurementsReady;std::function<void(const std::shared_ptr<Frame>&,double,double)> infoReady;bool wantImage=false,recording=false,autogate=true,original=false,centerMarker=true,extremaMarkers=false;bool motion=true,locked=false;int observations=4,paletteMode=0;bool pendingReset=false;
 bool healthy()const{return !failed;}
 View(Camera&c):cam(c){overlay=new MeasurementOverlay(this);QSurfaceFormat f;f.setRenderableType(QSurfaceFormat::OpenGLES);f.setVersion(3,1);f.setSwapInterval(1);setFormat(f);setUpdateBehavior(QOpenGLWidget::PartialUpdate);}
 ~View(){if(context()){makeCurrent();glUseProgram(0);glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);for(unsigned i=0;i<8;i++)glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,0);pipe.reset();glDeleteProgram(drawProgram);glDeleteVertexArrays(1,&vao);if(originalTexture)glDeleteTextures(1,&originalTexture);glFinish();doneCurrent();}}
 void resizeEvent(QResizeEvent*e)override{QOpenGLWidget::resizeEvent(e);overlay->setGeometry(rect());}
 void syncOverlay(){bool fresh=latest&&now()-latest->time<2&&cam.status().empty();bool changed=overlay->center!=centerMarker||overlay->extrema!=extremaMarkers||overlay->fresh!=fresh;overlay->center=centerMarker;overlay->extrema=extremaMarkers;overlay->fresh=fresh;if(changed)overlay->update();}
 bool pipelineMatches()const{return pipe&&!pendingReset&&pipe->autogate==autogate&&pipe->motion==motion;}
 void render(){syncOverlay();if(failed)return;if(isVisible()){if(cam.hasPending()||pendingReset)update();}else if((recording||wantImage)&&context()){makeCurrent();paintGL();doneCurrent();}else{auto frames=cam.take();if(!frames.empty()&&infoReady)infoReady(frames.back(),-1,-1);statsTime=now();statsFrames=shown;}}
 void paintGL()override{if(failed)return;double currentTime=now();if(currentTime-thermalReadTime>=1){QFile thermal("/sys/class/thermal/thermal_zone0/temp");if(thermal.open(QIODevice::ReadOnly)){bool valid=false;double t=thermal.readAll().trimmed().toDouble(&valid)/1000;if(valid&&t>0&&t<150){cpuTemperature=t;}}thermalReadTime=currentTime;}auto fs=cam.take();if(fs.empty()&&pipe&&!wantImage&&!pendingReset&&width()==lastWidth&&height()==lastHeight){auto error=cam.status();if(!error.empty()&&status)status->setText(QString::fromStdString(error));return;}
 try {if(!pipe){pipe=std::make_unique<Pipeline>();const char*vs="#version 310 es\nprecision highp float;out vec2 uv;void main(){vec2 p=vec2((gl_VertexID&1)*2-1,(gl_VertexID>>1)*2-1);gl_Position=vec4(p,0,1);uv=vec2((p.x+1.0)*.5,(1.0-p.y)*.5);}";
 const char*fs="#version 310 es\nprecision highp float;uniform sampler2D tex;in vec2 uv;out vec4 color;void main(){color=texture(tex,uv);}";GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);drawProgram=glCreateProgram();glAttachShader(drawProgram,v);glAttachShader(drawProgram,f);glLinkProgram(drawProgram);glDeleteShader(v);glDeleteShader(f);GLint ok;glGetProgramiv(drawProgram,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("display program link failed");glGenVertexArrays(1,&vao);}
 if(pendingReset){pipe->reset();pendingReset=false;}if(pipe->observations!=observations){pipe->reset();}pipe->autogate=autogate;pipe->motion=motion;pipe->locked=locked;pipe->observations=observations;pipe->paletteMode=paletteMode;
 QImage sourceImage;
 if(!fs.empty()){latest=fs.back();overlay->measurements=Measurements::decode(latest->param.data(),latest->param.size());if(measurementsReady)measurementsReady(overlay->measurements,latest->time);syncOverlay();if(centerMarker||extremaMarkers)overlay->update();if(!original)pipe->process(latest);shown++;}
 if(original&&latest){sourceImage=QImage(W,H,QImage::Format_RGBA8888);for(size_t i=0;i<N;i+=2){int u=int(latest->yuv[i*2])-128,v=int(latest->yuv[i*2+2])-128;for(int k=0;k<2;k++){int y=latest->yuv[(i+k)*2+1];auto dst=sourceImage.bits()+(i+k)*4;dst[0]=std::clamp(int(y+1.402*v),0,255);dst[1]=std::clamp(int(y-.344136*u-.714136*v),0,255);dst[2]=std::clamp(int(y+1.772*u),0,255);dst[3]=255;}}if(!originalTexture){glGenTextures(1,&originalTexture);glBindTexture(GL_TEXTURE_2D,originalTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,sourceImage.constBits());}else{glBindTexture(GL_TEXTURE_2D,originalTexture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,sourceImage.constBits());}}
 if(!fs.empty()&&latest&&(wantImage||recording)&&imageReady){if(original)imageReady(sourceImage);else{QImage image(W,H,QImage::Format_RGBA8888);pipe->readPixels(image.bits());imageReady(image);}wantImage=false;}
 lastWidth=width();lastHeight=height();glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());int ww=width()*devicePixelRatio(),hh=height()*devicePixelRatio();glViewport(0,0,ww,hh);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);int w=ww,h=w*512/640;if(h>hh){h=hh;w=h*640/512;}glViewport((ww-w)/2,(hh-h)/2,w,h);
 if(latest){glUseProgram(drawProgram);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,original?originalTexture:pipe->outputTexture());glUniform1i(glGetUniformLocation(drawProgram,"tex"),0);
 glBindVertexArray(vao);glDrawArrays(GL_TRIANGLE_STRIP,0,4);}lastWidth=width();lastHeight=height();
 double t=now();if(t-statsTime>=1){double fps=(shown-statsFrames)/(t-statsTime);QString temps;if(latest){auto word=[&](int n){return int16_t(uint16_t(latest->param[2*n])|(uint16_t(latest->param[2*n+1])<<8));};int hot=word(46),cold=word(49);if(hot>=cold&&cold>=-1000&&hot<=10000)temps=QString("  热 %1°C / 冷 %2°C").arg(hot/10.,0,'f',1).arg(cold/10.,0,'f',1);}
 QString cpuTemp=cpuTemperature>0?QString("  CPU %1°C").arg(cpuTemperature,0,'f',1):QString();QString s=QString("%1 fps · %2 ms · USB %3 帧%4\n%5").arg(fps,0,'f',1).arg(pipe->milliseconds,0,'f',1).arg(cam.frames.load()).arg(temps).arg(QString::fromStdString(cam.status().empty()?pipe->renderer:cam.status())+cpuTemp);if(status)status->setText(s);if(infoReady)infoReady(latest,fps,pipe->milliseconds);if(diagnostics)std::cout<<"displayFPS="<<fps<<" pipelineMS="<<pipe->milliseconds<<" usbFrames="<<cam.frames<<" queueDrops="<<cam.dropped<<" syncLoss="<<cam.syncLoss<<" mode="<<(motion?"motion":"autogate")<<" observations="<<observations<<" palette="<<paletteMode<<" cpuTemp="<<cpuTemperature<<"\n"<<std::flush;statsTime=t;statsFrames=shown;}
 }catch(const std::exception&e){failed=true;std::cerr<<e.what()<<std::endl;if(status)status->setText(QString::fromUtf8(e.what()));}}
};

#include "ui.hpp"
#include "language_dialog.hpp"
int main(int argc,char**argv){
 // This viewer has no editable text fields. Avoid loading the desktop IME's
 // candidate windows/watchers; physical keyboard shortcuts still use Qt.
 qputenv("QT_IM_MODULE","compose");
 if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")){
  // Qt 6.4's xdg activation integration leaks proxies when changing focus.
  // Use hardware EGL through XWayland on that version; newer Qt stays native.
  if(QString::fromLatin1(qVersion()).startsWith("6.4.")&&!qEnvironmentVariableIsEmpty("DISPLAY"))qputenv("QT_QPA_PLATFORM","xcb");
  else if(!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))qputenv("QT_QPA_PLATFORM","wayland");
 }
 if(qEnvironmentVariable("QT_QPA_PLATFORM")=="xcb"&&qEnvironmentVariableIsEmpty("QT_XCB_GL_INTEGRATION"))qputenv("QT_XCB_GL_INTEGRATION","xcb_egl");
 qputenv("MESA_SHADER_CACHE_DIR",QDir(qEnvironmentVariable("XDG_CACHE_HOME",QDir::home().filePath(".cache"))).filePath("itl612-cm5").toLocal8Bit());qputenv("MESA_SHADER_CACHE_MAX_SIZE","16M");
 qputenv("QT_DISABLE_SHADER_DISK_CACHE","1");
 QApplication app(argc,argv);QPixmapCache::setCacheLimit(2048);QImageReader::setAllocationLimit(16);app.setApplicationName("ITL612 CM5");QSettings settings("ITL612","CM5");
#ifdef ITL_ENABLE_DIAGNOSTICS
 for(auto arg:app.arguments())if(arg.startsWith("--first-run-test=")){auto code=arg.section('=',1);QTimer::singleShot(100,&app,[code]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="initialLanguage")if(auto b=w->findChild<QPushButton*>("language-"+code))b->click();});}
#endif
 if(!selectInitialLanguage(settings))return 0;
#ifdef ITL_ENABLE_DIAGNOSTICS
 if(app.arguments().contains("--language-config-check")){std::cout<<"language="<<settings.value("ui/language").toString().toStdString()<<std::endl;return 0;}
#endif
 Camera cam;cam.start();AppWindow window(cam);QObject::connect(&app,&QCoreApplication::aboutToQuit,&window,[&window]{window.shutdown();});window.showFullScreen();window.startAudit(app.arguments());int result=app.exec();cam.stop();return result;}
