#pragma once
#include "i18n.hpp"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
namespace itl {
inline bool selectInitialLanguage(QSettings&settings){
 auto saved=settings.value("ui/language").toString();
 if(saved=="zh"||saved=="en"){english=saved=="en";return true;}
 QDialog dialog;dialog.setObjectName("initialLanguage");dialog.setWindowTitle("语言/lang");dialog.setFixedSize(480,250);
 dialog.setStyleSheet("QDialog{background:#0e0d09;color:#FF8C00;} QLabel{color:#FF8C00;font-size:22px;} QPushButton{background:#19130c;color:#FF8C00;border:1px solid #62401d;border-radius:3px;padding:12px;font-size:22px;min-height:40px;}");
 auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);auto title=new QLabel("选择语言 / Language");title->setAlignment(Qt::AlignCenter);layout->addWidget(title);auto row=new QHBoxLayout;
 for(auto code:{QString("zh"),QString("en")}){auto button=new QPushButton(code=="zh"?QString::fromUtf8("中文"):QString("ENG"));button->setObjectName("language-"+code);row->addWidget(button);QObject::connect(button,&QPushButton::clicked,&dialog,[&,code]{english=code=="en";settings.setValue("ui/language",code);settings.sync();dialog.accept();});}layout->addLayout(row);
 return dialog.exec()==QDialog::Accepted;
}
}
