#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include "aur/project.hpp"
#include "aur/script.hpp"

namespace aur {

class EventPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QListWidget* eventList = nullptr;
    QListWidget* commandList = nullptr;

public:
    explicit EventPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshEvents();
};

} // namespace aur
