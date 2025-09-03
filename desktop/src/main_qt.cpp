#ifdef VERITY_QT_SHELL
#include "verity/autosave.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QSurfaceFormat>
#include <QAction>
#include <cstdlib>
#include <memory>
#include "viewport/ViewportWidget.hpp"

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(const char* projectDirEnv) {
        setWindowTitle("Verity Desktop Shell");
        auto* timeline = new QDockWidget("Timeline", this);
        timeline->setWidget(new QLabel("Timeline placeholder"));
        addDockWidget(Qt::BottomDockWidgetArea, timeline);

        auto* graph = new QDockWidget("Graph", this);
        graph->setWidget(new QLabel("Graph placeholder"));
        addDockWidget(Qt::RightDockWidgetArea, graph);

        auto* viewportDock = new QDockWidget("Viewport", this);
        auto* viewport = new ViewportWidget(this);
        viewportDock->setWidget(viewport);
        addDockWidget(Qt::LeftDockWidgetArea, viewportDock);
        setCentralWidget(new QLabel("Scene"));

        // Menu
        auto* fileMenu = menuBar()->addMenu("&File");
        auto* saveSnap = new QAction("Save Snapshot Now", this);
        fileMenu->addAction(saveSnap);

        auto* viewMenu = menuBar()->addMenu("&View");
        auto* toggleMD = new QAction("Force No MultiDraw", this);
        toggleMD->setCheckable(true);
        viewMenu->addAction(toggleMD);
        auto* toggleGL = new QAction("Show GL Info", this);
        toggleGL->setCheckable(true);
        viewMenu->addAction(toggleGL);

        // Status
        statusBar()->showMessage("Autosave: Off");

        if (projectDirEnv && *projectDirEnv) {
            autosaver_ = std::make_unique<verity::AutosaveScheduler>(projectDirEnv, std::chrono::seconds(60));
            autosaver_->start();
            projectDir_ = projectDirEnv;
            statusBar()->showMessage(QString("Autosave: On - ") + projectDir_.c_str());
            connect(saveSnap, &QAction::triggered, this, [this]() {
                if (autosaver_) autosaver_->snapshotNow();
            });
        } else {
            connect(saveSnap, &QAction::triggered, this, [this]() {
                statusBar()->showMessage("Autosave: No project set (VERITY_PROJECT_DIR)", 3000);
            });
        }

        // Hook view toggles
        connect(toggleMD, &QAction::toggled, viewport, [viewport](bool on){ viewport->setForceNoMultiDraw(on); });
        connect(toggleGL, &QAction::toggled, viewport, [viewport](bool on){ viewport->setShowGlInfo(on); });
    }
private:
    std::unique_ptr<verity::AutosaveScheduler> autosaver_;
    std::string projectDir_;
};

int main(int argc, char** argv) {
    // Enable basic MSAA for smoother line joins where supported
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    if (fmt.samples() < 4) fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc, argv);
    const char* proj = std::getenv("VERITY_PROJECT_DIR");
    MainWindow w(proj);
    w.show();
    return app.exec();
}
#endif
