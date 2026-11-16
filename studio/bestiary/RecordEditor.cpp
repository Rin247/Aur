#include "RecordEditor.hpp"

#include "RecordFields.hpp"
#include "theme/ResponsiveSplitter.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
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
    namespace records
    {

        RecordEditor::RecordEditor(Project *value, const QString &record_kind, QWidget *parent)
            : QWidget(parent), project(value), kind(record_kind)
        {
            auto *splitter = new QSplitter(Qt::Horizontal, this);

            auto *leftPane = new QWidget(splitter);
            auto *leftLayout = new QVBoxLayout(leftPane);
            leftLayout->addWidget(new QLabel("Project records", leftPane));
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
                "The record and its fields. Changes stay in memory until you save the project.",
                rightPane));

            auto *scroll = new QScrollArea(rightPane);
            scroll->setWidgetResizable(true);
            formWidget = new QWidget(scroll);
            form = new QFormLayout(formWidget);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            for (const Field &field : fieldsFor(kind.toStdString()))
            {
                if (field.kind == FieldKind::Group || field.kind == FieldKind::Pair)
                {
                    auto *row = new QWidget(formWidget);
                    auto *rowLayout = new QHBoxLayout(row);
                    rowLayout->setContentsMargins(0, 0, 0, 0);
                    for (const auto &[part, high] : field.parts)
                    {
                        auto *edit = new QLineEdit(row);
                        edit->setValidator(new QIntValidator(0, high, edit));
                        rowLayout->addWidget(edit);
                        edits.emplace_back(QString::fromStdString(field.key + "." + part), edit);
                    }
                    rowLayout->addStretch();
                    form->addRow(QString::fromStdString(field.label + ":"), row);
                    continue;
                }
                auto *edit = new QLineEdit(formWidget);
                if (field.kind == FieldKind::Number)
                    edit->setValidator(new QIntValidator(field.low, field.high, edit));
                else if (field.kind == FieldKind::Fraction)
                    edit->setValidator(new QDoubleValidator(0.0, field.high, 2, edit));
                edits.emplace_back(QString::fromStdString(field.key), edit);
                form->addRow(QString::fromStdString(field.label + ":"), edit);
            }
            scroll->setWidget(formWidget);
            rightLayout->addWidget(scroll, 1);

            applyButton = new QPushButton("Apply Changes", rightPane);
            applyButton->setToolTip("Apply the edits to this project in memory. Use the main Save "
                                    "Project action to write them to disk.");
            rightLayout->addWidget(applyButton, 0, Qt::AlignRight);

            splitter->addWidget(leftPane);
            splitter->addWidget(rightPane);
            theme::bindResponsiveSplitter(splitter, {22, 78});
            auto *outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->addWidget(splitter);

            connect(list, &QListWidget::currentRowChanged, this, [this](int) { loadSelected(); });
            connect(newButton, &QPushButton::clicked, this, [this]() { addRecord(); });
            connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteSelected(); });
            connect(applyButton, &QPushButton::clicked, this, [this]() { applyChanges(); });

            refresh();
        }

        void RecordEditor::setProject(Project *value)
        {
            project = value;
            refresh();
        }

        QString RecordEditor::selectedId() const
        {
            if (!list || list->currentRow() < 0)
                return QString();
            QListWidgetItem *item = list->currentItem();
            return item ? item->data(Qt::UserRole).toString() : QString();
        }

        void RecordEditor::refresh()
        {
            refreshList();
            loadSelected();
        }

        void RecordEditor::refreshList()
        {
            const QString keep = selectedId();
            list->clear();
            if (project && project->db.contains(kind.toStdString()) &&
                project->db[kind.toStdString()].is_object())
            {
                for (const auto &[id, record] : project->db[kind.toStdString()].as_object())
                {
                    const QString name = record["name"].is_string()
                                             ? QString::fromStdString(record["name"].as_string())
                                             : QString::fromStdString(id);
                    auto *item = new QListWidgetItem(name, list);
                    item->setData(Qt::UserRole, QString::fromStdString(id));
                }
            }
            for (int row = 0; row < list->count(); ++row)
            {
                if (list->item(row)->data(Qt::UserRole).toString() == keep)
                {
                    list->setCurrentRow(row);
                    return;
                }
            }
            if (list->count() > 0)
                list->setCurrentRow(0);
        }

        void RecordEditor::loadSelected()
        {
            const QString id = selectedId();
            const Json *record = nullptr;
            if (!id.isEmpty() && project && project->db.contains(kind.toStdString()) &&
                project->db[kind.toStdString()].is_object())
            {
                const JsonObject &records = project->db[kind.toStdString()].as_object();
                const auto entry = records.find(id.toStdString());
                if (entry != records.end())
                    record = &entry->second;
            }
            for (const auto &[path, edit] : edits)
            {
                const std::string key = path.toStdString();
                const std::string field_key = key.substr(0, key.find('.') == std::string::npos
                                                              ? key.size()
                                                              : key.find('.'));
                const std::string part = key.size() > field_key.size()
                                             ? key.substr(field_key.size() + 1)
                                             : std::string();
                const Field *field = nullptr;
                for (const Field &candidate : fieldsFor(kind.toStdString()))
                {
                    if (candidate.key == field_key)
                    {
                        field = &candidate;
                        break;
                    }
                }
                edit->setText(record && field
                                  ? QString::fromStdString(fieldText(*record, *field, part))
                                  : QString());
            }
            const bool has = record != nullptr;
            applyButton->setEnabled(has);
            deleteButton->setEnabled(has);
        }

        void RecordEditor::applyChanges()
        {
            const QString id = selectedId();
            if (id.isEmpty() || !project)
                return;
            std::vector<std::pair<std::string, std::string>> values;
            for (const auto &[path, edit] : edits)
                values.emplace_back(path.toStdString(), edit->text().toStdString());
            JsonObject &records = project->db[kind.toStdString()].as_object();
            const auto entry = records.find(id.toStdString());
            if (entry == records.end())
                return;
            entry->second = applyFields(kind.toStdString(), entry->second, values);
            refreshList();
            loadSelected();
        }

        void RecordEditor::addRecord()
        {
            if (!project)
                return;
            bool ok = false;
            const QString name = QInputDialog::getText(this, "New Record",
                                                        "Name of the record:", QLineEdit::Normal,
                                                        QString(), &ok);
            if (!ok || name.trimmed().isEmpty())
                return;
            if (!project->db.contains(kind.toStdString()) ||
                !project->db[kind.toStdString()].is_object())
                project->db[kind.toStdString()] = JsonObject{};
            JsonObject &records = project->db[kind.toStdString()].as_object();
            std::string base;
            for (const char c : name.trimmed().toStdString())
                base.push_back(static_cast<char>(std::isalnum(static_cast<unsigned char>(c))
                                                     ? std::tolower(static_cast<unsigned char>(c))
                                                     : '_'));
            std::string id = base;
            int suffix = 2;
            while (records.contains(id))
                id = base + "_" + std::to_string(suffix++);
            records[id] = newRecord(kind.toStdString(), id, name.trimmed().toStdString());
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

        void RecordEditor::deleteSelected()
        {
            const QString id = selectedId();
            if (id.isEmpty() || !project)
                return;
            JsonObject &records = project->db[kind.toStdString()].as_object();
            if (!records.contains(id.toStdString()))
                return;
            records.erase(id.toStdString());
            refresh();
        }

    } // namespace records
} // namespace aur
