#pragma once

// BestiaryEditor.hpp - the Database page's creatures tab.
//
// The list of the project's creatures, their record, and their artwork.
// The fields the editor does not show are carried through untouched.

#include <QWidget>

#include <string>
#include <utility>
#include <vector>

#include "aur/json.hpp"
#include "aur/project.hpp"

class QLineEdit;
class QListWidget;
class QPushButton;

namespace aur
{
    namespace bestiary
    {

        class CreaturePortrait;

        class BestiaryEditor : public QWidget
        {
            Q_OBJECT
        public:
            explicit BestiaryEditor(Project *project = nullptr, QWidget *parent = nullptr);

            void setProject(Project *project);
            void refresh();

        private:
            void refreshList();
            void loadSelected();
            void applyChanges();
            void addCreature();
            void deleteSelected();
            std::string selectedId() const;

            Project *project = nullptr;
            QListWidget *list = nullptr;
            CreaturePortrait *portrait = nullptr;
            std::vector<std::pair<std::string, QLineEdit *>> edits;
            QPushButton *applyButton = nullptr;
            QPushButton *deleteButton = nullptr;
        };

    } // namespace bestiary
} // namespace aur
