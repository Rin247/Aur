#include "theme/AppearanceDialog.hpp"

#include "theme/LabelRoles.hpp"
#include "theme/ThemeManager.hpp"
#include "theme/ThemeQt.hpp"

#include <QButtonGroup>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>

namespace aur::theme
{
    namespace
    {
        constexpr int kCols = 24;
        constexpr int kHsvRows = 24;
        constexpr int kCell = 16;
        constexpr int kGap = 2;
        constexpr int kStride = kCell + kGap;

        constexpr std::array<std::uint32_t, 23> kGreys{{
            0xffffff, 0xf4f4f4, 0xe9e9e9, 0xdedede, 0xd3d3d3, 0xc8c8c8, 0xbebebe, 0xb3b3b3,
            0xa8a8a8, 0x9d9d9d, 0x929292, 0x878787, 0x7c7c7c, 0x717171, 0x666666, 0x5b5b5b,
            0x505050, 0x464646, 0x3b3b3b, 0x303030, 0x252525, 0x1a1a1a, 0x0f0f0f,
        }};

        QColor matrix_color(int row, int col)
        {
            if (row < kHsvRows)
            {
                const qreal h = (col / 24.0);
                const qreal s = 0.40 + (row / 24.0) * 0.60;
                const qreal v = 1.00 - (row / 24.0) * 0.50;
                return QColor::fromHsvF(h, s, v).toRgb();
            }
            if (col < static_cast<int>(kGreys.size()))
                return qt(rgb_from_int(kGreys[static_cast<std::size_t>(col)]));
            return QColor();
        }

        class AccentMatrix final : public QWidget
        {
        public:
            explicit AccentMatrix(QWidget *parent = nullptr) : QWidget(parent)
            {
                setFixedSize(kCols * kStride - kGap, (kHsvRows + 1) * kStride + 4);
                setCursor(Qt::PointingHandCursor);
            }

        protected:
            void paintEvent(QPaintEvent *) override
            {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing, true);
                const QColor selected = ThemeManager::instance().accent().toRgb();
                const QColor ring = qt(tokens().palette.text);
                for (int r = 0; r <= kHsvRows; ++r)
                {
                    const int y = r * kStride + (r == kHsvRows ? 4 : 0);
                    for (int c = 0; c < kCols; ++c)
                    {
                        const QRect cell(c * kStride, y, kCell, kCell);
                        const QColor col = matrix_color(r, c);
                        if (col.isValid())
                        {
                            p.setPen(Qt::NoPen);
                            p.setBrush(col);
                            p.drawRoundedRect(cell, 3, 3);
                            if (col.rgb() == selected.rgb())
                            {
                                p.setBrush(Qt::NoBrush);
                                p.setPen(QPen(ring, 2));
                                p.drawRoundedRect(QRectF(cell).adjusted(1, 1, -1, -1), 2.5, 2.5);
                            }
                        }
                        else
                        {
                            for (int slice = 0; slice < 6; ++slice)
                            {
                                p.setPen(Qt::NoPen);
                                p.setBrush(QColor::fromHsv(slice * 60, 210, 235));
                                p.drawPie(cell.adjusted(1, 1, -1, -1), slice * 60 * 16, 60 * 16);
                            }
                        }
                    }
                }
            }

            void mousePressEvent(QMouseEvent *event) override
            {
                if (event->button() != Qt::LeftButton) return;
                const QPoint pos = event->pos();
                const int col = pos.x() / kStride;
                const int adj_y = pos.y() >= kHsvRows * kStride ? pos.y() - 4 : pos.y();
                const int row = adj_y / kStride;
                if (col < 0 || col >= kCols || row < 0 || row > kHsvRows) return;
                const QColor picked = matrix_color(row, col);
                if (picked.isValid())
                {
                    ThemeManager::instance().setAccent(picked);
                }
                else
                {
                    const QColor custom = QColorDialog::getColor(
                        ThemeManager::instance().accent(), window(), QStringLiteral("Pick Accent"));
                    if (custom.isValid()) ThemeManager::instance().setAccent(custom);
                }
            }
        };
    }

    AppearanceDialog::AppearanceDialog(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Appearance"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 20, 20, 20);
        root->setSpacing(12);

        auto *modeTitle = new QLabel(QStringLiteral("THEME MODE"), this);
        sectionTitle(modeTitle);
        root->addWidget(modeTitle);

        modeGroup_ = new QButtonGroup(this);
        modeGroup_->setExclusive(true);
        auto *grid = new QGridLayout();
        grid->setSpacing(6);

        struct Entry
        {
            QString id;
            QString label;
        };
        QList<Entry> entries{{QStringLiteral("system"), QStringLiteral("System")}};
        for (const Mode &m : modes())
            entries.append({QString::fromUtf8(m.id.data(), static_cast<int>(m.id.size())),
                            QString::fromUtf8(m.name.data(), static_cast<int>(m.name.size()))});

        for (int i = 0; i < entries.size(); ++i)
        {
            auto *btn = new QPushButton(entries[i].label, this);
            btn->setCheckable(true);
            btn->setMinimumHeight(30);
            btn->setProperty("modeId", entries[i].id);
            modeGroup_->addButton(btn, i);
            grid->addWidget(btn, i / 4, i % 4);
            connect(btn, &QPushButton::clicked, this, [id = entries[i].id]() {
                ThemeManager::instance().setMode(id);
            });
        }
        root->addLayout(grid);

        auto *accentTitle = new QLabel(QStringLiteral("ACCENT COLOUR"), this);
        sectionTitle(accentTitle);
        root->addWidget(accentTitle);

        matrix_ = new AccentMatrix(this);
        root->addWidget(matrix_, 0, Qt::AlignLeft);

        auto *hexRow = new QHBoxLayout();
        auto *hexLabel = new QLabel(QStringLiteral("Hex (#rrggbb):"), this);
        dimLabel(hexLabel);
        hexEdit_ = new QLineEdit(this);
        hexEdit_->setMaxLength(7);
        hexEdit_->setFixedWidth(100);
        hexRow->addWidget(hexLabel);
        hexRow->addWidget(hexEdit_);
        hexRow->addStretch();
        auto *closeBtn = new QPushButton(QStringLiteral("Done"), this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        hexRow->addWidget(closeBtn);
        root->addLayout(hexRow);

        connect(hexEdit_, &QLineEdit::textEdited, this, [](const QString &text) {
            Rgb parsed;
            if (parse_hex(text.toUtf8().toStdString(), parsed))
                ThemeManager::instance().setAccent(qt(parsed));
        });
        connect(&ThemeManager::instance(), &ThemeManager::changed, this,
                &AppearanceDialog::syncFromTheme);
        syncFromTheme();
    }

    void AppearanceDialog::syncFromTheme()
    {
        const QString currentMode = ThemeManager::instance().modeId();
        for (QAbstractButton *btn : modeGroup_->buttons())
            btn->setChecked(btn->property("modeId").toString() == currentMode);
        const QString hex = ThemeManager::instance().accent().name();
        if (!hexEdit_->hasFocus() || hexEdit_->text().compare(hex, Qt::CaseInsensitive) != 0)
            hexEdit_->setText(hex);
        if (matrix_) matrix_->update();
    }

}
