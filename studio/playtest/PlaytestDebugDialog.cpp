#include "playtest/PlaytestDebugDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace aur::playtest {

namespace {

void populate_flags_list(QListWidget* flagsList, const Json& switches) {
    flagsList->clear();
    if (!switches.is_object()) return;
    for (const auto& [flag, value] : switches.as_object()) {
        flagsList->addItem(QString("%1 = %2").arg(
            QString::fromStdString(flag),
            value.is_bool() && value.as_bool() ? "true" : "false"));
    }
}

} // namespace

bool run_debug_dialog(QWidget* parent, Project& project, Game& game, bool& debugHud) {
    const bool was_paused = game.clock.paused;
    game.clock.paused = true;
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("In-Game Debug Menu \u00b7 F2"));
    dialog.setMinimumWidth(440);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Live playtest overrides; these do not modify saved project settings.", &dialog));
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto* eraCombo = new QComboBox(&dialog);
    const Json& eras = project.meta["eras"];
    if (eras.is_object()) {
        for (const auto& [id, era] : eras.as_object()) {
            const std::string name = era["name"].is_string() ? era["name"].as_string() : id;
            eraCombo->addItem(QString::fromStdString(name), QString::fromStdString(id));
        }
    }
    for (int i = 0; i < eraCombo->count(); ++i) {
        if (eraCombo->itemData(i).toString().toStdString() ==
            game.state["current_era"].as_string()) eraCombo->setCurrentIndex(i);
    }
    form->addRow("Era:", eraCombo);

    auto* yearSpin = new QSpinBox(&dialog);
    yearSpin->setRange(1, 1000000);
    yearSpin->setValue(game.clock.year());
    form->addRow("Year:", yearSpin);
    auto* seasonCombo = new QComboBox(&dialog);
    for (const std::string& season : game.clock.configured_seasons) {
        seasonCombo->addItem(QString::fromStdString(season));
    }
    seasonCombo->setCurrentIndex(game.clock.season_index());
    form->addRow("Season:", seasonCombo);
    auto* daySpin = new QSpinBox(&dialog);
    daySpin->setRange(1, game.clock.configured_days_per_season);
    daySpin->setValue(game.clock.day_of_season());
    form->addRow("Day:", daySpin);
    auto* hourSpin = new QSpinBox(&dialog);
    hourSpin->setRange(0, static_cast<int>(game.clock.day_minutes / 60.0) - 1);
    hourSpin->setValue(game.clock.hour());
    form->addRow("Hour:", hourSpin);
    auto* minuteSpin = new QSpinBox(&dialog);
    minuteSpin->setRange(0, 59);
    minuteSpin->setValue(game.clock.minute());
    form->addRow("Minute:", minuteSpin);

    auto* rateSpin = new QDoubleSpinBox(&dialog);
    rateSpin->setDecimals(3);
    rateSpin->setRange(0.0, 100.0);
    rateSpin->setSingleStep(0.1);
    rateSpin->setValue(game.clock.scale);
    form->addRow("Game minutes / real second:", rateSpin);
    auto* pausedCheck = new QCheckBox("Keep clock paused after closing", &dialog);
    pausedCheck->setChecked(was_paused);
    form->addRow("Clock:", pausedCheck);
    auto* debugHudCheck = new QCheckBox("Show era / time / weather HUD in playtest", &dialog);
    debugHudCheck->setChecked(debugHud);
    form->addRow("HUD:", debugHudCheck);

    auto* climateCombo = new QComboBox(&dialog);
    climateCombo->addItem("Use map climate", "@map");
    const Json& weatherSettings = project.meta["weather"];
    if (weatherSettings["profiles"].is_object()) {
        for (const auto& [climate, profile] : weatherSettings["profiles"].as_object()) {
            (void)profile;
            climateCombo->addItem(QString::fromStdString(climate));
        }
    }
    if (climateCombo->findText(QString::fromStdString(game.active_climate)) < 0) {
        climateCombo->addItem(QString::fromStdString(game.active_climate));
    }
    if (game.climate_override) {
        climateCombo->setCurrentText(QString::fromStdString(game.active_climate));
    } else {
        climateCombo->setCurrentIndex(0);
    }
    form->addRow("Live climate:", climateCombo);

    auto* weatherCombo = new QComboBox(&dialog);
    weatherCombo->addItems({"clear", "cloudy", "rain", "storm", "snow", "fog"});
    weatherCombo->setCurrentText(QString::fromStdString(game.weather.current()));
    form->addRow("Weather state:", weatherCombo);
    auto* lockWeatherCheck = new QCheckBox("Lock selected weather (disable automatic rolls)", &dialog);
    lockWeatherCheck->setChecked(game.weather.manual_override);
    form->addRow("Weather override:", lockWeatherCheck);
    layout->addLayout(form);

    layout->addWidget(new QLabel("World flags", &dialog));
    auto* flagsList = new QListWidget(&dialog);
    populate_flags_list(flagsList, game.state["switches"]);
    layout->addWidget(flagsList);
    auto* flagRow = new QHBoxLayout();
    auto* flagName = new QLineEdit(&dialog);
    flagName->setPlaceholderText("world_flag_id");
    auto* flagValue = new QComboBox(&dialog);
    flagValue->addItems({"true", "false"});
    auto* setFlagButton = new QPushButton("Set flag", &dialog);
    auto* removeFlagButton = new QPushButton("Remove", &dialog);
    flagRow->addWidget(flagName, 1);
    flagRow->addWidget(flagValue);
    flagRow->addWidget(setFlagButton);
    flagRow->addWidget(removeFlagButton);
    layout->addLayout(flagRow);
    QObject::connect(setFlagButton, &QPushButton::clicked, &dialog, [&game, flagName, flagValue, flagsList]() {
        const QString key = flagName->text().trimmed();
        if (key.isEmpty()) return;
        game.state["switches"][key.toStdString()] = flagValue->currentIndex() == 0;
        populate_flags_list(flagsList, game.state["switches"]);
    });
    QObject::connect(removeFlagButton, &QPushButton::clicked, &dialog, [&game, flagsList]() {
        if (!flagsList->currentItem()) return;
        const QString key = flagsList->currentItem()->text().section(" = ", 0, 0);
        game.state["switches"].as_object().erase(key.toStdString());
        delete flagsList->takeItem(flagsList->currentRow());
    });

    auto* buttons = new QHBoxLayout();
    auto* applyButton = new QPushButton("Apply", &dialog);
    auto* closeButton = new QPushButton("Close", &dialog);
    buttons->addStretch();
    buttons->addWidget(applyButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);
    bool applied = false;
    QObject::connect(applyButton, &QPushButton::clicked, &dialog, [&project, &game, &debugHud, &applied,
            eraCombo, yearSpin, seasonCombo, daySpin, hourSpin, minuteSpin, rateSpin, pausedCheck,
            climateCombo, weatherCombo, lockWeatherCheck, debugHudCheck]() {
        if (eraCombo->count() > 0) {
            game.state["current_era"] = eraCombo->currentData().toString().toStdString();
        }
        game.clock.set_date_time(yearSpin->value(), seasonCombo->currentIndex(),
                                daySpin->value(), hourSpin->value(), minuteSpin->value());
        game.clock.scale = rateSpin->value();
        game.clock.paused = pausedCheck->isChecked();
        if (climateCombo->currentData().toString() == "@map") {
            game.climate_override = false;
            if (project.maps.contains(game.map_id) && project.maps[game.map_id]["climate"].is_string()) {
                game.active_climate = project.maps[game.map_id]["climate"].as_string();
            }
        } else {
            game.active_climate = climateCombo->currentText().toStdString();
            game.climate_override = true;
        }
        debugHud = debugHudCheck->isChecked();
        if (lockWeatherCheck->isChecked()) {
            game.weather.force(weatherCombo->currentText().toStdString());
        } else {
            game.weather.clear_override();
        }
        applied = true;
    });
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    if (!applied) game.clock.paused = was_paused;
    return applied;
}

} // namespace aur::playtest
