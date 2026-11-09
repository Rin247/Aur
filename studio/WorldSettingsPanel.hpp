#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include "aur/project.hpp"

class QTabWidget;

namespace aur {

class WorldSettingsPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QTabWidget* settingsTabs = nullptr;
    QPlainTextEdit* calendarEdit = nullptr;
    QPlainTextEdit* erasEdit = nullptr;
    QPlainTextEdit* weatherEdit = nullptr;

public:
    explicit WorldSettingsPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshData();

private:
    void applyChanges();
};

} // namespace aur
