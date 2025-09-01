#ifdef VERITY_QT_SHELL
#include "verity/autosave.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <cstdlib>
#include <memory>

class MainWindow : public QMainWindow {
public:
    MainWindow() {
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
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    // Optional autosave if VERITY_PROJECT_DIR is provided
    std::unique_ptr<verity::AutosaveScheduler> autosaver;
    if (const char* proj = std::getenv("VERITY_PROJECT_DIR")) {
        autosaver = std::make_unique<verity::AutosaveScheduler>(proj, std::chrono::seconds(60));
        autosaver->start();
    }
    return app.exec();
}
#endif
