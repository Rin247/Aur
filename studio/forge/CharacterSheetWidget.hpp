#pragma once

#include <QWidget>
#include "aur/json.hpp"

namespace aur {

class CharacterSheetWidget : public QWidget {
    Q_OBJECT
    Json graphic;

public:
    explicit CharacterSheetWidget(QWidget* parent = nullptr);
    void setGraphic(const Json& g);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace aur
