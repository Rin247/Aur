#pragma once

#include <QWidget>
#include <string>
#include "aur/json.hpp"

namespace aur {

class CharacterSheetWidget : public QWidget {
    Q_OBJECT
    Json graphic;
    std::string activeFrame = "down_0";
    bool turnaroundMode = true;

public:
    explicit CharacterSheetWidget(QWidget* parent = nullptr);
    void setGraphic(const Json& g);
    void setActiveFrame(const std::string& frameName);
    void setTurnaroundMode(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace aur
