#include "world/WorldSettingsPanel.hpp"

#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <cmath>
#include <set>

namespace aur {
namespace {

QPlainTextEdit* jsonEditor(QWidget* parent) {
    auto* editor = new QPlainTextEdit(parent);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    return editor;
}

bool valid_calendar(const Json& value, std::string& error) {
    if (!value.is_object()) {
        error = "Calendar settings must be a JSON object.";
        return false;
    }
    const Json& hours = value["hours_per_day"];
    const Json& start_hour = value["start_hour"];
    const Json& real_minutes = value["real_day_minutes"];
    const Json& days = value["days_per_season"];
    const Json& seasons = value["seasons"];
    const Json& phases = value["phase_hours"];
    if (!hours.is_number() || hours.as_number() < 1 || hours.as_number() > 48 ||
        !start_hour.is_number() || start_hour.as_number() < 0 ||
        start_hour.as_number() >= hours.as_number() ||
        !real_minutes.is_number() || real_minutes.as_number() < 1 || real_minutes.as_number() > 10080 ||
        !days.is_number() || days.as_int() < 1 || days.as_int() > 365 ||
        !seasons.is_array() || seasons.as_array().empty() || seasons.as_array().size() > 16 ||
        !phases.is_array() || phases.as_array().size() != 3) {
        error = "Calendar requires hours_per_day (1-48), start_hour within the day, real_day_minutes (1-10080), days_per_season (1-365), 1-16 season names, and three phase_hours.";
        return false;
    }
    std::set<std::string> season_names;
    for (const Json& season : seasons.as_array()) {
        if (!season.is_string() || season.as_string().empty() ||
            !season_names.insert(season.as_string()).second) {
            error = "Season names must be non-empty and unique.";
            return false;
        }
    }
    double previous = 0.0;
    for (const Json& phase : phases.as_array()) {
        if (!phase.is_number() || phase.as_number() <= previous ||
            phase.as_number() >= hours.as_number()) {
            error = "Phase boundaries must be numeric, strictly increasing, and below hours_per_day.";
            return false;
        }
        previous = phase.as_number();
    }
    return true;
}

bool valid_eras(const Json& value, std::string& error) {
    if (!value.is_object() || value.as_object().empty()) {
        error = "Eras must be a non-empty JSON object keyed by stable era IDs.";
        return false;
    }
    for (const auto& [id, era] : value.as_object()) {
        if (id.empty() || !era.is_object() || !era["name"].is_string() ||
            era["name"].as_string().empty()) {
            error = "Every era needs a stable ID and a non-empty name.";
            return false;
        }
    }
    return true;
}

bool valid_weather(const Json& value, std::string& error) {
    static const std::set<std::string> weather_states = {
        "clear", "cloudy", "rain", "storm", "snow", "fog"
    };
    if (!value.is_object() || !value["interval_hours"].is_number() ||
        value["interval_hours"].as_int() < 1 || value["interval_hours"].as_int() > 24 ||
        !value["profiles"].is_object() || value["profiles"].as_object().empty()) {
        error = "Weather needs interval_hours (1-24) and at least one climate profile.";
        return false;
    }
    for (const auto& [climate, profile] : value["profiles"].as_object()) {
        if (climate.empty() || !profile.is_object() || profile.as_object().empty()) {
            error = "Each weather climate profile must be a non-empty object.";
            return false;
        }
        int total = 0;
        for (const auto& [state, weight] : profile.as_object()) {
            if (!weather_states.contains(state) || !weight.is_number() ||
                weight.as_number() < 0 || std::floor(weight.as_number()) != weight.as_number() ||
                weight.as_number() > 100000) {
                error = "Weather profiles use clear, cloudy, rain, storm, snow, or fog with integer weights from 0 to 100000.";
                return false;
            }
            total += weight.as_int();
        }
        if (total <= 0) {
            error = "Each climate profile needs at least one positive weather weight.";
            return false;
        }
    }
    return true;
}

} // namespace

WorldSettingsPanel::WorldSettingsPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        "Edit project calendar, era definitions, and climate-based weather probabilities as JSON. Era-specific map variants and timegate behavior are not wired yet. Apply validates all sections; Save Project writes them to disk.",
        this));

    settingsTabs = new QTabWidget(this);
    calendarEdit = jsonEditor(settingsTabs);
    erasEdit = jsonEditor(settingsTabs);
    weatherEdit = jsonEditor(settingsTabs);
    settingsTabs->addTab(calendarEdit, "Calendar & Time");
    settingsTabs->addTab(erasEdit, "Eras & World States");
    settingsTabs->addTab(weatherEdit, "Weather");
    layout->addWidget(settingsTabs, 1);

    auto* applyButton = new QPushButton("Apply World Settings", this);
    applyButton->setToolTip("Validate and apply all three JSON sections to the project in memory.");
    layout->addWidget(applyButton, 0, Qt::AlignRight);
    connect(applyButton, &QPushButton::clicked, this, [this]() { applyChanges(); });
    refreshData();
}

void WorldSettingsPanel::setProject(Project* p) {
    project = p;
    refreshData();
}

void WorldSettingsPanel::refreshData() {
    if (!project) {
        calendarEdit->clear();
        erasEdit->clear();
        weatherEdit->clear();
        return;
    }
    calendarEdit->setPlainText(QString::fromStdString(dump_json(project->meta["calendar"], 2)));
    erasEdit->setPlainText(QString::fromStdString(dump_json(project->meta["eras"], 2)));
    weatherEdit->setPlainText(QString::fromStdString(dump_json(project->meta["weather"], 2)));
}

void WorldSettingsPanel::applyChanges() {
    if (!project) return;
    Json calendar;
    Json eras;
    Json weather;
    try {
        calendar = parse_json(calendarEdit->toPlainText().toStdString());
        eras = parse_json(erasEdit->toPlainText().toStdString());
        weather = parse_json(weatherEdit->toPlainText().toStdString());
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "World settings", QString("Invalid JSON: ") + error.what());
        return;
    }

    std::string error;
    if (!valid_calendar(calendar, error) || !valid_eras(eras, error) ||
        !valid_weather(weather, error)) {
        QMessageBox::warning(this, "World settings", QString::fromStdString(error));
        return;
    }

    project->meta["calendar"] = std::move(calendar);
    project->meta["start_hour"] = project->meta["calendar"]["start_hour"];
    project->meta["eras"] = std::move(eras);
    if (!project->meta["eras"].contains(project->meta["current_era"].as_string())) {
        project->meta["current_era"] = project->meta["eras"].as_object().begin()->first;
    }
    project->meta["weather"] = std::move(weather);
    QMessageBox::information(this, "World settings", "Calendar, eras, and weather settings applied. Save the project to keep them.");
}

} // namespace aur
