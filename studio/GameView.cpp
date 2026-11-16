#include "GameView.hpp"
#include "music/SharedAudio.hpp"
#include "music/MasterVolumeStrip.hpp"
#include "playtest/PlaytestDebugDialog.hpp"
#include "theme/CardFrame.hpp"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace aur {

GameView::GameView(const std::string& project_path, QWidget* parent, SharedAudio* sharedAudio)
    : QWidget(parent), project(Project::load(project_path)), game(&project) {
    shared = sharedAudio;
    renderer.project = &project;
    game.layout.w = static_cast<int>(1280.0 / renderer.zoom);
    game.layout.h = static_cast<int>(720.0 / renderer.zoom);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(384, 256);
    QTimer::singleShot(0, this, [this]() { setFocus(); });
    connect(&timer, &QTimer::timeout, [this]() {
        game.update(0.016, input_state);
        syncSong();
        update();
    });
    auto* debugShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    debugShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(debugShortcut, &QShortcut::activated, this, [this]() { openDebugMenu(); });
    if (shared)
        buildAudioOverlay();
    timer.start(16);
}

void GameView::buildAudioOverlay() {
    audioOverlay = new theme::CardFrame(this);
    audioOverlay->setObjectName("playtestAudio");
    auto* row = new QHBoxLayout(audioOverlay);
    row->setContentsMargins(8, 3, 8, 3);
    auto* strip = new MasterVolumeStrip(shared, QStringLiteral("Volume"), audioOverlay);
    strip->setObjectName("playtestVolume");
    row->addWidget(strip);
    soundButton = new QPushButton("Play sound", audioOverlay);
    soundButton->setToolTip("Play the map's song while the playtest is on screen");
    row->addWidget(soundButton);
    connect(soundButton, &QPushButton::clicked, this, [this]() { setSound(!soundOn); });
    audioOverlay->setFocusPolicy(Qt::NoFocus);
    // Anchor the audio card to the bottom-left corner via layout.
    auto* corner = new QVBoxLayout(this);
    corner->setContentsMargins(10, 10, 10, 10);
    corner->addStretch();
    corner->addWidget(audioOverlay, 0, Qt::AlignLeft);
    audioOverlay->show();
}

void GameView::syncSong() {
    // Hidden playtest pages stay silent so they do not interrupt DAW playback.
    if (!shared || !pageVisible || !soundOn) return;
    if (songMapId == game.map_id && shared->output().isPlaying()) return;
    songMapId = game.map_id;
    const Json* map = project.maps.contains(game.map_id) ? &project.maps[game.map_id] : nullptr;
    const Json music = map ? (*map)["music"] : Json();
    const std::string songId = music.is_string() ? music.as_string() : std::string();
    const auto entry = songId.empty() ? project.music.end() : project.music.find(songId);
    if (entry == project.music.end()) {
        shared->output().stop();
        return;
    }
    shared->mixer().resetPlayback();
    shared->mixer().loadSong(entry->second);
    shared->output().start(audio::QtAudioOutput::kSampleRate);
}

void GameView::setSound(bool on) {
    soundOn = on;
    if (soundButton)
        soundButton->setText(on ? "Stop sound" : "Play sound");
    if (!on) {
        if (shared) shared->output().stop();
        songMapId.clear();
        return;
    }
    syncSong();
}

void GameView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    pageVisible = true;
    syncSong();
}

void GameView::hideEvent(QHideEvent* event) {
    pageVisible = false;
    soundOn = false;
    if (soundButton) soundButton->setText("Play sound");
    if (shared) shared->output().stop();
    songMapId.clear();
    QWidget::hideEvent(event);
}

void GameView::reloadProject() {
    loadProject(project.root);
}

void GameView::loadProject(const std::string& project_path) {
    setProject(Project::load(project_path));
}

void GameView::setProject(const Project& source) {
    project = source;
    game = Game(&project);
    game.layout.w = static_cast<int>(1280.0 / renderer.zoom);
    game.layout.h = static_cast<int>(720.0 / renderer.zoom);
    renderer.project = &project;
    renderer.tile_cache.clear();
    renderer.tileset_cache.clear();
    update();
}

void GameView::syncProject(const Project& source) {
    project = source;
    game.refreshProjectSettings();
    renderer.project = &project;
    renderer.tile_cache.clear();
    renderer.tileset_cache.clear();
    update();
}

void GameView::openDebugMenu() {
    input_state.held.clear();
    if (playtest::run_debug_dialog(this, project, game, debugHud)) {
        update();
    }
}

void GameView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Left: case Qt::Key_A: input_state.press("left"); break;
        case Qt::Key_Right: case Qt::Key_D: input_state.press("right"); break;
        case Qt::Key_Up: case Qt::Key_W: input_state.press("up"); break;
        case Qt::Key_Down: case Qt::Key_S: input_state.press("down"); break;
        case Qt::Key_Z: case Qt::Key_Return: case Qt::Key_Space: input_state.press("confirm"); break;
        case Qt::Key_X: case Qt::Key_Escape: input_state.press("cancel"); break;
    }
}

void GameView::keyReleaseEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Left: case Qt::Key_A: input_state.release("left"); break;
        case Qt::Key_Right: case Qt::Key_D: input_state.release("right"); break;
        case Qt::Key_Up: case Qt::Key_W: input_state.release("up"); break;
        case Qt::Key_Down: case Qt::Key_S: input_state.release("down"); break;
        case Qt::Key_Z: case Qt::Key_Return: case Qt::Key_Space: input_state.release("confirm"); break;
        case Qt::Key_X: case Qt::Key_Escape: input_state.release("cancel"); break;
    }
}

void GameView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.fillRect(rect(), Qt::black);
    const qreal sw = 1280.0, sh = 720.0;
    const qreal s = qMin(static_cast<qreal>(width()) / sw, static_cast<qreal>(height()) / sh);
    p.translate(width() / 2.0 - sw * s / 2.0, height() / 2.0 - sh * s / 2.0);
    p.scale(s, s);
    renderer.set_painter(&p);
    game.draw(renderer);
    if (debugHud) {
        const QString era = QString::fromStdString(game.state["current_era"].as_string());
        const QString status = QStringLiteral("%1  \u00b7  Year %2 %3 %4  \u00b7  %5:%6  \u00b7  %7 / %8")
            .arg(era)
            .arg(game.clock.year())
            .arg(QString::fromStdString(game.clock.season()))
            .arg(game.clock.day_of_season())
            .arg(game.clock.hour(), 2, 10, QChar('0'))
            .arg(game.clock.minute(), 2, 10, QChar('0'))
            .arg(QString::fromStdString(game.active_climate))
            .arg(QString::fromStdString(game.weather.current()));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 14, 18, 210));
        p.drawRoundedRect(QRectF(12, 12, 580, 30), 4, 4);
        p.setPen(QColor("#eef0f2"));
        p.drawText(QRectF(22, 12, 560, 30), Qt::AlignVCenter | Qt::AlignLeft, status);
    }
}

} // namespace aur
