#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>

#include "gl_triangle_window.h"

int main(int argc, char* argv[]) {
  QGuiApplication a(argc, argv);

  if (!QGuiApplication::primaryScreen()) {
    qCritical() << "No screens available!";
    return EXIT_FAILURE;
  }

  GlTriangleWindow window{};
  if (!window.winId()) {
    qCritical() << "Failed to create window!";
    return EXIT_FAILURE;
  }
  window.resize(640, 480);
  window.setTitle("Qt OpenGL Triangle");
  window.show();

  return a.exec();
}
