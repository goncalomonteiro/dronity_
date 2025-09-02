#pragma once

#ifdef VERITY_QT_SHELL

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QPoint>
#include <deque>
#include <vector>
#include <thread>
#include <atomic>
#include "verity/engine.hpp"

class ViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    // Camera controls
    void resetView();
    // UI toggles
    void setForceNoMultiDraw(bool v) { forceNoMultiDraw_ = v; }
    bool forceNoMultiDraw() const { return forceNoMultiDraw_; }
    void setShowGlInfo(bool v) { showGlInfo_ = v; }
    bool showGlInfo() const { return showGlInfo_; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    // Helpers
    void drawGrid(QPainter& p);
    void drawHud(QPainter& p);
    void updateFps();
    void buildBaseCurves();
    void scheduleBuildNext();
    void updateGpuPath();

    // View state
    QPoint lastMouse_ {0, 0};
    double zoom_ {1.0};
    QPointF pan_ {0.0, 0.0};
    QElapsedTimer timer_;
    qint64 lastFrameNs_ {0};
    std::deque<double> frameTimesMs_;
    double fps_ {0.0};
    double tAnim_ {0.0}; // seconds
    float firstOffsetX_ {0.0f};
    bool showTrail_ {false};
    bool perPathColors_ {false};
    std::deque<QPointF> trail_; // recent actor positions (world)
    bool showGlInfo_ {false};

    // Engine curves (2D path = X and Y curves)
    int curveX_ {-1};
    int curveY_ {-1};
    std::vector<float> pathVerts_; // interleaved x,y in world units
    struct PathRange { int start; int count; QRectF bounds; };
    std::vector<PathRange> ranges_;
    int totalPaths_ {32};
    int builtPaths_ {0};
    int samplesPerPath_ {400};
    float spacing_ {120.0f};

    // GL objects
    QOpenGLBuffer vbo_ {QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao_;
    QOpenGLShaderProgram prog_;
    QOpenGLBuffer actorVbo_ {QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject actorVao_;
    QOpenGLExtraFunctions* extra_ {nullptr};
    // Avoid APIENTRY to keep this portable across moc builds
    using PFNGLMULTIDRAWARRAYSPROC = void (*)(GLenum, const GLint*, const GLsizei*, GLsizei);
    PFNGLMULTIDRAWARRAYSPROC multiDrawFn_ {nullptr};
    bool canMultiDraw_ {false};
    bool forceNoMultiDraw_ {false};
    QString glInfoText_;

    // Worker build
    std::thread builder_;
    std::atomic<bool> pendingReady_ {false};
    std::vector<float> pendingVerts_;
    std::vector<PathRange> pendingRanges_;
    QTimer* buildTimer_ {nullptr};
};

#endif // VERITY_QT_SHELL
