#include "events/EventCommandOps.hpp"

namespace aur::events {

QStringList command_type_items() {
    return {
        "say (Dialogue)", "choice (Branching Choice)", "if (Condition)",
        "set_switch (Flag)", "set_var (Variable)", "give_item (Loot)",
        "teleport (Map Transition)", "battle (Combat Trigger)", "set_time (Clock)",
        "music (BGM)", "sfx (Sound Effect)", "toast (Popup)"
    };
}

QStringList default_command_preview() {
    return {
        "1. say (Speaker='Elder', Text='Welcome to Eldervale.')",
        "2. if (cond={night: true}, then=[say text='Rest at the inn.'])",
        "3. give_item (item='moon_shard', qty=1)",
        "4. set_switch (name='met_elder', value=true)"
    };
}

QStringList format_event_commands(const Json& event_doc) {
    if (!event_doc.is_object() || !event_doc.contains("commands") ||
        !event_doc["commands"].is_array() || event_doc["commands"].as_array().empty()) {
        return default_command_preview();
    }
    QStringList out;
    int idx = 1;
    for (const Json& cmd : event_doc["commands"].as_array()) {
        if (cmd.is_object() && cmd.contains("op") && cmd["op"].is_string()) {
            const QString op = QString::fromStdString(cmd["op"].as_string());
            const QString text = cmd.contains("text") && cmd["text"].is_string()
                ? QString::fromStdString(cmd["text"].as_string()) : QString();
            out.append(text.isEmpty() ? QString("%1. %2").arg(idx).arg(op)
                                      : QString("%1. %2 (%3)").arg(idx).arg(op, text));
        } else {
            out.append(QString("%1. command").arg(idx));
        }
        ++idx;
    }
    return out;
}

} // namespace aur::events
