#pragma once

// TimeWeatherBar.hpp - the World header's time and weather controls.
//
// The day, the hour and the kind of weather, in one place: the editor's
// answer to "what does the game show then?". The pick is the engine's,
// so the preview and the game cannot drift.

#include <QWidget>

#include <string>

#include "aur/json.hpp"
#include "aur/project.hpp"

class QComboBox;
class QSpinBox;
class QTimeEdit;

namespace aur
{
    namespace world
    {

        class TimeWeatherBar : public QWidget
        {
            Q_OBJECT
        public:
            explicit TimeWeatherBar(Project *project = nullptr, QWidget *parent = nullptr);

            void setProject(Project *project);
            void setMap(const std::string &map_id);

            // The kind the game rolls at this day and hour, or the forced
            // kind the person chose.
            std::string previewWeather() const;

            int hour() const;
            int minute() const;
            void setTime(int hour, int minute);

        signals:
            void timeChanged(int hour, int minute);
            void weatherChanged(const QString &kind);

        private:
            void refreshWeather();

            Project *project = nullptr;
            std::string map_id;
            QSpinBox *daySpin = nullptr;
            QTimeEdit *timeEdit = nullptr;
            QComboBox *weatherCombo = nullptr;
        };

    } // namespace world
} // namespace aur
