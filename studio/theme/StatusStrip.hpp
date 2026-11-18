#pragma once

#include <QString>
#include <QWidget>

class QLabel;

namespace aur::theme
{

    // Page, last posted line, project state and environment; read-only by design.
    class StatusStrip : public QWidget
    {
        Q_OBJECT
    public:
        explicit StatusStrip(QWidget *parent = nullptr);

        void setPage(const QString &page);
        void setProject(const QString &path, bool dirty);
        void setMessage(const QString &text);
        void refreshEnvironment();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;

    signals:
        // A panel posted a line, which means the project may have changed.
        void messagePosted();

    private:
        void applyMessageElide();

        QLabel *pageLabel_ = nullptr;
        QLabel *messageLabel_ = nullptr;
        QLabel *projectLabel_ = nullptr;
        QLabel *environmentLabel_ = nullptr;
        QString fullMessage_;
    };

    // Panels post one line of context here; the strip shows the latest.
    void postStatus(const QString &text);

    // "D3D12", "Vulkan" or "Null", from the configured RHI backend.
    QString backendLabel();

} // namespace aur::theme
