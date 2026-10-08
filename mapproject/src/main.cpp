#include <QApplication>

#include "mapmainwidget.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  MapMainWidget view;
  view.setWindowTitle("MapStudio - OSM Map Demo");
  view.resize(1024, 768);
  view.show();

  return app.exec();
}