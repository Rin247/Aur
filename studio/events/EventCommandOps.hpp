#pragma once

#include <QStringList>
#include "aur/json.hpp"

namespace aur::events {

QStringList command_type_items();
QStringList default_command_preview();
QStringList format_event_commands(const Json& event_doc);

} // namespace aur::events
