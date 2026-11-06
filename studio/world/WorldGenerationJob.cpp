#include "world/WorldGenerationJob.hpp"

namespace aur::world {

WorldGenerationJob::WorldGenerationJob(QWidget* parent_widget, QObject* parent)
    : QObject(parent), parent_widget_(parent_widget) {}

void WorldGenerationJob::cancel_pending() {
    if (cancelled_) cancelled_->store(true);
    current_source_ = nullptr;
}

void WorldGenerationJob::start(WorldGenerationParams params, const Project* source_project) {
    ++request_;
    if (cancelled_) cancelled_->store(true);
    cancelled_ = std::make_shared<std::atomic_bool>(false);
    current_source_ = source_project;
    const auto cancelled = cancelled_;
    const std::uint64_t request = request_;

    auto* watcher = new QFutureWatcher<Json>(this);
    progress_ = new QProgressDialog(
        QStringLiteral("Generating spherical world\u2026"), QStringLiteral("Cancel"),
        0, 0, parent_widget_);
    progress_->setWindowTitle(QStringLiteral("Generate world"));
    progress_->setWindowModality(Qt::WindowModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    connect(progress_, &QProgressDialog::canceled, this, [cancelled]() {
        cancelled->store(true);
    });

    connect(watcher, &QFutureWatcher<Json>::finished, this,
            [this, watcher, cancelled, request, source_project, id = params.map_id]() {
        progress_->hide();
        if (request == request_ && current_source_ == source_project && !cancelled->load()) {
            try {
                emit generationFinished(QString::fromStdString(id), watcher->result());
            } catch (const std::exception& error) {
                emit generationFailed(QString::fromStdString(error.what()));
            }
        }
        watcher->deleteLater();
        progress_->deleteLater();
        progress_ = nullptr;
    });

    watcher->setFuture(QtConcurrent::run(
        [params = std::move(params), cancelled]() {
            return generate_world_map(params.map_id, params.map_name, params.width,
                params.height, params.seed, params.topology, params.water,
                params.vegetation, params.decay, params.axial_tilt, cancelled.get());
        }));
}

} // namespace aur::world
