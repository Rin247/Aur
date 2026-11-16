#include "BestiaryEditor.hpp"

#include "BestiaryData.hpp"
#include "CreaturePortrait.hpp"
#include "theme/ResponsiveSplitter.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace aur
{
    namespace bestiary
    {

        BestiaryEditor::BestiaryEditor(Project *value, QWidget *parent)
            : QWidget(parent), project(value)
        {
            auto *splitter = new QSplitter(Qt::Horizontal, this);

            auto *leftPane = new QWidget(splitter);
            auto *leftLayout = new QVBoxLayout(leftPane);
            leftLayout->addWidget(new QLabel("Project creatures", leftPane));
            list = new QListWidget(leftPane);
            list->setMinimumWidth(210);
            leftLayout->addWidget(list, 1);
            auto *listButtons = new QHBoxLayout();
            auto *newButton = new QPushButton("New", leftPane);
            deleteButton = new QPushButton("Delete", leftPane);
            listButtons->addWidget(newButton);
            listButtons->addWidget(deleteButton);
            leftLayout->addLayout(listButtons);

            auto *rightPane = new QWidget(splitter);
            auto *rightLayout = new QVBoxLayout(rightPane);
            rightLayout->addWidget(new QLabel(
                "The creature's own record and artwork. Changes stay in memory until you save "
                "the project.",
                rightPane));
            portrait = new CreaturePortrait(rightPane);
            portrait->setMinimumHeight(140);
            rightLayout->addWidget(portrait, 2);

            auto *scrollArea = new QScrollArea(rightPane);
            scrollArea->setWidgetResizable(true);
            auto *formWidget = new QWidget(scrollArea);
            auto *formLayout = new QFormLayout(formWidget);
            formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            for (const Field &field : fields())
            {
                auto *edit = new QLineEdit(formWidget);
                if (field.numeric)
                    edit->setValidator(new QIntValidator(field.low, field.high, edit));
                edits.emplace_back(field.key, edit);
                formLayout->addRow(QString::fromStdString(field.label) + ":", edit);
            }
            scrollArea->setWidget(formWidget);
            rightLayout->addWidget(scrollArea, 3);

            applyButton = new QPushButton("Apply Creature Changes", rightPane);
            applyButton->setToolTip("Apply the edits to this project in memory. Use the main "
                                    "Save Project action to write them to disk.");
            rightLayout->addWidget(applyButton, 0, Qt::AlignRight);

            splitter->addWidget(leftPane);
            splitter->addWidget(rightPane);
            theme::bindResponsiveSplitter(splitter, {22, 78});

            auto *outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->addWidget(splitter);

            connect(list, &QListWidget::currentRowChanged, this, [this](int) { loadSelected(); });
            connect(newButton, &QPushButton::clicked, this, [this]() { addCreature(); });
            connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteSelected(); });
            connect(applyButton, &QPushButton::clicked, this, [this]() { applyChanges(); });

            refresh();
        }

        void BestiaryEditor::setProject(Project *value)
        {
            project = value;
            portrait->setProject(project);
            refresh();
        }

        std::string BestiaryEditor::selectedId() const
        {
            if (!list || list->currentRow() < 0)
                return std::string();
            QListWidgetItem *item = list->currentItem();
            if (!item)
                return std::string();
            return item->data(Qt::UserRole).toString().toStdString();
        }

        void BestiaryEditor::refresh()
        {
            refreshList();
            loadSelected();
        }

        void BestiaryEditor::refreshList()
        {
            const std::string keep = selectedId();
            list->clear();
            if (!project || !project->db.contains("enemies") ||
                !project->db["enemies"].is_object())
                return;
            for (const auto &[id, record] : project->db["enemies"].as_object())
            {
                auto *item = new QListWidgetItem(
                    QString::fromStdString(label(id, record)), list);
                item->setData(Qt::UserRole, QString::fromStdString(id));
            }
            if (!keep.empty())
            {
                for (int row = 0; row < list->count(); ++row)
                {
                    if (list->item(row)->data(Qt::UserRole).toString().toStdString() == keep)
                    {
                        list->setCurrentRow(row);
                        return;
                    }
                }
            }
            if (list->count() > 0)
                list->setCurrentRow(0);
        }

        void BestiaryEditor::loadSelected()
        {
            const std::string id = selectedId();
            const Json *record = nullptr;
            if (!id.empty() && project && project->db.contains("enemies") &&
                project->db["enemies"].is_object())
            {
                const JsonObject &enemies = project->db["enemies"].as_object();
                const auto entry = enemies.find(id);
                if (entry != enemies.end())
                    record = &entry->second;
            }
            for (const auto &[key, edit] : edits)
                edit->setText(record ? QString::fromStdString(fieldText(*record, key))
                                     : QString());
            const bool has = record != nullptr;
            applyButton->setEnabled(has);
            deleteButton->setEnabled(has);
            if (record)
                portrait->setGraphicId(QString::fromStdString(fieldText(*record, "gfx")));
            else
                portrait->setGraphicId(QString());
        }

        void BestiaryEditor::applyChanges()
        {
            const std::string id = selectedId();
            if (id.empty() || !project)
                return;
            std::vector<std::pair<std::string, std::string>> values;
            for (const auto &[key, edit] : edits)
                values.emplace_back(key, edit->text().toStdString());
            JsonObject &enemies = project->db["enemies"].as_object();
            const auto entry = enemies.find(id);
            if (entry == enemies.end())
                return;
            entry->second = applyFields(entry->second, values);
            refreshList();
            loadSelected();
        }

        void BestiaryEditor::addCreature()
        {
            if (!project)
                return;
            bool ok = false;
            const QString name = QInputDialog::getText(this, "New Creature",
                                                        "Name of the creature:", QLineEdit::Normal,
                                                        QString(), &ok);
            if (!ok || name.trimmed().isEmpty())
                return;
            if (!project->db.contains("enemies") || !project->db["enemies"].is_object())
                project->db["enemies"] = JsonObject{};
            JsonObject &enemies = project->db["enemies"].as_object();
            const std::string id = suggestId(enemies, name.trimmed().toStdString());
            enemies[id] = newRecord(id, name.trimmed().toStdString());
            refreshList();
            for (int row = 0; row < list->count(); ++row)
            {
                if (list->item(row)->data(Qt::UserRole).toString().toStdString() == id)
                {
                    list->setCurrentRow(row);
                    break;
                }
            }
        }

        void BestiaryEditor::deleteSelected()
        {
            const std::string id = selectedId();
            if (id.empty() || !project)
                return;
            if (!project->db.contains("enemies") || !project->db["enemies"].is_object())
                return;
            JsonObject &enemies = project->db["enemies"].as_object();
            if (!enemies.contains(id))
                return;
            enemies.erase(id);
            refresh();
        }

    } // namespace bestiary
} // namespace aur
