#pragma once

#include <QWidget>
#include "aur/game.hpp"
#include "aur/project.hpp"

namespace aur::playtest {

bool run_debug_dialog(QWidget* parent, Project& project, Game& game, bool& debugHud);

} // namespace aur::playtest
