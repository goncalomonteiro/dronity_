#include "viewport/ViewportWidget.hpp"

#ifdef VERITY_QT_SHELL

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QMatrix4x4>
#include <QDebug>
#include <cmath>
#include <algorithm>

ViewportWidget::ViewportWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    timer_.start();
}

ViewportWidget::~ViewportWidget() {
    if (builder_.joinable()) builder_.join();
}

void ViewportWidget::resetView() {
    camYaw_ = 0.0f; camPitch_ = 0.35f; camDist_ = 600.0f;
    zoom_ = 1.0; pan_ = QPointF(0.0, 0.0);
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
        layout(location=0) in vec3 a_pos;
        uniform mat4 u_mvp;
        uniform float u_pointSize;
        void main(){
          gl_Position = u_mvp * vec4(a_pos, 1.0);
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

    // No screen-space polyline program in 3D-only mode

    vao_.create();
    vao_.bind();
    vbo_.create();
    vbo_.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (const void*)0);
    vbo_.release();
    vao_.release();
    // Prepare ping-pong buffer
    vboPing_.create();

    actorVbo_.create();
    actorVao_.create();

    // Instanced markers VAO (per-instance position at location 0, divisor=1)
    instVao_.create();
    instVbo_.create();
    instVao_.bind();
    instVbo_.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (const void*)0);
    if (extra_) extra_->glVertexAttribDivisor(0, 1);
    instVbo_.release();
    instVao_.release();

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    // Improve line continuity/quality
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.25f);

    // Depth test off for path lines to ensure visual continuity
    glDisable(GL_DEPTH_TEST);

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

void ViewportWidget::resizeGL(int, int) { /* no-op */ }

void ViewportWidget::drawGrid(QPainter& p) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(QColor(60, 60, 60));
    p.setPen(pen);
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
    // Build three Hermite base curves for X(t), Y(t), Z(t) over t in [0,1]
    curveX_ = createCurve(CurveKind::Hermite);
    curveY_ = createCurve(CurveKind::Hermite);
    curveZ_ = createCurve(CurveKind::Hermite);
    const int K = 50;
    std::vector<Key> kx, ky, kz;
    kx.reserve(K); ky.reserve(K); kz.reserve(K);
    for (int i = 0; i < K; ++i) {
        float t = float(i) / float(K - 1);
        float x = 200.0f * std::sin(2.0f * float(M_PI) * t);
        float y = 100.0f * std::sin(4.0f * float(M_PI) * t + 0.5f);
        // Use integer multiples of 2π for perfect loop continuity at t=0 and t=1
        float z = 50.0f  * std::sin(6.0f * float(M_PI) * t + 1.0f); // 3 full cycles
        float dxdt = 200.0f * 2.0f * float(M_PI) * std::cos(2.0f * float(M_PI) * t);
        float dydt = 100.0f * 4.0f * float(M_PI) * std::cos(4.0f * float(M_PI) * t + 0.5f);
        float dzdt = 50.0f  * 6.0f * float(M_PI) * std::cos(6.0f * float(M_PI) * t + 1.0f);
        kx.push_back(verity::Key{t, x, dxdt, dxdt});
        ky.push_back(verity::Key{t, y, dydt, dydt});
        kz.push_back(verity::Key{t, z, dzdt, dzdt});
    }
    setKeys(curveX_, kx);
    setKeys(curveY_, ky);
    setKeys(curveZ_, kz);
    setConstantSpeed(curveX_, false);
    setConstantSpeed(curveY_, false);
    setConstantSpeed(curveZ_, false);

}

