#ifdef VERITY_QT_SHELL
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>

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
    return app.exec();
}
#endif

