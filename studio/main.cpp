#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStyleHints>
#include <QTimer>
#include "MainWindow.hpp"
#include "RhiBackend.hpp"

int main(int argc, char* argv[]) {
    aur::editor::configureRhiBackendBeforeApplication();
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setApplicationName("Aur");
    app.setApplicationDisplayName("Aur");
    app.setOrganizationName("Aur");
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/aur-icon-256.png")));
    app.setStyleSheet(R"(
        QWidget {
            background-color: #151a22;
            color: #e8e1cf;
            selection-background-color: #b9853f;
            selection-color: #10141a;
        }
        QMainWindow, QDialog, QMenu, QMenuBar, QStatusBar {
            background-color: #11161e;
        }
        QLabel#StudioTitle {
            color: #e8edf2; font-size: 13px; font-weight: 700;
            padding: 2px 6px;
        }
        QMenuBar { border-bottom: 1px solid #39414b; }
        QMenuBar::item { padding: 6px 10px; }
        QMenuBar::item:selected, QMenu::item:selected { background: #39434c; }
        QMenu { border: 1px solid #515a64; }
        QToolBar { background: #1b222c; border: 0; border-bottom: 1px solid #39414b; spacing: 6px; }
        QTabWidget::pane { border: 1px solid #39414b; background: #151a22; }
        QTabBar::tab {
            background: #202832; color: #c8c0ae; padding: 8px 13px;
            border: 1px solid #39414b; border-bottom: 0; margin-right: 2px;
        }
        QTabBar::tab:selected { background: #34434a; color: #f2c879; }
        QTabBar::tab:hover:!selected { background: #2a343e; }
        QPushButton, QToolButton {
            background: #29333c; color: #f0e9d8; border: 1px solid #4b5660;
            border-radius: 4px; padding: 5px 10px;
        }
        QPushButton:hover, QToolButton:hover { background: #3b4a50; border-color: #d3a75d; }
        QPushButton:pressed, QToolButton:pressed { background: #806334; }
        QPushButton:disabled { color: #737b83; background: #20262d; border-color: #353c43; }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: #10151c; color: #eee7d8; border: 1px solid #45505a;
            border-radius: 3px; padding: 4px 6px; selection-background-color: #b9853f;
        }
        QComboBox QAbstractItemView, QListWidget, QTreeWidget, QTableWidget {
            background: #11171f; alternate-background-color: #19212a;
            color: #e8e1cf; border: 1px solid #39444e;
        }
        QListWidget::item, QTreeWidget::item { padding: 4px; }
        QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected {
            background: #725b36; color: #fff3d4;
        }
        QHeaderView::section { background: #252f39; color: #e7d6b3; padding: 5px; border: 0; }
        QGroupBox { border: 1px solid #3c4650; border-radius: 4px; margin-top: 10px; padding-top: 7px; }
        QGroupBox::title { color: #dfbd79; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QSplitter::handle { background: #303943; }
        QScrollBar:vertical, QScrollBar:horizontal { background: #141a21; border: 0; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #47545d; min-height: 24px; min-width: 24px; border-radius: 4px;
        }
        QScrollBar::handle:hover { background: #728078; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QToolTip { background: #252f39; color: #fff3d4; border: 1px solid #d3a75d; padding: 4px; }
    )");
    QString project_path;
    QString tab_name;
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
    aur::MainWindow window(project_path.toStdString());
    // The window's state comes from the data root, unless the command line
    // asked for a size or a page: then the command line wins. A run that
    // must not touch the state at all says --no-state.
    const bool sized = window_width > 0 && window_height > 0;
    if (no_state) {
        window.setSaveWindowState(false);
        window.restoreWindowState(false, false);
    } else {
        window.restoreWindowState(!sized, tab_name.isEmpty());
    }
    if (!tab_name.isEmpty() && !window.selectTab(tab_name))
        qWarning("Aur: no tab named '%s'", qPrintable(tab_name));
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
