#include "viewport/ViewportWidget.hpp"

#ifdef VERITY_QT_SHELL

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QDebug>
#include <cmath>

ViewportWidget::ViewportWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    timer_.start();
}

ViewportWidget::~ViewportWidget() {
    if (builder_.joinable()) builder_.join();
}

void ViewportWidget::resetView() {
    zoom_ = 1.0;
    pan_ = QPointF(0.0, 0.0);
}

void ViewportWidget::initializeGL() {
    initializeOpenGLFunctions();
    extra_ = context()->extraFunctions();
    lastFrameNs_ = timer_.nsecsElapsed();

    // Probe for multi-draw support
    QOpenGLContext* ctx = context();
    const auto fmt = ctx->format();
    const bool isGLES = ctx->isOpenGLES();
    const bool hasARB = ctx->hasExtension(QByteArrayLiteral("GL_ARB_multi_draw_arrays"));
    const bool hasEXT = ctx->hasExtension(QByteArrayLiteral("GL_EXT_multi_draw_arrays"));
    const bool verOK = (fmt.majorVersion() > 1) || (fmt.majorVersion() == 1 && fmt.minorVersion() >= 4);
    multiDrawFn_ = reinterpret_cast<PFNGLMULTIDRAWARRAYSPROC>(ctx->getProcAddress("glMultiDrawArrays"));
    canMultiDraw_ = (multiDrawFn_ != nullptr) && (hasARB || hasEXT || verOK) && !isGLES;
    qDebug() << "Viewport: multi-draw" << (canMultiDraw_ ? "ENABLED" : "DISABLED");
    // Compose GL info text
    glInfoText_ = QString("GL %1.%2 %3")
                      .arg(fmt.majorVersion())
                      .arg(fmt.minorVersion())
                      .arg(isGLES ? "(ES)" : "(desktop)");

    // Build simple color-only shader
    const char* vs = R"GLSL(
        #version 330 core
        layout(location=0) in vec2 a_pos;
        uniform vec2 u_viewport; // w,h in pixels
        uniform float u_zoom;
        uniform vec2 u_pan;
        uniform float u_pointSize;
        void main(){
          // a_pos is in world units. Convert to pixels: zoom * a_pos.
          // Pan is stored in pixels (from mouse drag). Origin is screen center.
          vec2 px = u_zoom * a_pos + u_pan; // pixels relative to screen center
          // Convert pixels to NDC: divide by half viewport and flip Y.
          vec2 ndc = vec2(px.x / (0.5*u_viewport.x), -px.y / (0.5*u_viewport.y));
          gl_Position = vec4(ndc, 0.0, 1.0);
          gl_PointSize = u_pointSize;
        }
    )GLSL";
    const char* fs = R"GLSL(
        #version 330 core
        uniform vec4 u_color;
        out vec4 fcolor;
        void main(){ fcolor = u_color; }
    )GLSL";
    prog_.addShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    prog_.addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    prog_.link();

    vao_.create();
    vao_.bind();
    vbo_.create();
    vbo_.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2, (const void*)0);
    vbo_.release();
    vao_.release();

    actorVbo_.create();

    glEnable(GL_PROGRAM_POINT_SIZE);

    // Build base engine curves once
    buildBaseCurves();

    // Start incremental builder timer
    buildTimer_ = new QTimer(this);
    buildTimer_->setInterval(50); // 20 Hz
    connect(buildTimer_, &QTimer::timeout, this, [this]() { scheduleBuildNext(); });
    buildTimer_->start();
    // Kick first batch immediately
    scheduleBuildNext();
}

void ViewportWidget::resizeGL(int, int) {
    // No-op; we use QPainter with widget coords.
}