void ViewportWidget::scheduleBuildNext() {
    // Build next batch of paths asynchronously
    if (builder_.joinable()) return; // still working
    const int N = samplesPerPath_;
    std::vector<float> base;
    base.resize(N * 3);
    float bminx = 1e9f, bminy = 1e9f, bmaxx = -1e9f, bmaxy = -1e9f;
    for (int i = 0; i < N; ++i) {
        float t = float(i) / float(N - 1);
        float x = verity::evaluate(curveX_, t);
        float y = verity::evaluate(curveY_, t);
        float z = verity::evaluate(curveZ_, t);
        base[i*3+0] = x;
        base[i*3+1] = y;
        base[i*3+2] = z;
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
        localVerts.reserve((endPath - startPath) * N * 3);
        float centerShiftL = -0.5f * (totalPaths_ - 1) * spacing_;
        for (int p = startPath; p < endPath; ++p) {
            float offsetX = centerShiftL + p * spacing_;
            int start = int(localVerts.size() / 3);
            float minx = 1e9f, miny = 1e9f, minz = 1e9f;
            float maxx = -1e9f, maxy = -1e9f, maxz = -1e9f;
            for (int i = 0; i < N; ++i) {
                float x = base[i*3+0] + offsetX;
                float y = base[i*3+1];
                float z = base[i*3+2];
                localVerts.push_back(x);
                localVerts.push_back(y);
                localVerts.push_back(z);
                minx = std::min(minx, x); maxx = std::max(maxx, x);
                miny = std::min(miny, y); maxy = std::max(maxy, y);
                minz = std::min(minz, z); maxz = std::max(maxz, z);
            }
            int count = N;
            QRectF bounds(QPointF(minx, miny), QPointF(maxx, maxy));
            QVector3D c((minx+maxx)*0.5f, (miny+maxy)*0.5f, (minz+maxz)*0.5f);
            float dx = (maxx - minx);
            float dy = (maxy - miny);
            float dz = (maxz - minz);
            float radius = 0.5f * std::sqrt(dx*dx + dy*dy + dz*dz);
            localRanges.push_back(PathRange{start, count, bounds, minz, maxz, c, radius});
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
    // Ping-pong VBO update to avoid driver stalls on in-use buffers
    QOpenGLBuffer& target = usePing_ ? vbo_ : vboPing_;
    if (!target.isCreated()) target.create();
    target.bind();
    target.allocate(pathVerts_.data(), int(pathVerts_.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (const void*)0);
    target.release();
    vao_.release();
    usePing_ = !usePing_;
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
    if (!enable3D_) drawGrid(p);
    drawHud(p);
    p.end();

    // If worker has built new data, append to VBO
    if (pendingReady_.load()) {
        int oldFloats = int(pathVerts_.size());
        int oldVerts = oldFloats / 3;
        pathVerts_.insert(pathVerts_.end(), pendingVerts_.begin(), pendingVerts_.end());
        for (const auto& r : pendingRanges_) {
            ranges_.push_back(PathRange{r.start + oldVerts, r.count, r.bounds, r.minZ, r.maxZ, r.center, r.radius});
        }
        updateGpuPath();
        pendingReady_ = false;
        if (builder_.joinable()) builder_.join();
    }

    // Draw paths with GL line strip (3D only)
    prog_.bind();
    updateMatrices();
    prog_.setUniformValue("u_mvp", mvp_);
    prog_.setUniformValue("u_color", QVector4D(0.47f, 0.63f, 0.86f, 1.0f));
    vao_.bind();
    // Draw all paths as single line strips (no 2D/NDC expansion)
    int idx = 0;
    for (const auto& r : ranges_) {
        // simple palette per path for debugging continuity
        float hue = (idx % 12) / 12.0f;
        QVector4D col(0.47f + 0.4f*hue, 0.63f - 0.3f*hue, 0.86f, 1.0f);
        prog_.setUniformValue("u_color", col);
        glDrawArrays(GL_LINE_STRIP, r.start, r.count);
        ++idx;
    }
    vao_.release();
    prog_.release();

    // Draw moving actor as a point sprite following the left-most path via engine eval
    float t = std::fmod(float(timer_.elapsed()) * 0.00025f, 1.0f); // 0.25 cycles per second
    float ax = verity::evaluate(curveX_, t) + firstOffsetX_;
    float ay = verity::evaluate(curveY_, t);
    float az = verity::evaluate(curveZ_, t);
    float actor[3] = {ax, ay, az};
    actorVbo_.bind();
    actorVbo_.allocate(actor, sizeof(float)*3);
    actorVbo_.release();

    actorVao_.bind();
    actorVbo_.bind();
    // Re-specify attribute pointer to be robust across drivers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (const void*)0);
    prog_.bind();
    prog_.setUniformValue("u_mvp", mvp_);
    prog_.setUniformValue("u_color", QVector4D(0.94f, 0.78f, 0.24f, 1.0f));
    prog_.setUniformValue("u_pointSize", 8.0f);
    // Point size is set in the vertex shader via u_pointSize
    glDrawArrays(GL_POINTS, 0, 1);
    // Instanced head markers for each path (animated along the strip)
    std::vector<float> inst;
    inst.reserve(ranges_.size() * 3);
    for (const auto& r : ranges_) {
        // always draw in this simplified path
        int si = std::clamp(int(std::fmod(float(timer_.elapsed()) * 0.00025f, 1.0f) * float(r.count - 1)), 0, r.count - 1);
        int idx0 = (r.start + si) * 3;
        if (idx0 + 2 < int(pathVerts_.size())) {
            inst.push_back(pathVerts_[idx0 + 0]);
            inst.push_back(pathVerts_[idx0 + 1]);
            inst.push_back(pathVerts_[idx0 + 2]);
        }
    }
    if (!inst.empty()) {
        instVao_.bind();
        instVbo_.bind();
        instVbo_.allocate(inst.data(), int(inst.size()*sizeof(float)));
        prog_.setUniformValue("u_color", QVector4D(1.0f, 0.8f, 0.2f, 1.0f));
        prog_.setUniformValue("u_pointSize", 6.0f);
        if (extra_) extra_->glDrawArraysInstanced(GL_POINTS, 0, 1, GLsizei(inst.size()/3));
        instVbo_.release();
        instVao_.release();
    }
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
    if (enable3D_) {
        camYaw_   += 0.005f * d.x();
        camPitch_ += 0.005f * d.y();
        camPitch_ = std::clamp(camPitch_, -1.2f, 1.2f);
    } else {
        pan_ += QPointF(d.x(), -d.y());
    }
}

void ViewportWidget::wheelEvent(QWheelEvent* e) {
    const double step = e->angleDelta().y() / 120.0; // 1 step per notch
    if (enable3D_) {
        camDist_ = std::clamp(camDist_ * float(std::pow(1.1, -step)), 50.0f, 5000.0f);
    } else {
        const double factor = std::pow(1.1, step);
        zoom_ = std::clamp(zoom_ * factor, 0.1, 10.0);
    }
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
    } else {
        mdText = "MultiDraw: active";
    }
    p.drawText(QPoint(xText, yText), mdText);
    if (showGlInfo_) {
        yText += lineH;
        QString info = QString("%1  ForcedNoMD:%2")
                           .arg(glInfoText_)
                           .arg(forceNoMultiDraw_ ? "on" : "off");
        p.drawText(QPoint(xText, yText), info);
    }
    yText += lineH;
    p.setPen(QColor(180, 180, 180));
    if (enable3D_) {
        p.drawText(QPoint(xText, yText), QString("3D: Drag orbit, Wheel dolly, R reset, M toggle 2D"));
    } else {
        p.drawText(QPoint(xText, yText), QString("2D: Drag pan, Wheel zoom, R reset, M toggle 3D"));
    }

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
    // No trail overlay in 3D-only mode
}

// Optional: reset view with 'R'
void ViewportWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_R) {
        resetView();
    } else if (e->key() == Qt::Key_M) {
        enable3D_ = !enable3D_;
    }
    QOpenGLWidget::keyPressEvent(e);
}

