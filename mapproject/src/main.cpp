#include <QApplication>
#include <QFile>

#include "mapmainwidget.h"

QString loadQss(const QString& resourcePath) {
  QFile file(resourcePath);
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qWarning() << "Cannot open QSS:" << resourcePath << file.errorString();
    return {};
  }
  QString qss = QString::fromUtf8(file.readAll());
  file.close();
  return qss;
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  QString qss = loadQss(":/style/resources_white.qss");
  if (!qss.isEmpty()) {
    qApp->setStyleSheet(qss);
  }

  MapMainWidget view;
  view.setWindowTitle("MapStudio - Map Demo");
  view.resize(600, 300);
  view.show();
  return app.exec();
}