void ViewportWidget::drawGrid(QPainter& p) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(QColor(60, 60, 60));
    p.setPen(pen);

    // Draw grid lines in a window around origin
    const int count = 20;
    const double step = 50.0;
    for (int i = -count; i <= count; ++i) {
        QPointF a = QPointF(width()*0.5 + (i*step*zoom_ + pan_.x()), height()*0.5 - (-count*step*zoom_ + pan_.y()));
        QPointF b = QPointF(width()*0.5 + (i*step*zoom_ + pan_.x()), height()*0.5 - (count*step*zoom_ + pan_.y()));
        p.drawLine(a, b);
        QPointF c = QPointF(width()*0.5 + (-count*step*zoom_ + pan_.x()), height()*0.5 - (i*step*zoom_ + pan_.y()));
        QPointF d = QPointF(width()*0.5 + (count*step*zoom_ + pan_.x()), height()*0.5 - (i*step*zoom_ + pan_.y()));
        p.drawLine(c, d);
    }

    // Axes
    p.setPen(QPen(QColor(150, 60, 60)));
    p.drawLine(QPointF(width()*0.5 + (-1000*zoom_ + pan_.x()), height()*0.5 - (0 + pan_.y())),
               QPointF(width()*0.5 + (1000*zoom_ + pan_.x()), height()*0.5 - (0 + pan_.y())));
    p.setPen(QPen(QColor(60, 150, 60)));
    p.drawLine(QPointF(width()*0.5 + (0 + pan_.x()), height()*0.5 - (-1000*zoom_ + pan_.y())),
               QPointF(width()*0.5 + (0 + pan_.x()), height()*0.5 - (1000*zoom_ + pan_.y())));

    p.restore();
}

void ViewportWidget::buildBaseCurves() {
    using namespace verity;
    // Build two Hermite base curves for X(t), Y(t) over t in [0,1]
    curveX_ = createCurve(CurveKind::Hermite);
    curveY_ = createCurve(CurveKind::Hermite);
    const int K = 50;
    std::vector<Key> kx, ky;
    kx.reserve(K); ky.reserve(K);
    for (int i = 0; i < K; ++i) {
        float t = float(i) / float(K - 1);
        float x = 200.0f * std::sin(2.0f * float(M_PI) * t);
        float y = 100.0f * std::sin(4.0f * float(M_PI) * t + 0.5f);
        float dxdt = 200.0f * 2.0f * float(M_PI) * std::cos(2.0f * float(M_PI) * t);
        float dydt = 100.0f * 4.0f * float(M_PI) * std::cos(4.0f * float(M_PI) * t + 0.5f);
        kx.push_back(verity::Key{t, x, dxdt, dxdt});
        ky.push_back(verity::Key{t, y, dydt, dydt});
    }
    setKeys(curveX_, kx);
    setKeys(curveY_, ky);
    setConstantSpeed(curveX_, true);
    setConstantSpeed(curveY_, true);

}

void ViewportWidget::scheduleBuildNext() {
    // Build next batch of paths asynchronously
    if (builder_.joinable()) return; // still working
    const int N = samplesPerPath_;
    std::vector<float> base;
    base.resize(N * 2);
    float bminx = 1e9f, bminy = 1e9f, bmaxx = -1e9f, bmaxy = -1e9f;
    for (int i = 0; i < N; ++i) {
        float t = float(i) / float(N - 1);
        float x = verity::evaluate(curveX_, t);
        float y = verity::evaluate(curveY_, t);
        base[i*2+0] = x;
        base[i*2+1] = y;
        bminx = std::min(bminx, x); bmaxx = std::max(bmaxx, x);
        bminy = std::min(bminy, y); bmaxy = std::max(bmaxy, y);
    }

    int batch = 8; // build a few per tick
    int startPath = builtPaths_;
    int endPath = std::min(totalPaths_, builtPaths_ + batch);
    if (startPath == 0) {
        pendingVerts_.clear();
        pendingRanges_.clear();
        pathVerts_.clear();
        ranges_.clear();
        float centerShift = -0.5f * (totalPaths_ - 1) * spacing_;
        firstOffsetX_ = centerShift;
    }
    builder_ = std::thread([=]() {
        std::vector<float> localVerts;
        std::vector<PathRange> localRanges;
        localVerts.reserve((endPath - startPath) * N * 2);
        float centerShiftL = -0.5f * (totalPaths_ - 1) * spacing_;
        for (int p = startPath; p < endPath; ++p) {
            float offsetX = centerShiftL + p * spacing_;
            int start = int(localVerts.size() / 2);
            float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
            for (int i = 0; i < N; ++i) {
                float x = base[i*2+0] + offsetX;
                float y = base[i*2+1];
                localVerts.push_back(x);
                localVerts.push_back(y);
                minx = std::min(minx, x); maxx = std::max(maxx, x);
                miny = std::min(miny, y); maxy = std::max(maxy, y);
            }
            int count = N;
            QRectF bounds(QPointF(minx, miny), QPointF(maxx, maxy));
            localRanges.push_back(PathRange{start, count, bounds});
        }
        // publish
        pendingVerts_ = std::move(localVerts);
        pendingRanges_ = std::move(localRanges);
        pendingReady_ = true;
    });
    builtPaths_ = endPath;
}

