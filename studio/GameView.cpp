#include "SharedAudio.hpp"
#include "theme/CardFrame.hpp"
#include "GameView.hpp"
#include "aur/gfx.hpp"
#include "TileTexture.hpp"
#include "music/MasterVolumeStrip.hpp"

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
#include <QKeySequence>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace aur {

void QtRenderer::set_painter(QPainter* p) { painter = p; }

void QtRenderer::begin(const std::string& st) {
    Renderer::begin(st);
    if (painter) {
        painter->save();
        painter->scale(zoom, zoom);
        w = static_cast<int>(1280.0 / zoom);
        h = static_cast<int>(720.0 / zoom);
    }
}

void QtRenderer::end() {
    if (painter) painter->restore();
}

void QtRenderer::camera(int x, int y) {
    if (painter) painter->translate(-x, -y);
}

static QColor tile_color(int idx) {
    switch (idx) {
        case 0: return QColor("#2e7d32"); // grass
        case 1: return QColor("#8d6e63"); // path
        case 2: return QColor("#1565c0"); // water
        case 3: return QColor("#66bb6a"); // bush
        default: return QColor("#37474f");
    }
}

QImage QtRenderer::graphic_image(const std::string& gid, const std::string& frame) {
    auto key = std::make_pair(gid, frame);
    auto it = tile_cache.find(key);
    if (it != tile_cache.end()) return it->second;
    QImage img;
    if (project) {
        Json g = project->graphic(gid);
        auto [w, h, data] = frame_rgba(g, frame);
        if (w > 0 && h > 0 && !data.empty()) {
            img = QImage(data.data(), w, h, QImage::Format_RGBA8888).convertToFormat(QImage::Format_ARGB32);
        }
    }
    tile_cache[key] = img;
    return img;
}

void QtRenderer::tile(const std::string& tileset, int index, int x, int y, const std::string&) {
    if (!painter) return;
    const auto key = std::make_pair(tileset, index);
    auto it = tileset_cache.find(key);
    if (it == tileset_cache.end() && project) {
        it = tileset_cache.emplace(key, make_tileset_tile(*project, tileset, index)).first;
    }
    const QImage img = it == tileset_cache.end() ? QImage{} : it->second;
    if (!img.isNull()) {
        const QImage scaled = img.scaled(field_tile_size, field_tile_size,
                                         Qt::IgnoreAspectRatio, Qt::FastTransformation);
        painter->drawImage(x, y, scaled);
    } else {
        painter->fillRect(x, y, field_tile_size, field_tile_size, tile_color(index));
        painter->setPen(QColor("#222222"));
        painter->drawRect(x, y, field_tile_size, field_tile_size);
    }
}

void QtRenderer::sprite(const std::string& graphic, const std::string& frame, int x, int y, bool flip, double scale,
                        const std::string& tint, double alpha, bool shadow, int z) {
    if (!painter) return;
    QImage img = graphic_image(graphic, frame);
    if (img.isNull()) {
        QColor c = tint.empty() ? QColor("#ffb300") : QColor(QString::fromStdString(tint));
        c.setAlphaF(alpha);
        painter->fillRect(x, y, (int)(16 * scale), (int)(16 * scale), c);
        painter->setPen(Qt::black);
        painter->drawRect(x, y, (int)(16 * scale), (int)(16 * scale));
        return;
    }
    if (flip) {
        img = img.mirrored(true, false);
    }
    QSize scaled((int)(img.width() * scale), (int)(img.height() * scale));
    img = img.scaled(scaled, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (alpha < 1.0) {
        QImage tmp(img.size(), QImage::Format_ARGB32);
        tmp.fill(Qt::transparent);
        QPainter tp(&tmp);
        tp.setOpacity(alpha);
        tp.drawImage(0, 0, img);
        img = tmp;
    }
    painter->drawImage(x, y, img);
}

void QtRenderer::ground_rect(int x, int y, int w, int h, const std::string& color, double alpha) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#000000") : QColor(QString::fromStdString(color));
    c.setAlphaF(alpha);
    painter->fillRect(x, y, w, h, c);
}

void QtRenderer::world_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color) {
    if (!painter) return;
    painter->setPen(QColor(std::get<0>(color), std::get<1>(color), std::get<2>(color)));
    painter->drawText(x, y + 12, QString::fromStdString(text));
}

void QtRenderer::arc(int x, int y, int radius, double angle, double span_deg,
                     const std::tuple<int,int,int>& color, double alpha) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(alpha);
    painter->setPen(QPen(c, 3));
    painter->drawArc(x - radius, y - radius, radius * 2, radius * 2,
                     (int)(angle * 16 * 180 / 3.14159265), (int)(span_deg * 16));
}

void QtRenderer::ring(int x, int y, int radius, const std::tuple<int,int,int>& color, double alpha) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(alpha);
    painter->setPen(QPen(c, 3));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void QtRenderer::light(int x, int y, int radius, const std::tuple<int,int,int>& color, double intensity) {
    if (!painter) return;
    QColor c(std::get<0>(color), std::get<1>(color), std::get<2>(color));
    c.setAlphaF(0.35 * intensity);
    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    painter->drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void QtRenderer::ambient(const std::string& color, double darkness) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#ffffff") : QColor(QString::fromStdString(color));
    c.setAlphaF(std::clamp(darkness, 0.0, 1.0));
    painter->save();
    painter->resetTransform();
    painter->fillRect(painter->viewport(), c);
    painter->restore();
}

