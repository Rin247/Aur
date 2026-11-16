#include "DatabasePanel.hpp"

#include "bestiary/BestiaryEditor.hpp"
#include "bestiary/RecordEditor.hpp"

#include <QVBoxLayout>

namespace aur {

DatabasePanel::DatabasePanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* mainLayout = new QVBoxLayout(this);
    dbTabs = new QTabWidget(this);

    dbTabs->addTab(new records::RecordEditor(project, "actors", this), "Actors");
    dbTabs->addTab(new records::RecordEditor(project, "classes", this), "Classes");
    dbTabs->addTab(new records::RecordEditor(project, "races", this), "Races");
    bestiaryEditor = new bestiary::BestiaryEditor(project, this);
    dbTabs->addTab(bestiaryEditor, "Bestiary");
    dbTabs->addTab(new records::RecordEditor(project, "items", this), "Items");
    dbTabs->addTab(new records::RecordEditor(project, "weapons", this), "Weapons");
    dbTabs->addTab(new records::RecordEditor(project, "skills", this), "Skills");

    mainLayout->addWidget(dbTabs);
}

void DatabasePanel::selectPageTab(const QString& name) {
    if (!dbTabs) return;
    for (int index = 0; index < dbTabs->count(); ++index) {
        if (dbTabs->tabText(index) == name) {
            dbTabs->setCurrentIndex(index);
            return;
        }
    }
}

void DatabasePanel::setProject(Project* p) {
    project = p;
    refreshData();
}

void DatabasePanel::refreshData() {
    for (auto* editor : findChildren<records::RecordEditor*>()) editor->setProject(project);
    if (bestiaryEditor) bestiaryEditor->setProject(project);
}

} // namespace aur