void ViewportWidget::updateGpuPath() {
    vao_.bind();
    vbo_.bind();
    vbo_.allocate(pathVerts_.data(), int(pathVerts_.size() * sizeof(float)));
    vbo_.release();
    vao_.release();
}

void ViewportWidget::updateFps() {
    qint64 now = timer_.nsecsElapsed();
    double dtMs = double(now - lastFrameNs_) / 1.0e6;
    lastFrameNs_ = now;
    frameTimesMs_.push_back(dtMs);
    if (frameTimesMs_.size() > 60) frameTimesMs_.pop_front();
    double sum = 0.0;
    for (double m : frameTimesMs_) sum += m;
    double avg = frameTimesMs_.empty() ? 0.0 : sum / double(frameTimesMs_.size());
    fps_ = (avg > 0.0) ? (1000.0 / avg) : 0.0;
    tAnim_ += dtMs / 1000.0; // seconds
}

void ViewportWidget::paintGL() {
    updateFps();

    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 22));
    drawGrid(p);
    drawHud(p);
    p.end();

    // If worker has built new data, append to VBO
    if (pendingReady_.load()) {
        int oldVerts = int(pathVerts_.size());
        int oldCount = int(ranges_.size());
        pathVerts_.insert(pathVerts_.end(), pendingVerts_.begin(), pendingVerts_.end());
        for (const auto& r : pendingRanges_) {
            ranges_.push_back(PathRange{r.start + oldVerts/2, r.count, r.bounds});
        }
        updateGpuPath();
        pendingReady_ = false;
        if (builder_.joinable()) builder_.join();
    }

    // Draw paths with GL line strip, with simple bounds culling
    prog_.bind();
    prog_.setUniformValue("u_viewport", QVector2D(float(width()), float(height())));
    prog_.setUniformValue("u_zoom", float(zoom_));
    prog_.setUniformValue("u_pan", QVector2D(float(pan_.x()), float(pan_.y())));
    prog_.setUniformValue("u_color", QVector4D(0.47f, 0.63f, 0.86f, 1.0f));
    vao_.bind();
    const QRectF viewWorld(QPointF(-width()*0.5/zoom_ - pan_.x()/zoom_, -height()*0.5/zoom_ - pan_.y()/zoom_),
                           QSizeF(width()/zoom_, height()/zoom_));
    if (perPathColors_ || !canMultiDraw_ || forceNoMultiDraw_) {
        // Per-path colors or no extra functions: loop draw with color changes
        int idx = 0;
        for (const auto& r : ranges_) {
            if (!r.bounds.intersects(viewWorld)) { ++idx; continue; }
            // simple palette
            float hue = (idx % 12) / 12.0f;
            QVector4D col(0.47f + 0.4f*hue, 0.63f - 0.3f*hue, 0.86f, 1.0f);
            prog_.setUniformValue("u_color", col);
            glDrawArrays(GL_LINE_STRIP, r.start, r.count);
            ++idx;
        }
    } else {
        // Use multi-draw for fewer state changes
        std::vector<GLint> firsts;
        std::vector<GLsizei> counts;
        firsts.reserve(ranges_.size()); counts.reserve(ranges_.size());
        for (const auto& r : ranges_) {
            if (!r.bounds.intersects(viewWorld)) continue;
            firsts.push_back(r.start);
            counts.push_back(r.count);
        }
        prog_.setUniformValue("u_color", QVector4D(0.47f, 0.63f, 0.86f, 1.0f));
        if (!firsts.empty()) multiDrawFn_(GL_LINE_STRIP, firsts.data(), counts.data(), GLsizei(firsts.size()));
    }
    vao_.release();
    prog_.release();

    // Draw moving actor as a point sprite following the left-most path via engine eval
    float t = std::fmod(float(timer_.elapsed()) * 0.00025f, 1.0f); // 0.25 cycles per second
    float ax = verity::evaluate(curveX_, t) + firstOffsetX_;
    float ay = verity::evaluate(curveY_, t);
    float actor[2] = {ax, ay};
    if (showTrail_) {
        trail_.push_back(QPointF(actor[0], actor[1]));
        if (trail_.size() > 200) trail_.pop_front();
    } else {
        trail_.clear();
    }
    actorVbo_.bind();
    actorVbo_.allocate(actor, sizeof(actor));
    actorVbo_.release();

    actorVao_.bind();
    actorVbo_.bind();
    // Re-specify attribute pointer to be robust across drivers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2, (const void*)0);
    prog_.bind();
    prog_.setUniformValue("u_viewport", QVector2D(float(width()), float(height())));
    prog_.setUniformValue("u_zoom", float(zoom_));
    prog_.setUniformValue("u_pan", QVector2D(float(pan_.x()), float(pan_.y())));
    prog_.setUniformValue("u_color", QVector4D(0.94f, 0.78f, 0.24f, 1.0f));
    prog_.setUniformValue("u_pointSize", 8.0f);
    // Point size is set in the vertex shader via u_pointSize
    glDrawArrays(GL_POINTS, 0, 1);
    prog_.release();
    actorVbo_.release();
    actorVao_.release();

    // Schedule next repaint (roughly 60 FPS)
    update();
}

