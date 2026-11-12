#pragma once

// CreaturePortrait.hpp - the picture of a creature.
//
// The creature's artwork is the project's own graphic, drawn by the
// engine's frame_rgba: the same pixels the game draws. A creature whose
// graphic the project does not have says so, rather than showing
// nothing.

#include <QImage>
#include <QString>
#include <QWidget>

#include "aur/project.hpp"

namespace aur
{
    namespace bestiary
    {

        class CreaturePortrait : public QWidget
        {
            Q_OBJECT
        public:
            explicit CreaturePortrait(QWidget *parent = nullptr);

            void setProject(Project *project);
            void setGraphicId(const QString &id);

        protected:
            void paintEvent(QPaintEvent *event) override;

        private:
            void reload();

            Project *project = nullptr;
            QString graphicId;
            QImage image;
        };

    } // namespace bestiary
} // namespace aur
