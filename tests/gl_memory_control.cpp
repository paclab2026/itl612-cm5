#include <QApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <GLES3/gl31.h>
class PaintControl:public QOpenGLWidget {void paintGL()override{glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}};
int main(int argc,char**argv){QApplication app(argc,argv);QWidget window;auto layout=new QVBoxLayout(&window);layout->addWidget(new QLabel("窗口资源对照"));auto widget=new PaintControl;QSurfaceFormat f;f.setRenderableType(QSurfaceFormat::OpenGLES);f.setVersion(3,1);widget->setFormat(f);widget->setUpdateBehavior(QOpenGLWidget::PartialUpdate);layout->addWidget(widget,1);auto notice=new QLabel("提示");layout->addWidget(notice);window.showFullScreen();QTimer timer;int step=0;QObject::connect(&timer,&QTimer::timeout,&window,[&]{notice->setVisible(step%2);widget->update();if(++step==24)window.close();});timer.start(80);return app.exec();}
