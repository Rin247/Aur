#include "CreatureForgePanel.hpp"
#include "theme/LabelRoles.hpp"

#include <QColor>
#include <QColorDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <vector>

namespace aur {
namespace {

struct SwatchSlot {
    const char* key;
    const char* label;
};

constexpr SwatchSlot kSwatchSlots[] = {
    {"skin", "Skin"}, {"hair", "Hair"}, {"eye", "Eye"},
    {"top", "Top"}, {"bottom", "Bottom"}, {"accent", "Accent"}
};

} // namespace

void CreatureForgePanel::buildColorSwatches(QWidget* body, QVBoxLayout* layout) {
    auto* group = new QGroupBox("Palette Overrides", body);
    auto* grid = new QGridLayout(group);
    int index = 0;
    for (const SwatchSlot& slot : kSwatchSlots) {
        const std::string key = slot.key;
        auto* button = new QPushButton(QString::fromLatin1(slot.label), group);
        colorButtons[key] = button;
        grid->addWidget(button, index / 3, index % 3);
        ++index;
        connect(button, &QPushButton::clicked, this, [this, key]() {
            const QString current = previewSpec.contains("colors") &&
                                    previewSpec["colors"].contains(key)
                ? QString::fromStdString(previewSpec["colors"][key].as_string())
                : QStringLiteral("#ffffff");
            const QColor picked = QColorDialog::getColor(
                QColor(current), this, QString("Pick %1 colour").arg(QString::fromStdString(key)));
            if (!picked.isValid()) return;
            colorOverrides[key] = picked.name(QColor::HexRgb).toStdString();
            generateRandomNPC();
        });
    }
    auto* reset = new QPushButton("Reset Palette to Seed", group);
    grid->addWidget(reset, 2, 0, 1, 3);
    connect(reset, &QPushButton::clicked, this, [this]() {
        colorOverrides.clear();
        generateRandomNPC();
    });
    layout->addWidget(group);
}

void CreatureForgePanel::refreshColorSwatches() {
    if (!previewSpec.contains("colors") || !previewSpec["colors"].is_object()) return;
    for (const auto& [key, button] : colorButtons) {
        if (!button || !previewSpec["colors"].contains(key)) continue;
        const std::string hex = previewSpec["colors"][key].as_string();
        QPixmap swatch(14, 14);
        swatch.fill(QColor(QString::fromStdString(hex)));
        button->setIcon(QIcon(swatch));
    }
}

} // namespace aur
