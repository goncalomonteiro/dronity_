#ifdef VERITY_QT_SHELL
#include "verity/autosave.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <cstdlib>
#include <memory>

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

        auto* viewport = new QDockWidget("Viewport", this);
        viewport->setWidget(new QLabel("Viewport placeholder"));
        addDockWidget(Qt::LeftDockWidgetArea, viewport);
        setCentralWidget(new QLabel("Scene"));

        // Menu
        auto* fileMenu = menuBar()->addMenu("&File");
        auto* saveSnap = new QAction("Save Snapshot Now", this);
        fileMenu->addAction(saveSnap);

        // Status
        statusBar()->showMessage("Autosave: Off");

        if (projectDirEnv && *projectDirEnv) {
            autosaver_ = std::make_unique<verity::AutosaveScheduler>(projectDirEnv, std::chrono::seconds(60));
            autosaver_->start();
            projectDir_ = projectDirEnv;
            statusBar()->showMessage(QString("Autosave: On — ") + projectDir_.c_str());
            connect(saveSnap, &QAction::triggered, this, [this]() {
                if (autosaver_) autosaver_->snapshotNow();
            });
        } else {
            connect(saveSnap, &QAction::triggered, this, [this]() {
                statusBar()->showMessage("Autosave: No project set (VERITY_PROJECT_DIR)", 3000);
            });
        }
    }
private:
    std::unique_ptr<verity::AutosaveScheduler> autosaver_;
    std::string projectDir_;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const char* proj = std::getenv("VERITY_PROJECT_DIR");
    MainWindow w(proj);
    w.show();
    return app.exec();
}
#endif
