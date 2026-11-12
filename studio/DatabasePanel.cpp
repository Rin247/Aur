#include "DatabasePanel.hpp"

#include "bestiary/BestiaryEditor.hpp"

#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>

namespace aur {
namespace {

std::string field_text(const Json& record, const std::string& field) {
    const Json& value = record[field];
    return value.is_string() ? value.as_string() : std::string{};
}

std::string suggested_id(const JsonObject& races) {
    std::string id = "new_race";
    for (int suffix = 2; races.contains(id); ++suffix) {
        id = "new_race_" + std::to_string(suffix);
    }
    return id;
}

} // namespace

DatabasePanel::DatabasePanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    dbTabs = new QTabWidget(this);

    auto createCategoryView = [this](const std::string& catKey, const QStringList& fieldNames) {
        QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
        QListWidget* listWidget = new QListWidget(this);
        QWidget* formContainer = new QWidget(this);
        QFormLayout* formLayout = new QFormLayout(formContainer);

        for (const QString& field : fieldNames) {
            formLayout->addRow(field + ":", new QLineEdit(formContainer));
        }

        splitter->addWidget(listWidget);
        splitter->addWidget(formContainer);
        splitter->setSizes({200, 800});

        if (project && project->db.contains(catKey) && project->db[catKey].is_object()) {
            for (const auto& [id, obj] : project->db[catKey].as_object()) {
                std::string label = id;
                if (obj.contains("name") && obj["name"].is_string()) {
                    label += " (" + obj["name"].as_string() + ")";
                }
                listWidget->addItem(QString::fromStdString(label));
            }
        }

        return splitter;
    };

    dbTabs->addTab(createCategoryView("actors", {"Name", "Char Graphic", "Level", "HP", "MP", "Str", "Def"}), "Actors");
    dbTabs->addTab(createCategoryView("classes", {"Class Name", "Primary Stat", "HP Growth", "MP Growth"}), "Classes");
    dbTabs->addTab(createRaceEditor(), "Races");
    bestiaryEditor = new bestiary::BestiaryEditor(project, this);
    dbTabs->addTab(bestiaryEditor, "Bestiary");
    dbTabs->addTab(createCategoryView("items", {"Item Name", "Type", "Value", "Cost"}), "Items");
    dbTabs->addTab(createCategoryView("weapons", {"Weapon Name", "Might", "Reach", "Cost"}), "Weapons");
    dbTabs->addTab(createCategoryView("skills", {"Skill Name", "MP Cost", "Power", "Radius"}), "Skills");

    mainLayout->addWidget(dbTabs);
    refreshData();
}

QWidget* DatabasePanel::createRaceEditor() {
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* leftPane = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->addWidget(new QLabel("Project races", leftPane));

    raceList = new QListWidget(leftPane);
    raceList->setMinimumWidth(210);
    leftLayout->addWidget(raceList, 1);

    auto* listButtons = new QHBoxLayout();
    auto* newButton = new QPushButton("New", leftPane);
    auto* deleteButton = new QPushButton("Delete", leftPane);
    listButtons->addWidget(newButton);
    listButtons->addWidget(deleteButton);
    leftLayout->addLayout(listButtons);

    auto* rightPane = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->addWidget(new QLabel(
        "Edit the project's race lore and gameplay-facing traits. Changes stay in memory until you save the project.",
        rightPane));

    auto* scrollArea = new QScrollArea(rightPane);
    scrollArea->setWidgetResizable(true);
    auto* formWidget = new QWidget(scrollArea);
    auto* formLayout = new QFormLayout(formWidget);
    raceIdEdit = new QLineEdit(formWidget);
    raceIdEdit->setToolTip("Stable lowercase ID used to refer to this race in project data.");
    raceNameEdit = new QLineEdit(formWidget);
    formLayout->addRow("Race ID:", raceIdEdit);
    formLayout->addRow("Name:", raceNameEdit);

    const std::vector<std::pair<std::string, QString>> fields = {
        {"category", "Category"},
        {"lifespan", "Lifespan"},
        {"maturation", "Maturation"},
        {"habitat", "Habitat"},
        {"appearance", "Appearance"},
        {"adaptations", "Adaptations"},
        {"culture", "Culture and society"},
        {"naming", "Naming"},
        {"combat_role", "Gameplay / combat role"},
        {"notes", "Additional notes"}
    };
    for (const auto& [key, label] : fields) {
        auto* edit = new QPlainTextEdit(formWidget);
        edit->setMinimumHeight(key == "notes" ? 100 : 64);
        raceFields.emplace_back(key, edit);
        formLayout->addRow(label + ":", edit);
    }
    scrollArea->setWidget(formWidget);
    rightLayout->addWidget(scrollArea, 1);

    auto* applyButton = new QPushButton("Apply Race Changes", rightPane);
    applyButton->setToolTip("Apply edits to this project in memory. Use the main Save Project action to write them to disk.");
    rightLayout->addWidget(applyButton, 0, Qt::AlignRight);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    // The list keeps its width; the editor takes the rest of the page.
    leftPane->setMaximumWidth(420);

    connect(raceList, &QListWidget::currentRowChanged, this, [this](int) {
        loadSelectedRace();
    });
    connect(newButton, &QPushButton::clicked, this, [this]() {
        const JsonObject empty;
        raceList->clearSelection();
        raceList->setCurrentRow(-1);
        clearRaceForm();
        raceIdEdit->setText(QString::fromStdString(suggested_id(empty)));
        if (project && project->db.contains("races") && project->db["races"].is_object()) {
            raceIdEdit->setText(QString::fromStdString(suggested_id(project->db["races"].as_object())));
        }
        raceNameEdit->setText("New Race");
        raceNameEdit->setFocus();
    });
    connect(deleteButton, &QPushButton::clicked, this, [this]() {
        deleteSelectedRace();
    });
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        applyRaceChanges();
    });
    return splitter;
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
    if (bestiaryEditor) bestiaryEditor->setProject(p);
    refreshData();
}

