#include <QApplication>
#include <QMainWindow>
#include "../studio/GameView.hpp"
#include "../studio/SharedAudio.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    std::string project_path = (argc > 1) ? argv[1] : "projects/sylvi";

    QMainWindow window;
    window.setWindowTitle("Aur Game Runtime - Sylvi and the Moon Shard");
    window.resize(1280, 720);
    window.setMinimumSize(1280, 720);

    // The runtime plays the map's song, like the studio's playtest does.
    aur::SharedAudio* audio = new aur::SharedAudio(&window);
    aur::GameView* view = new aur::GameView(project_path, &window, audio);
    window.setCentralWidget(view);
    window.show();
    view->setFocus();

    return app.exec();
}
