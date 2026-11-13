#pragma once

// RecordEditor.hpp - the generic editor for a kind of record.
//
// The kinds differ only in their fields, so the editor is generic and
// the fields are a table (studio/bestiary/RecordFields.*).

#include <QString>
#include <QWidget>

#include <utility>
#include <vector>

#include "aur/json.hpp"
#include "aur/project.hpp"

class QFormLayout;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace aur
{
    namespace records
    {

        class RecordEditor : public QWidget
        {
            Q_OBJECT
        public:
            explicit RecordEditor(Project *project, const QString &kind,
                                  QWidget *parent = nullptr);

            void setProject(Project *project);
            void refresh();

        private:
            void refreshList();
            void loadSelected();
            void applyChanges();
            void addRecord();
            void deleteSelected();
            QString selectedId() const;

            Project *project = nullptr;
            QString kind;
            QListWidget *list = nullptr;
            QWidget *formWidget = nullptr;
            QFormLayout *form = nullptr;
            std::vector<std::pair<QString, QLineEdit *>> edits;
            QPushButton *applyButton = nullptr;
            QPushButton *deleteButton = nullptr;
        };

    } // namespace records
} // namespace aur
