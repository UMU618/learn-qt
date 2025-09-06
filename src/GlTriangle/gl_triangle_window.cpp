#include "gl_triangle_window.h"

#include <QtGui/QScreen>
#include <QtGui/QVector3D>

static const char* kVertexShaderSource =
    "attribute highp vec4 posAttr;\n"
    "attribute lowp vec4 colAttr;\n"
    "varying lowp vec4 col;\n"
    "uniform highp mat4 matrix;\n"
    "void main() {\n"
    "   col = colAttr;\n"
    "   gl_Position = matrix * posAttr;\n"
    "}\n";

static const char* kFragmentShaderSource =
    "varying lowp vec4 col;\n"
    "void main() {\n"
    "   gl_FragColor = col;\n"
    "}\n";

GlTriangleWindow::GlTriangleWindow(QWindow* parent)
    : QOpenGLWindow(NoPartialUpdate, parent), animate_timer_{new QTimer(this)} {
  animate_timer_->setInterval(30);
  connect(animate_timer_, &QTimer::timeout, this, &GlTriangleWindow::OnTimer);
}

GlTriangleWindow::~GlTriangleWindow() {}

void GlTriangleWindow::initializeGL() {
  initializeOpenGLFunctions();

  shader_program_ = new QOpenGLShaderProgram(this);
  shader_program_->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                           kVertexShaderSource);
  shader_program_->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                           kFragmentShaderSource);
  shader_program_->link();
  pos_ = shader_program_->attributeLocation("posAttr");
  Q_ASSERT(pos_ != -1);
  col_ = shader_program_->attributeLocation("colAttr");
  Q_ASSERT(col_ != -1);
  matrix_uniform_ = shader_program_->uniformLocation("matrix");
  Q_ASSERT(matrix_uniform_ != -1);

  animate_timer_->start();
}

void GlTriangleWindow::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
}

void GlTriangleWindow::paintGL() {
  const qreal retina_scale = devicePixelRatio();
  glViewport(0, 0, width() * retina_scale, height() * retina_scale);

  glClear(GL_COLOR_BUFFER_BIT);

  shader_program_->bind();

  QMatrix4x4 matrix;
  matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
  matrix.translate(0, 0, -2);
  matrix.rotate(100.0f * frame_ / screen()->refreshRate(), 0, 1, 0);

  shader_program_->setUniformValue(matrix_uniform_, matrix);

  static const GLfloat kVertices[] = {0.0f, 0.707f, -0.5f, -0.5f, 0.5f, -0.5f};

  static const GLfloat kColors[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f};

  glVertexAttribPointer(pos_, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  glVertexAttribPointer(col_, 3, GL_FLOAT, GL_FALSE, 0, kColors);

  glEnableVertexAttribArray(pos_);
  glEnableVertexAttribArray(col_);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisableVertexAttribArray(pos_);
  glDisableVertexAttribArray(col_);

  shader_program_->release();

  ++frame_;
}

void GlTriangleWindow::OnTimer() {
  update();
}
