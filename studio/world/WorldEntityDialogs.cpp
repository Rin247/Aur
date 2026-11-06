#include "world/WorldEntityDialogs.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>

namespace aur::world {

namespace {

QComboBox* make_target_map_combo(QDialog& dialog, const Project& project) {
    auto* combo = new QComboBox(&dialog);
    for (const auto& [map_id, target] : project.maps) {
        const std::string name = target.contains("name") && target["name"].is_string()
            ? target["name"].as_string() : map_id;
        combo->addItem(QString::fromStdString(name), QString::fromStdString(map_id));
    }
    return combo;
}

Json existing_target(const Json& entity) {
    return entity.contains("target") ? entity["target"] : Json(JsonObject{});
}

} // namespace

std::optional<WorldExitTarget> run_add_exit_dialog(QWidget* parent, const Project& project) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Add world exit"));
    QFormLayout form(&dialog);
    QComboBox* target_map = make_target_map_combo(dialog, project);
    if (target_map->count() == 0) {
        QMessageBox::warning(parent, QStringLiteral("Add world exit"),
                             QStringLiteral("The project has no target maps."));
        return std::nullopt;
    }
    form.addRow(QStringLiteral("Destination map:"), target_map);
    auto* target_spawn = new QLineEdit(&dialog);
    target_spawn->setPlaceholderText(QStringLiteral("Optional spawn ID"));
    form.addRow(QStringLiteral("Destination spawn:"), target_spawn);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    WorldExitTarget target;
    target.map = target_map->currentData().toString().toStdString();
    target.spawn = target_spawn->text().trimmed().toStdString();
    return target;
}

std::optional<WorldLocationEdits> run_edit_location_dialog(
    QWidget* parent, int map_width, int map_height, const Json& entity) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Edit world location"));
    QFormLayout form(&dialog);
    auto* id_edit = new QLineEdit(&dialog);
    if (entity.contains("id") && entity["id"].is_string()) {
        id_edit->setText(QString::fromStdString(entity["id"].as_string()));
    }
    auto* x = new QSpinBox(&dialog);
    auto* y = new QSpinBox(&dialog);
    x->setRange(0, map_width - 1);
    y->setRange(0, map_height - 1);
    x->setValue(entity["x"].as_int());
    y->setValue(entity["y"].as_int());
    form.addRow(QStringLiteral("Location ID:"), id_edit);
    form.addRow(QStringLiteral("X (tiles):"), x);
    form.addRow(QStringLiteral("Y (tiles):"), y);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    WorldLocationEdits edits;
    edits.id_text = id_edit->text().trimmed();
    edits.x = x->value();
    edits.y = y->value();
    return edits;
}

std::optional<WorldExitEdits> run_edit_exit_dialog(
    QWidget* parent, const Project& project, int map_width, int map_height, const Json& entity) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Edit world exit"));
    QFormLayout form(&dialog);
    auto* x = new QSpinBox(&dialog);
    auto* y = new QSpinBox(&dialog);
    x->setRange(0, map_width - 1);
    y->setRange(0, map_height - 1);
    x->setValue(entity["x"].as_int());
    y->setValue(entity["y"].as_int());
    form.addRow(QStringLiteral("X (tiles):"), x);
    form.addRow(QStringLiteral("Y (tiles):"), y);
    QComboBox* target_map = make_target_map_combo(dialog, project);
    const Json target = existing_target(entity);
    if (target.contains("map")) {
        const int target_index = target_map->findData(
            QString::fromStdString(target["map"].as_string()));
        if (target_index >= 0) target_map->setCurrentIndex(target_index);
    }
    auto* target_spawn = new QLineEdit(&dialog);
    if (target.contains("spawn") && target["spawn"].is_string()) {
        target_spawn->setText(QString::fromStdString(target["spawn"].as_string()));
    }
    target_spawn->setPlaceholderText(QStringLiteral("Optional spawn ID"));
    form.addRow(QStringLiteral("Destination map:"), target_map);
    form.addRow(QStringLiteral("Destination spawn:"), target_spawn);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    WorldExitEdits edits;
    edits.target_map = target_map->currentData().toString().toStdString();
    edits.target_spawn = target_spawn->text().trimmed().toStdString();
    edits.x = x->value();
    edits.y = y->value();
    return edits;
}

Json make_exit_entity(int x, int y, const std::string& target_map, const std::string& spawn) {
    JsonObject target{{"map", target_map}};
    if (!spawn.empty()) target["spawn"] = spawn;
    return Json(JsonObject{
        {"effect", "fade"}, {"h", 1}, {"target", Json(target)},
        {"type", "exit"}, {"w", 1}, {"x", x}, {"y", y}
    });
}

} // namespace aur::world
