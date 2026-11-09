#include <QApplication>
#include <QMainWindow>
#include "../studio/GameView.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    std::string project_path = (argc > 1) ? argv[1] : "projects/sylvi";

    QMainWindow window;
    window.setWindowTitle("Aur Game Runtime - Sylvi and the Moon Shard");
    window.resize(1280, 720);
    window.setMinimumSize(1280, 720);

    aur::GameView* view = new aur::GameView(project_path, &window);
    window.setCentralWidget(view);
    window.show();
    view->setFocus();

    return app.exec();
}
