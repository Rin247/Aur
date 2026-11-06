#pragma once
#include "aur/json.hpp"
#include "aur/project.hpp"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <optional>
#include <string>

class QPoint;

namespace aur::world {

struct WorldExitTarget {
    std::string map;
    std::string spawn;
};

struct WorldLocationEdits {
    QString id_text;
    int x = 0;
    int y = 0;
};

struct WorldExitEdits {
    std::string target_map;
    std::string target_spawn;
    int x = 0;
    int y = 0;
};

// Add-exit dialog: warns and returns nullopt when the project has no
// target maps; nullopt also means the user cancelled.
std::optional<WorldExitTarget> run_add_exit_dialog(QWidget* parent, const Project& project);

// Edit dialogs return the raw entered values; the caller keeps ownership of
// validation (duplicate IDs, empty target lists) exactly as before.
std::optional<WorldLocationEdits> run_edit_location_dialog(
    QWidget* parent, int map_width, int map_height, const Json& entity);

std::optional<WorldExitEdits> run_edit_exit_dialog(
    QWidget* parent, const Project& project, int map_width, int map_height, const Json& entity);

// Fill the exit-entity placement payload (effect/h/w/target/type/x/y).
Json make_exit_entity(int x, int y, const std::string& target_map, const std::string& spawn);

} // namespace aur::world
