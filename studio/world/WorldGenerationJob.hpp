#pragma once
#include "aur/json.hpp"
#include "aur/project.hpp"

#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QWidget>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace aur::world {

struct WorldGenerationParams {
    std::string map_id;
    std::string map_name;
    int width = 256;
    int height = 128;
    int seed = 1;
    std::string topology = "continents";
    int water = 50;
    int vegetation = 65;
    int decay = 15;
    int axial_tilt = 0;
};

// Runs generate_world_map on a worker thread with stale-request/cancel
// tracking; emits generationFinished(map_id, json) only for the newest
// request of a still-current project that was not cancelled.
class WorldGenerationJob : public QObject {
    Q_OBJECT

public:
    explicit WorldGenerationJob(QWidget* parent_widget, QObject* parent = nullptr);

    void start(WorldGenerationParams params, const Project* source_project);
    void cancel_pending();

signals:
    void generationFinished(QString map_id, aur::Json map);
    void generationFailed(QString message);

private:
    QWidget* parent_widget_ = nullptr;
    QProgressDialog* progress_ = nullptr;
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::uint64_t request_ = 0;
    const Project* current_source_ = nullptr;
};

} // namespace aur::world