void QtRenderer::weather(const std::string& kind, double intensity, double t, double flash) {
    if (!painter || kind == "clear") return;
    painter->save();
    painter->resetTransform();
    const QRect area = painter->viewport();
    const int strength = qBound(0, static_cast<int>(intensity * 255.0), 255);
    if (kind == "cloudy" || kind == "storm") {
        painter->fillRect(area, QColor(35, 44, 58, strength / 5));
    }
    if (kind == "rain" || kind == "storm") {
        painter->fillRect(area, QColor(35, 50, 72, strength / 10));
        painter->setPen(QPen(QColor(170, 205, 235, strength / 2), kind == "storm" ? 2 : 1));
        const int dropCount = kind == "storm" ? 240 : 180;
        for (int i = 0; i < dropCount; ++i) {
            const int x = (i * 97 + static_cast<int>(t * 420.0)) % qMax(1, area.width());
            const int y = (i * 53 + static_cast<int>(t * 760.0)) % qMax(1, area.height());
            painter->drawLine(x, y, x - 5, y + 14);
        }
    } else if (kind == "snow") {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(238, 245, 255, strength * 3 / 4));
        for (int i = 0; i < 90; ++i) {
            const int x = (i * 113 + static_cast<int>(std::sin(t + i) * 24.0)) % qMax(1, area.width());
            const int y = (i * 67 + static_cast<int>(t * (18.0 + i % 9))) % qMax(1, area.height());
            painter->drawEllipse(QPoint(x, y), 2 + i % 2, 2 + i % 2);
        }
    } else if (kind == "fog") {
        painter->fillRect(area, QColor(185, 198, 205, strength / 4));
    }
    if (flash > 0.0) {
        painter->fillRect(area, QColor(245, 248, 255, qBound(0, static_cast<int>(flash * 180.0), 180)));
    }
    painter->restore();
}

void QtRenderer::ui_rect(int x, int y, int w, int h, const std::string& color, double alpha, const std::string&) {
    if (!painter) return;
    QColor c = color.empty() ? QColor("#000000") : QColor(QString::fromStdString(color));
    c.setAlphaF(alpha);
    painter->fillRect(x, y, w, h, c);
}

void QtRenderer::ui_graphic(const std::string&, const std::string&, int x, int y, int scale) {
    if (!painter) return;
    painter->fillRect(x, y, 16 * scale, 16 * scale, QColor("#9e9e9e"));
}

void QtRenderer::fade(double, const std::tuple<int,int,int>&) {}
void QtRenderer::flash(double, const std::tuple<int,int,int>&) {}

void QtRenderer::ui_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color, bool shadow) {
    if (!painter) return;
    painter->setPen(QColor(std::get<0>(color), std::get<1>(color), std::get<2>(color)));
    painter->drawText(x, y + 12, QString::fromStdString(text));
}

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
    // The corner is the layout's job: a hand-placed widget is measured
    // before the page has its real size, and lands off the bottom.
    auto* corner = new QVBoxLayout(this);
    corner->setContentsMargins(10, 10, 10, 10);
    corner->addStretch();
    corner->addWidget(audioOverlay, 0, Qt::AlignLeft);
    audioOverlay->show();
}

void GameView::syncSong() {
    // The hidden page does not touch the device: the timer keeps ticking
    // behind the other tabs, and it must not restart the music there.
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
    // Leaving the playtest stops the music, so the DAW's preview is not
    // played over it.
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
    const bool was_paused = game.clock.paused;
    game.clock.paused = true;
    input_state.held.clear();
    QDialog dialog(this);
    dialog.setWindowTitle("In-Game Debug Menu · F2");
    dialog.setMinimumWidth(440);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Live playtest overrides; these do not modify saved project settings.", &dialog));
    auto* form = new QFormLayout();

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
    if (game.state["switches"].is_object()) {
        for (const auto& [flag, value] : game.state["switches"].as_object()) {
            flagsList->addItem(QString("%1 = %2").arg(QString::fromStdString(flag),
                value.is_bool() && value.as_bool() ? "true" : "false"));
        }
    }
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
    connect(setFlagButton, &QPushButton::clicked, &dialog, [this, flagName, flagValue, flagsList]() {
        const QString key = flagName->text().trimmed();
        if (key.isEmpty()) return;
        game.state["switches"][key.toStdString()] = flagValue->currentIndex() == 0;
        flagsList->clear();
        for (const auto& [flag, value] : game.state["switches"].as_object()) {
            flagsList->addItem(QString("%1 = %2").arg(QString::fromStdString(flag),
                value.is_bool() && value.as_bool() ? "true" : "false"));
        }
    });
    connect(removeFlagButton, &QPushButton::clicked, &dialog, [this, flagsList]() {
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
    connect(applyButton, &QPushButton::clicked, &dialog, [this, &applied, eraCombo, yearSpin,
            seasonCombo, daySpin, hourSpin, minuteSpin, rateSpin, pausedCheck, climateCombo,
            weatherCombo, lockWeatherCheck, debugHudCheck]() {
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
        update();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    if (!applied) game.clock.paused = was_paused;
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

void GameView::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.fillRect(rect(), Qt::black);
    // Keep the game output at the 16:9 720p design resolution.
    const qreal sw = 1280.0, sh = 720.0;
    qreal sx = (qreal)width() / sw;
    qreal sy = (qreal)height() / sh;
    qreal s = qMin(sx, sy);
    p.translate(width() / 2.0 - sw * s / 2.0, height() / 2.0 - sh * s / 2.0);
    p.scale(s, s);
    renderer.set_painter(&p);
    game.draw(renderer);
    if (debugHud) {
        const QString era = QString::fromStdString(game.state["current_era"].as_string());
        const QString status = QString("%1  ·  Year %2 %3 %4  ·  %5:%6  ·  %7 / %8")
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