void ViewportWidget::updateMatrices() {
    mvp_.setToIdentity();
    proj_.setToIdentity();
    view_.setToIdentity();
    if (enable3D_) {
        proj_.perspective(45.0f, float(std::max(1, width()))/float(std::max(1, height())), 1.0f, 10000.0f);
        QVector3D target(0.0f, 0.0f, 0.0f);
        float cyaw = std::cos(camYaw_), syaw = std::sin(camYaw_);
        float cp = std::cos(camPitch_), sp = std::sin(camPitch_);
        QVector3D eye = target + QVector3D(camDist_ * cp * cyaw,
                                           camDist_ * sp,
                                           camDist_ * cp * syaw);
        view_.lookAt(eye, target, QVector3D(0,1,0));
        mvp_ = proj_ * view_;
    } else {
        float cx = float(-pan_.x() / zoom_);
        float cy = float(-pan_.y() / zoom_);
        float halfW = float(width()) * 0.5f / float(zoom_);
        float halfH = float(height()) * 0.5f / float(zoom_);
        proj_.ortho(cx - halfW, cx + halfW, cy - halfH, cy + halfH, -1000.0f, 1000.0f);
        QMatrix4x4 flip; flip.setToIdentity(); flip.scale(1.0f, -1.0f, 1.0f);
        mvp_ = flip * proj_ * view_;
    }
}

// scheduleBuild() deprecated; incremental building uses scheduleBuildNext() via QTimer

#endif // VERITY_QT_SHELL