void ViewportWidget::mousePressEvent(QMouseEvent* e) {
    lastMouse_ = e->pos();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* e) {
    QPoint d = e->pos() - lastMouse_;
    lastMouse_ = e->pos();
    // Pan in world units scaled by zoom
    pan_ += QPointF(d.x(), -d.y());
}

void ViewportWidget::wheelEvent(QWheelEvent* e) {
    const double step = e->angleDelta().y() / 120.0; // 1 step per notch
    const double factor = std::pow(1.1, step);
    zoom_ = std::clamp(zoom_ * factor, 0.1, 10.0);
}

void ViewportWidget::drawHud(QPainter& p) {
    // Text HUD (stacked lines to avoid overlaps)
    int xText = 10;
    int yText = 18; // first baseline
    int lineH = 18; // line spacing
    p.setPen(QColor(220, 220, 220));
    p.drawText(QPoint(xText, yText), QString("FPS: %1").arg(fps_, 0, 'f', 1));
    yText += lineH;
    QString mdText;
    if (!canMultiDraw_) {
        mdText = "MultiDraw: unsupported on this context";
    } else if (forceNoMultiDraw_) {
        mdText = "MultiDraw: supported (forced off)";
    } else if (perPathColors_) {
        mdText = "MultiDraw: supported (off due to per-path colors)";
    } else {
        mdText = "MultiDraw: active";
    }
    p.drawText(QPoint(xText, yText), mdText);
    if (showGlInfo_) {
        yText += lineH;
        p.drawText(QPoint(xText, yText), QString("%1  ForcedNoMD:%2  Colors:%3")
                                             .arg(glInfoText_)
                                             .arg(forceNoMultiDraw_ ? "on" : "off")
                                             .arg(perPathColors_ ? "on" : "off"));
    }
    yText += lineH;
    p.setPen(QColor(180, 180, 180));
    p.drawText(QPoint(xText, yText), QString("Drag: pan, Wheel: zoom, R: reset, T: trail, C: per-path colors"));

    // FPS history bar (last ~60 frames)
    int x0 = 10, y0 = yText + 12, w = 120, h = 30;
    p.setPen(QColor(100,100,100));
    p.drawRect(x0, y0, w, h);
    if (!frameTimesMs_.empty()) {
        double maxMs = 40.0; // clamp at ~25 FPS
        int n = int(frameTimesMs_.size());
        for (int i = 0; i < n; ++i) {
            double ms = std::min(maxMs, frameTimesMs_[i]);
            int bar = int((ms / maxMs) * h);
            p.fillRect(x0 + i*2, y0 + (h - bar), 1, bar, QColor(120, 180, 120));
        }
    }
    // Trail (overlay in screen space)
    if (showTrail_ && !trail_.empty()) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(255, 220, 100, 160), 2));
        QPointF prev;
        bool first=true;
        for (const auto& w : trail_) {
            QPointF px(width()*0.5 + (w.x()*zoom_ + pan_.x()), height()*0.5 - (w.y()*zoom_ + pan_.y()));
            if (!first) p.drawLine(prev, px);
            prev = px; first = false;
        }
    }
}

// Optional: reset view with 'R'
void ViewportWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_R) {
        resetView();
    } else if (e->key() == Qt::Key_T) {
        showTrail_ = !showTrail_;
    } else if (e->key() == Qt::Key_C) {
        perPathColors_ = !perPathColors_;
    }
    QOpenGLWidget::keyPressEvent(e);
}

// scheduleBuild() deprecated; incremental building uses scheduleBuildNext() via QTimer

#endif // VERITY_QT_SHELL
