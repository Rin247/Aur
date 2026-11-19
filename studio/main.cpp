#include <QApplication>
#include "theme/ThemeManager.hpp"
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStyleHints>
#include <QTimer>
#include "MainWindow.hpp"
#include "core/RhiBackend.hpp"

int main(int argc, char* argv[]) {
    aur::editor::configureRhiBackendBeforeApplication();
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setApplicationName("Aur");
    app.setApplicationDisplayName("Aur");
    app.setOrganizationName("Aur");
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/aur-icon-256.png")));
    aur::theme::ThemeManager::instance().install(app);
    QString project_path;
    QString tab_name;
    QString sub_tab_name;
    QString map_id;
    bool globe = false;
    QString theme_mode;
    qreal zoom_factor = 0.0;
    bool sprite_mode = false;
    bool preview = false;
    bool no_state = false;
    int window_width = 0;
    int window_height = 0;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--tab" && i + 1 < argc) {
            tab_name = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--zoom" && i + 1 < argc) {
            zoom_factor = QString::fromLocal8Bit(argv[++i]).toDouble();
        } else if (arg == "--sub-tab" && i + 1 < argc) {
            sub_tab_name = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--map" && i + 1 < argc) {
            map_id = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--globe") {
            globe = true;
        } else if (arg == "--theme" && i + 1 < argc) {
            theme_mode = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--sprite") {
            sprite_mode = true;
        } else if (arg == "--preview") {
            preview = true;
        } else if (arg == "--no-state") {
            no_state = true;
        } else if (arg == "--size" && i + 1 < argc) {
            const QString size = QString::fromLocal8Bit(argv[++i]).toLower();
            const int x = size.indexOf('x');
            if (x > 0) {
                window_width = size.left(x).toInt();
                window_height = size.mid(x + 1).toInt();
            }
        } else if (project_path.isEmpty()) {
            project_path = arg;
        }
    }
    if (project_path.isEmpty()) {
        const QString working_directory_project =
            QDir::current().filePath("projects/sylvi/project.json");
        const QString executable_directory_project =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("../../projects/sylvi/project.json");
        if (QFileInfo::exists(working_directory_project)) {
            project_path = QFileInfo(working_directory_project).absolutePath();
        } else if (QFileInfo::exists(executable_directory_project)) {
            project_path = QFileInfo(executable_directory_project).absolutePath();
        } else {
            project_path = QDir::current().filePath("projects/sylvi");
        }
    }
    if (!theme_mode.isEmpty())
        aur::theme::ThemeManager::instance().applyTransientMode(theme_mode);
    aur::MainWindow window(project_path.toStdString());
    // Restore state from ~/.Aur unless overridden by CLI flags (--size, --tab, --no-state).
    const bool sized = window_width > 0 && window_height > 0;
    if (no_state) {
        window.setSaveWindowState(false);
        window.restoreWindowState(false, false);
    } else {
        window.restoreWindowState(!sized, tab_name.isEmpty());
    }
    if (!tab_name.isEmpty() && !window.selectTab(tab_name))
        qWarning("Aur: no tab named '%s'", qPrintable(tab_name));
    if (!sub_tab_name.isEmpty()) window.setSubTab(sub_tab_name);
    if (!map_id.isEmpty()) window.setPlaytestMap(map_id);
    if (globe) window.setGlobeView(true);
    if (sprite_mode) window.setSpriteMode(true);
    if (window_width > 0 && window_height > 0) {
        window.resize(window_width, window_height);
        window.show();
    } else {
        window.showMaximized();
    }
    if (zoom_factor > 0.0) {
        // After the show, so the canvas has its real size and the factor is
        // taken against the fit rather than against a stale viewport.
        QTimer::singleShot(0, &window, [&window, zoom_factor] {
            window.setEditorZoom(zoom_factor);
        });
    }
    if (preview) {
        // After the show, so the page has loaded its sequence.
        QTimer::singleShot(250, &window, [&window] { window.startPreview(); });
    }
    return app.exec();
}
