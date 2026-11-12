#include "TimeWeatherBar.hpp"

#include "WorldMapHelpers.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimeEdit>

namespace aur
{
    namespace world
    {

        TimeWeatherBar::TimeWeatherBar(Project *value, QWidget *parent)
            : QWidget(parent), project(value)
        {
            auto *row = new QHBoxLayout(this);
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(6);
            // The bar is not squeezed to nothing when the header is full.
            setMinimumWidth(290);

            daySpin = new QSpinBox(this);
            daySpin->setRange(0, 111);
            daySpin->setValue(0);
            daySpin->setPrefix("Day ");
            daySpin->setToolTip("The day of the project's calendar. The weather is rolled per day "
                                "and hour, so the day decides it.");
            row->addWidget(daySpin);

            weatherCombo = new QComboBox(this);
            weatherCombo->addItem("Game's roll", "@roll");
            weatherCombo->addItem("Climate mix", "climate");
            weatherCombo->addItem("Clear", "clear");
            weatherCombo->addItem("Cloudy", "cloudy");
            weatherCombo->addItem("Rain", "rain");
            weatherCombo->addItem("Snow", "snow");
            weatherCombo->addItem("Storm", "storm");
            weatherCombo->addItem("Fog", "fog");
            weatherCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            weatherCombo->setMinimumContentsLength(7);
            weatherCombo->setToolTip(
                "The kind the game rolls at this day and hour, a climate-driven mix across the "
                "map, or a forced condition.");
            row->addWidget(weatherCombo);

            row->addWidget(new QLabel("Time:", this));
            timeEdit = new QTimeEdit(QTime(8, 0), this);
            timeEdit->setDisplayFormat("HH:mm");
            timeEdit->setToolTip("Preview world lighting and terrain shading at this local time.");
            row->addWidget(timeEdit);

            connect(timeEdit, &QTimeEdit::timeChanged, this, [this](const QTime &time) {
                emit timeChanged(time.hour(), time.minute());
                refreshWeather();
            });
            connect(daySpin, &QSpinBox::valueChanged, this, [this](int) { refreshWeather(); });
            connect(weatherCombo, &QComboBox::currentIndexChanged, this, [this](int) {
                refreshWeather();
            });
        }

        void TimeWeatherBar::setProject(Project *value)
        {
            project = value;
            refreshWeather();
        }

        void TimeWeatherBar::setMap(const std::string &id)
        {
            map_id = id;
            refreshWeather();
        }

        int TimeWeatherBar::hour() const
        {
            return timeEdit ? timeEdit->time().hour() : 8;
        }

        int TimeWeatherBar::minute() const
        {
            return timeEdit ? timeEdit->time().minute() : 0;
        }

        void TimeWeatherBar::setTime(int hour, int minute)
        {
            if (!timeEdit)
                return;
            const QSignalBlocker blocker(timeEdit);
            timeEdit->setTime(QTime(hour, minute));
        }

        std::string TimeWeatherBar::previewWeather() const
        {
            const std::string choice = weatherCombo ? weatherCombo->currentData().toString().toStdString()
                                                    : std::string("@roll");
            if (choice != "@roll")
                return choice;
            if (!project)
                return "clear";
            const Json *map = project->maps.contains(map_id) ? &project->maps[map_id] : nullptr;
            if (!map)
                return "clear";
            return rolled_weather(project, *map, daySpin ? daySpin->value() : 0, hour());
        }

        void TimeWeatherBar::refreshWeather()
        {
            emit weatherChanged(QString::fromStdString(previewWeather()));
        }

    } // namespace world
} // namespace aur
