#ifndef GL_TRIANGLE_WINDOW_H
#define GL_TRIANGLE_WINDOW_H

#include <QtCore/QTimer>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLWindow>

class GlTriangleWindow : public QOpenGLWindow, protected QOpenGLFunctions {
  Q_OBJECT

 public:
  GlTriangleWindow(QWindow* parent = nullptr);
  ~GlTriangleWindow();

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

 private:
  void OnTimer();

 private:
  QOpenGLShaderProgram* shader_program_{};
  GLint pos_{};
  GLint col_{};
  GLint matrix_uniform_{};
  int frame_{};
  QTimer* animate_timer_;
};

#endif  // GL_TRIANGLE_WINDOW_H
