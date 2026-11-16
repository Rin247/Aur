#pragma once

#include <QString>
#include <QTabWidget>
#include <QWidget>
#include "aur/project.hpp"

namespace aur {

namespace bestiary {
class BestiaryEditor;
}

class DatabasePanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QTabWidget* dbTabs = nullptr;
    bestiary::BestiaryEditor* bestiaryEditor = nullptr;

public:
    explicit DatabasePanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshData();
    Q_INVOKABLE void selectPageTab(const QString& name);
};

} // namespace aur
