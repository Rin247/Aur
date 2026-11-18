#include "theme/StatusStrip.hpp"
#include "theme/LabelRoles.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeMetrics.hpp"
#include "core/RhiBackend.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>

namespace aur::theme
{

namespace
{
    StatusStrip *g_strip = nullptr;
}

StatusStrip::StatusStrip(QWidget *parent) : QWidget(parent)
{
    setObjectName("StatusStrip");
    setFixedHeight(kControlHeight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(12);

    pageLabel_ = new QLabel(this);
    QFont pageFont = pageLabel_->font();
    pageFont.setBold(true);
    pageLabel_->setFont(pageFont);

    messageLabel_ = new QLabel(this);
    messageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    messageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    projectLabel_ = new QLabel(this);
    environmentLabel_ = new QLabel(this);
    projectLabel_->setForegroundRole(QPalette::PlaceholderText);
    environmentLabel_->setForegroundRole(QPalette::PlaceholderText);

    layout->addWidget(pageLabel_);
    layout->addWidget(messageLabel_, 1);
    layout->addWidget(projectLabel_);
    layout->addWidget(environmentLabel_);

    g_strip = this;
    refreshEnvironment();
}

void StatusStrip::setPage(const QString &page)
{
    pageLabel_->setText(page);
}

void StatusStrip::setProject(const QString &path, bool dirty)
{
    QString name = path;
    const int slash = std::max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
    if (slash >= 0) name = name.mid(slash + 1);
    if (name.isEmpty()) name = "untitled";
    projectLabel_->setText(dirty ? name + QStringLiteral(" \u00b7 unsaved")
                                 : name + QStringLiteral(" \u00b7 saved"));
}

void StatusStrip::setMessage(const QString &text)
{
    fullMessage_ = text;
    applyMessageElide();
    emit messagePosted();
}

void StatusStrip::refreshEnvironment()
{
    ThemeManager &manager = ThemeManager::instance();
    environmentLabel_->setText(QStringLiteral("%1 \u00b7 %2 \u00b7 %3%")
                                   .arg(backendLabel(), manager.modeId())
                                   .arg(manager.uiScale()));
}

void StatusStrip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, 0, width(), 0);
}

void StatusStrip::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyMessageElide();
}

void StatusStrip::applyMessageElide()
{
    const int available = messageLabel_->width();
    if (available <= 0)
    {
        messageLabel_->setText(fullMessage_);
        return;
    }
    messageLabel_->setText(
        messageLabel_->fontMetrics().elidedText(fullMessage_, Qt::ElideRight, available));
}

void postStatus(const QString &text)
{
    if (g_strip) g_strip->setMessage(text);
}

QString backendLabel()
{
    switch (editor::configuredRhiWidgetApi())
    {
    case QRhiWidget::Api::Vulkan: return QStringLiteral("Vulkan");
    case QRhiWidget::Api::Direct3D12: return QStringLiteral("D3D12");
    case QRhiWidget::Api::Null: return QStringLiteral("Null");
    default: return QStringLiteral("RHI");
    }
}

} // namespace aur::theme