void DatabasePanel::refreshData() {
    if (!project) {
        raceList->clear();
        clearRaceForm();
        return;
    }
    if (!project->db.contains("races") || !project->db["races"].is_object()) {
        project->db["races"] = JsonObject{};
    }

    raceList->clear();
    for (const auto& [id, record] : project->db["races"].as_object()) {
        std::string label = id;
        if (record.is_object() && record["name"].is_string() && !record["name"].as_string().empty()) {
            label += " (" + record["name"].as_string() + ")";
        }
        auto* item = new QListWidgetItem(QString::fromStdString(label), raceList);
        item->setData(Qt::UserRole, QString::fromStdString(id));
    }
    clearRaceForm();
}

void DatabasePanel::loadSelectedRace() {
    if (!project || !raceList->currentItem()) {
        clearRaceForm();
        return;
    }
    const std::string id = raceList->currentItem()->data(Qt::UserRole).toString().toStdString();
    const Json& record = project->db["races"][id];
    raceIdEdit->setText(QString::fromStdString(id));
    raceNameEdit->setText(QString::fromStdString(field_text(record, "name")));
    for (const auto& [key, edit] : raceFields) {
        edit->setPlainText(QString::fromStdString(field_text(record, key)));
    }
}

void DatabasePanel::clearRaceForm() {
    raceIdEdit->clear();
    raceNameEdit->clear();
    for (const auto& [key, edit] : raceFields) {
        (void)key;
        edit->clear();
    }
}

void DatabasePanel::applyRaceChanges() {
    if (!project) return;
    const std::string id = raceIdEdit->text().trimmed().toStdString();
    const std::string name = raceNameEdit->text().trimmed().toStdString();
    if (id.empty() || name.empty()) {
        QMessageBox::warning(this, "Race data", "Race ID and name are required.");
        return;
    }
    const bool valid_id = std::all_of(id.begin(), id.end(), [](unsigned char ch) {
        return std::islower(ch) || std::isdigit(ch) || ch == '_';
    });
    if (!valid_id || id.front() == '_' || id.back() == '_' || id.find("__") != std::string::npos) {
        QMessageBox::warning(this, "Race data",
                             "Race ID must use lowercase letters, numbers, and single underscores.");
        return;
    }

    const std::string old_id = raceList->currentItem()
        ? raceList->currentItem()->data(Qt::UserRole).toString().toStdString()
        : std::string{};
    auto& races = project->db["races"].as_object();
    if (races.contains(id) && id != old_id) {
        QMessageBox::warning(this, "Race data", "That race ID is already in use.");
        return;
    }

    Json record = old_id.empty() ? Json(JsonObject{}) : races[old_id];
    record["name"] = name;
    for (const auto& [key, edit] : raceFields) {
        record[key] = edit->toPlainText().toStdString();
    }
    if (!old_id.empty() && id != old_id) races.erase(old_id);
    races[id] = std::move(record);

    refreshData();
    for (int row = 0; row < raceList->count(); ++row) {
        if (raceList->item(row)->data(Qt::UserRole).toString().toStdString() == id) {
            raceList->setCurrentRow(row);
            break;
        }
    }
}

void DatabasePanel::deleteSelectedRace() {
    if (!project || !raceList->currentItem()) return;
    const std::string id = raceList->currentItem()->data(Qt::UserRole).toString().toStdString();
    const auto answer = QMessageBox::question(
        this, "Delete race",
        QString("Delete race '%1'? This cannot be undone.").arg(QString::fromStdString(id)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) return;
    project->db["races"].as_object().erase(id);
    refreshData();
}

} // namespace aur
