#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <vector>
#include <utility>
#include "aur/project.hpp"

namespace aur {

namespace bestiary {
class BestiaryEditor;
}

class DatabasePanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QTabWidget* dbTabs = nullptr;
    QListWidget* raceList = nullptr;
    bestiary::BestiaryEditor* bestiaryEditor = nullptr;
    QLineEdit* raceIdEdit = nullptr;
    QLineEdit* raceNameEdit = nullptr;
    std::vector<std::pair<std::string, QPlainTextEdit*>> raceFields;

public:
    explicit DatabasePanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshData();
    // Shows one of the page's own tabs (the render sweep uses it).
    Q_INVOKABLE void selectPageTab(const QString& name);

private:
    QWidget* createRaceEditor();
    void loadSelectedRace();
    void clearRaceForm();
    void applyRaceChanges();
    void deleteSelectedRace();
};

} // namespace aur
