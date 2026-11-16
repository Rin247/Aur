#include "StudioChrome.hpp"

#include "theme/LabelRoles.hpp"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace aur
{
    namespace
    {
        QIcon hamburger_icon()
        {
            QIcon icon;
            for (int size : {16, 32})
            {
                QPixmap pixmap(size, size);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(QColor("#c8c0ae"), std::max(1.5, size / 6.0), Qt::SolidLine,
                                    Qt::RoundCap));
                const int margin = size / 5;
                const int gap = size / 3;
                for (int line = 0; line < 3; ++line)
                {
                    const int y = margin + line * gap;
                    painter.drawLine(margin, y, size - margin, y);
                }
                icon.addPixmap(pixmap);
            }
            return icon;
        }
    }

    StudioChrome::StudioChrome(QMainWindow *owner) : QWidget(owner), window(owner)
    {
        setObjectName("StudioChrome");
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 1);
        layout->setSpacing(2);
        titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(4, 0, 0, 0);
        // The file commands live behind a hamburger, so the toolbar can sit
        // under the title instead of under a menu row.
        auto *menuButton = new QToolButton(this);
        menuButton->setIcon(hamburger_icon());
        menuButton->setToolTip("File");
        menuButton->setPopupMode(QToolButton::InstantPopup);
        file_menu = new QMenu(menuButton);
        menuButton->setMenu(file_menu);
        titleRow->addWidget(menuButton);
        auto *titleLabel = new QLabel(owner->windowTitle(), this);
        titleLabel->setObjectName("StudioTitle");
        theme::sectionTitle(titleLabel);
        titleLabel->setContentsMargins(6, 2, 6, 2);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleRow->addWidget(titleLabel);
        project_buttons = new QWidget(this);
        projectLayout = new QHBoxLayout(project_buttons);
        projectLayout->setContentsMargins(4, 0, 4, 0);
        projectLayout->setSpacing(2);
        titleRow->addWidget(project_buttons);
        titleRow->addStretch();
        auto *minimize = new QToolButton(this);
        minimize->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
        minimize->setToolTip("Minimize");
        titleRow->addWidget(minimize);
        maximizeButton = new QToolButton(this);
        maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        maximizeButton->setToolTip("Maximize or restore");
        titleRow->addWidget(maximizeButton);
        auto *close = new QToolButton(this);
        close->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        close->setToolTip("Close Aur");
        close->setProperty("dangerHover", true);
        titleRow->addWidget(close);
        layout->addLayout(titleRow);
        owner->setMenuWidget(this);
        QObject::connect(minimize, &QToolButton::clicked, owner, &QWidget::showMinimized);
        QObject::connect(maximizeButton, &QToolButton::clicked, owner, [owner]() {
            if (owner->isMaximized())
                owner->showNormal();
            else
                owner->showMaximized();
        });
        QObject::connect(close, &QToolButton::clicked, owner, &QWidget::close);
        owner->installEventFilter(this);
    }

    void StudioChrome::setProjectActions(const QList<QAction *> &actions)
    {
        for (QAction *action : actions)
        {
            auto *button = new QToolButton(this);
            button->setDefaultAction(action);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(QSize(16, 16));
            projectLayout->addWidget(button);
        }
    }

    void StudioChrome::addTitleWidget(QWidget *widget)
    {
        titleRow->insertWidget(3, widget);
    }

    void StudioChrome::setCompactButtons(bool compact)
    {
        for (QToolButton *button : project_buttons->findChildren<QToolButton *>())
            button->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly
                                               : Qt::ToolButtonTextBesideIcon);
    }

    void StudioChrome::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    }

    bool StudioChrome::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == window && event->type() == QEvent::WindowStateChange && maximizeButton)
        {
            maximizeButton->setIcon(window->style()->standardIcon(
                window->isMaximized() ? QStyle::SP_TitleBarNormalButton
                                      : QStyle::SP_TitleBarMaxButton));
        }
        return QWidget::eventFilter(watched, event);
    }

    void StudioChrome::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton && window && !window->isMaximized())
        {
            dragging = true;
            dragPosition = event->globalPosition().toPoint() - window->frameGeometry().topLeft();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void StudioChrome::mouseMoveEvent(QMouseEvent *event)
    {
        if (dragging && window && (event->buttons() & Qt::LeftButton) && !window->isMaximized())
        {
            window->move(event->globalPosition().toPoint() - dragPosition);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void StudioChrome::mouseReleaseEvent(QMouseEvent *event)
    {
        dragging = false;
        QWidget::mouseReleaseEvent(event);
    }

    void StudioChrome::mouseDoubleClickEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton && window)
        {
            window->isMaximized() ? window->showNormal() : window->showMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

}
