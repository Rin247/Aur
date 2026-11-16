#pragma once

#include <QList>
#include <QPoint>
#include <QWidget>

class QAction;
class QHBoxLayout;
class QMainWindow;
class QMenu;
class QToolButton;

namespace aur
{

    class StudioChrome final : public QWidget
    {
    public:
        explicit StudioChrome(QMainWindow *owner);

        QMenu *fileMenu() const { return file_menu; }
        void setProjectActions(const QList<QAction *> &actions);
        void addTitleWidget(QWidget *widget);
        void setCompactButtons(bool compact);

    protected:
        void paintEvent(QPaintEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;

    private:
        QMainWindow *window = nullptr;
        QPoint dragPosition;
        bool dragging = false;
        QToolButton *maximizeButton = nullptr;
        QMenu *file_menu = nullptr;
        QWidget *project_buttons = nullptr;
        QHBoxLayout *titleRow = nullptr;
        QHBoxLayout *projectLayout = nullptr;
    };

}
