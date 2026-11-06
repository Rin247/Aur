#include "WorldPanel.hpp"
#include "RhiBackend.hpp"
#include "TileTexture.hpp"
#include "aur/autotile.hpp"
#include "aur/clock.hpp"
#include "aur/forge.hpp"
#include "aur/game.hpp"
#include "aur/world_topology.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QtConcurrent/QtConcurrentRun>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtWidgets/qrhiwidget.h>
#include <rhi/qrhi.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <queue>
#include <cstring>
#include <utility>

namespace aur {
namespace {

std::string make_biome_id(const std::string& name) {
    std::string id;
    bool separator = false;
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) {
            if (separator && !id.empty()) id.push_back('_');
            id.push_back(static_cast<char>(std::tolower(ch)));
            separator = false;
        } else {
            separator = true;
        }
    }
    return id;
}

bool has_hd_surface(const Json& map) {
    return map.contains("worldgen") && map["worldgen"].is_object() &&
        map["worldgen"].contains("style") &&
        map["worldgen"]["style"].as_string() == "hd2d" &&
        map.contains("layers") && map["layers"].is_object() &&
        map["layers"].contains("elevation") && map["layers"]["elevation"].is_array();
}

uint32_t surface_hash(uint32_t seed, int x, int y, uint32_t layer) {
    uint32_t value = seed ^ static_cast<uint32_t>(x) * 0x9e3779b9u ^
                     static_cast<uint32_t>(y) * 0x85ebca6bu ^ layer;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

std::string terrain_tile_category(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized.find("biome_") == 0) return "biomes";
    const auto contains_any = [&normalized](std::initializer_list<const char*> terms) {
        return std::any_of(terms.begin(), terms.end(), [&normalized](const char* term) {
            return normalized.find(term) != std::string::npos;
        });
    };
    if (contains_any({"shore", "coast", "beach", "sand"})) return "coast";
    if (contains_any({"ocean", "water", "sea", "reef"})) return "underwater";
    if (contains_any({"mountain", "hill", "cliff", "rock", "stone", "snow", "scree"})) {
        return "elevation";
    }
    if (contains_any({"grass", "forest", "foliage", "jungle", "moss"})) return "surface";
    return "structures";
}

std::string climate_weather(const Project* project, const std::string& climate,
                            uint32_t roll) {
    if (!project || !project->meta.contains("weather")) return "clear";
    const Json& weather_settings = project->meta.at("weather");
    if (!weather_settings.is_object() || !weather_settings.contains("profiles")) return "clear";
    const Json& profiles = weather_settings["profiles"];
    if (!profiles.is_object()) return "clear";
    const Json* profile = profiles.contains(climate) ? &profiles[climate] : nullptr;
    if ((!profile || !profile->is_object()) && profiles.contains("temperate")) {
        profile = &profiles["temperate"];
    }
    if (!profile || !profile->is_object()) return "clear";

    int total_weight = 0;
    for (const auto& [_, weight] : profile->as_object()) {
        if (weight.is_number() && weight.as_int() > 0) total_weight += weight.as_int();
    }
    if (total_weight <= 0) return "clear";

    int selection = static_cast<int>(roll % static_cast<uint32_t>(total_weight));
    for (const auto& [weather, weight] : profile->as_object()) {
        if (!weight.is_number() || weight.as_int() <= 0) continue;
        selection -= weight.as_int();
        if (selection < 0) return weather;
    }
    return "clear";
}

} // namespace

class WorldMapOverlayWidget final : public QWidget {
public:
    explicit WorldMapOverlayWidget(WorldMapCanvasWidget* owner)
        : QWidget(owner), owner_(owner) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        owner_->paintCanvas(painter);
    }

private:
    WorldMapCanvasWidget* owner_;
};

class TerrainRhiWidget final : public QRhiWidget {
public:
    explicit TerrainRhiWidget(WorldMapCanvasWidget* owner)
        : QRhiWidget(owner), owner_(owner) {
        setApi(editor::configuredRhiWidgetApi());
        setSampleCount(1);
        setColorBufferFormat(TextureFormat::RGBA8);
        connect(this, &QRhiWidget::renderFailed, this, []() {
            qFatal("Aur could not initialize the configured QRhi rendering backend");
        });
    }

protected:
    void initialize(QRhiCommandBuffer*) override {
        editor::assertConfiguredRhiBackend(rhi());
        if (editor::isHeadlessRhi()) return;
        const QRhiDriverInfo driver = rhi()->driverInfo();
        qInfo().noquote() << "Aur world sculpt backend:" << rhi()->backendName()
                          << "device:" << driver.deviceName;
        ensureResources();
    }

    void render(QRhiCommandBuffer* commandBuffer) override {
        editor::assertConfiguredRhiBackend(rhi());
        if (editor::isHeadlessRhi()) return;
        if (!owner_->world_model || !owner_->world_model->width() ||
            !owner_->world_model->height()) {
            commandBuffer->beginPass(renderTarget(), QColor("#171b20"),
                                     QRhiDepthStencilClearValue{});
            commandBuffer->endPass();
            return;
        }
        ensureResources();
        if (!resourcesReady_) return;

        QRhiResourceUpdateBatch* updates = rhi()->nextResourceUpdateBatch();
        uploadDirtyRects(updates);
        const WorldModelUniforms uniforms = makeUniforms();
        updates->updateDynamicBuffer(uniformBuffer_.get(), 0, sizeof(uniforms), &uniforms);
        commandBuffer->beginPass(renderTarget(), QColor("#171b20"),
                                 QRhiDepthStencilClearValue{}, updates);
        commandBuffer->setGraphicsPipeline(pipeline_.get());
        const QSize pixelSize = renderTarget()->pixelSize();
        commandBuffer->setViewport(QRhiViewport(
            0, 0, static_cast<float>(pixelSize.width()),
            static_cast<float>(pixelSize.height())));
        commandBuffer->setShaderResources(bindings_.get());
        commandBuffer->draw(3);
        commandBuffer->endPass();
    }

    void releaseResources() override {
        resourcesReady_ = false;
        pipeline_.reset();
        bindings_.reset();
        uniformBuffer_.reset();
        linearSampler_.reset();
        nearestSampler_.reset();
        lutSampler_.reset();
        heightTexture_.reset();
        biomeTexture_.reset();
        waterTexture_.reset();
        biomeLutTexture_.reset();
        textureWidth_ = 0;
        textureHeight_ = 0;
        textureMap_.clear();
        textureGeneration_ = 0;
        uploadedRevision_ = 0;
        biomeLutPendingUpload_ = false;
    }

private:
    struct alignas(16) WorldModelUniforms {
        float mapRect[4];
        float world[4];
        float viewportLight[4];
        float effects[4];
        float environment[4];
        float ambient[4];
    };

    WorldMapCanvasWidget* owner_;
    std::unique_ptr<QRhiTexture> heightTexture_;
    std::unique_ptr<QRhiTexture> biomeTexture_;
    std::unique_ptr<QRhiTexture> waterTexture_;
    std::unique_ptr<QRhiTexture> biomeLutTexture_;
    QImage biome_lut_;
    std::unique_ptr<QRhiSampler> linearSampler_;
    std::unique_ptr<QRhiSampler> nearestSampler_;
    std::unique_ptr<QRhiSampler> lutSampler_;
    std::unique_ptr<QRhiBuffer> uniformBuffer_;
    std::unique_ptr<QRhiShaderResourceBindings> bindings_;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline_;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    std::string textureMap_;
    std::uint64_t textureGeneration_ = 0;
    std::uint64_t uploadedRevision_ = 0;
    bool resourcesReady_ = false;
    bool biomeLutPendingUpload_ = false;

    static QShader loadShader(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qFatal("Unable to open compiled world sculpt shader: %s", qPrintable(path));
        }
        const QShader shader = QShader::fromSerialized(file.readAll());
        if (!shader.isValid()) {
            qFatal("Unable to deserialize compiled world sculpt shader: %s", qPrintable(path));
        }
        return shader;
    }

    void ensureResources() {
        if (!rhi() || !renderTarget()) return;
        if (!owner_->world_model) return;
        const int width = owner_->world_model->width();
        const int height = owner_->world_model->height();
        if (width <= 0 || height <= 0) return;
        if (textureWidth_ == width && textureHeight_ == height &&
            textureMap_ == owner_->world_model_map &&
            textureGeneration_ == owner_->world_model_generation && resourcesReady_) return;

        releaseResources();
        if (!rhi()->isTextureFormatSupported(QRhiTexture::R16) ||
            !rhi()->isTextureFormatSupported(QRhiTexture::R8)) {
            qFatal("The selected QRhi backend does not support R16 and R8 world data textures");
        }
        heightTexture_.reset(rhi()->newTexture(QRhiTexture::R16, QSize(width, height)));
        biomeTexture_.reset(rhi()->newTexture(QRhiTexture::R8, QSize(width, height)));
        waterTexture_.reset(rhi()->newTexture(QRhiTexture::R8, QSize(width, height)));
        biomeLutTexture_.reset(rhi()->newTexture(QRhiTexture::RGBA8, QSize(32, 8)));
        const QRhiSampler::AddressMode addressU = owner_->world_model->wrapsX()
            ? QRhiSampler::Repeat : QRhiSampler::ClampToEdge;
        linearSampler_.reset(rhi()->newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            addressU, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        nearestSampler_.reset(rhi()->newSampler(
            QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
            addressU, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        lutSampler_.reset(rhi()->newSampler(
            QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        uniformBuffer_.reset(rhi()->newBuffer(
            QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(WorldModelUniforms)));
        if (!heightTexture_->create() || !biomeTexture_->create() || !waterTexture_->create() ||
            !biomeLutTexture_->create() || !linearSampler_->create() ||
            !nearestSampler_->create() || !lutSampler_->create() || !uniformBuffer_->create()) {
            qFatal("Failed to create world sculpt textures, samplers, or uniforms");
        }
        createBiomeLut();
        createBindingsAndPipeline();
        textureWidth_ = width;
        textureHeight_ = height;
        resourcesReady_ = true;
        uploadedRevision_ = 0;
        textureMap_ = owner_->world_model_map;
        textureGeneration_ = owner_->world_model_generation;
        update();
    }

    void createBiomeLut() {
        biome_lut_ = QImage(32, 8, QImage::Format_RGBA8888);
        biome_lut_.fill(QColor(116, 134, 106, 0));
        for (std::size_t id = 0; id < owner_->world_model->biomeNames().size() && id < 32; ++id) {
            const std::string& biomeId = owner_->world_model->biomeNames()[id];
            const bool forestBiome = biomeId.find("forest") != std::string::npos ||
                biomeId.find("jungle") != std::string::npos;
            QColor base("#74866a");
            if (owner_->project) {
                const auto biome = owner_->project->biomes.find(biomeId);
                if (biome != owner_->project->biomes.end() &&
                    biome->second.contains("color") && biome->second["color"].is_string()) {
                    const QColor candidate(
                        QString::fromStdString(biome->second["color"].as_string()));
                    if (candidate.isValid()) base = candidate;
                }
            }
            for (int band = 0; band < 8; ++band) {
                QColor color = base;
                if (band <= 1) color = color.darker(120 + band * 12);
                else if (band >= 6) color = color.lighter(100 + (band - 5) * 9);
                color.setAlpha(forestBiome ? 255 : 0);
                biome_lut_.setPixelColor(static_cast<int>(id), band, color);
            }
        }
        biomeLutPendingUpload_ = true;
    }

    void createBindingsAndPipeline() {
        bindings_.reset(rhi()->newShaderResourceBindings());
        bindings_->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::FragmentStage, uniformBuffer_.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage,
                heightTexture_.get(), linearSampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(
                2, QRhiShaderResourceBinding::FragmentStage,
                biomeTexture_.get(), nearestSampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(
                3, QRhiShaderResourceBinding::FragmentStage,
                biomeLutTexture_.get(), lutSampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(
                4, QRhiShaderResourceBinding::FragmentStage,
                waterTexture_.get(), nearestSampler_.get())
        });
        if (!bindings_->create()) qFatal("Failed to bind world sculpt resources");

        pipeline_.reset(rhi()->newGraphicsPipeline());
        pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
        pipeline_->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(":/aur/shaders/world_sculpt.vert.qsb")},
            {QRhiShaderStage::Fragment, loadShader(":/aur/shaders/world_sculpt.frag.qsb")}
        });
        pipeline_->setShaderResourceBindings(bindings_.get());
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        if (!pipeline_->create()) qFatal("Failed to create world sculpt pipeline");
    }

    template<typename T>
    static void uploadRect(QRhiResourceUpdateBatch* updates, QRhiTexture* texture,
                           const std::vector<T>& values, int worldWidth,
                           const editor::WorldModel::Rect& rect) {
        const std::size_t rowBytes = static_cast<std::size_t>(rect.width) * sizeof(T);
        std::vector<char> packed(rowBytes * static_cast<std::size_t>(rect.height));
        for (int row = 0; row < rect.height; ++row) {
            const std::size_t source =
                static_cast<std::size_t>(rect.y + row) * worldWidth +
                static_cast<std::size_t>(rect.x);
            std::memcpy(packed.data() + static_cast<std::size_t>(row) * rowBytes,
                        values.data() + source, rowBytes);
        }
        const QByteArray data(packed.data(), static_cast<qsizetype>(packed.size()));
        QRhiTextureSubresourceUploadDescription subresource(data);
        subresource.setDataStride(static_cast<quint32>(rowBytes));
        subresource.setSourceSize(QSize(rect.width, rect.height));
        subresource.setDestinationTopLeft(QPoint(rect.x, rect.y));
        updates->uploadTexture(
            texture, QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, subresource)));
    }

    void uploadDirtyRects(QRhiResourceUpdateBatch* updates) {
        editor::WorldModel& model = *owner_->world_model;
        if (uploadedRevision_ == 0) {
            uploadRect(updates, heightTexture_.get(), model.heights(), model.width(),
                       {0, 0, model.width(), model.height()});
            uploadRect(updates, biomeTexture_.get(), model.biomes(), model.width(),
                       {0, 0, model.width(), model.height()});
            uploadRect(updates, waterTexture_.get(), model.waterTypes(), model.width(),
                       {0, 0, model.width(), model.height()});
            model.takeDirtyRects();
            if (biomeLutPendingUpload_) {
                updates->uploadTexture(biomeLutTexture_.get(), biome_lut_);
                biomeLutPendingUpload_ = false;
            }
            uploadedRevision_ = model.revision() + 1;
            return;
        }
        for (const editor::WorldModel::DirtyRect& dirty : model.takeDirtyRects()) {
            if (dirty.layer == editor::WorldModel::Layer::Height) {
                uploadRect(updates, heightTexture_.get(), model.heights(), model.width(), dirty.rect);
            } else {
                uploadRect(updates, biomeTexture_.get(), model.biomes(), model.width(), dirty.rect);
            }
        }
        if (biomeLutPendingUpload_) {
            updates->uploadTexture(biomeLutTexture_.get(), biome_lut_);
            biomeLutPendingUpload_ = false;
        }
        uploadedRevision_ = model.revision() + 1;
    }

    WorldModelUniforms makeUniforms() const {
        const QRectF bounds = owner_->mapBounds();
        const editor::WorldModel& model = *owner_->world_model;
        WorldModelUniforms uniforms{};
        uniforms.mapRect[0] = static_cast<float>(bounds.left());
        uniforms.mapRect[1] = static_cast<float>(bounds.top());
        uniforms.mapRect[2] = static_cast<float>(std::max<qreal>(1.0, bounds.width()));
        uniforms.mapRect[3] = static_cast<float>(std::max<qreal>(1.0, bounds.height()));
        uniforms.world[0] = static_cast<float>(model.width());
        uniforms.world[1] = static_cast<float>(model.height());
        uniforms.world[2] = static_cast<float>(model.seaLevel()) / 65535.0f;
        uniforms.world[3] = 0.05f;
        uniforms.viewportLight[0] = static_cast<float>(owner_->width());
        uniforms.viewportLight[1] = static_cast<float>(owner_->height());
        uniforms.viewportLight[2] = 1.0f;
        uniforms.viewportLight[3] = static_cast<float>(owner_->zoom_factor);
        uniforms.effects[0] = owner_->contours_enabled ? 1.0f : 0.0f;
        uniforms.effects[1] = 1.0f;
        const qreal nearestIntegerZoom = std::round(owner_->zoom_factor);
        uniforms.effects[2] = owner_->zoom_factor >= 1.0 &&
            std::abs(owner_->zoom_factor - nearestIntegerZoom) < 0.001 ? 1.0f : 0.0f;
        uniforms.effects[3] = model.wrapsX() ? 1.0f : 0.0f;
        GameClock lighting = project_clock(owner_->project);
        lighting.set_time(owner_->preview_hour);
        const auto [rgb, darkness] = lighting.ambient();
        uniforms.environment[0] = static_cast<float>(owner_->preview_hour);
        uniforms.environment[1] = owner_->daynight_overlay ? 1.0f : 0.0f;
        uniforms.ambient[0] = std::get<0>(rgb) / 255.0f;
        uniforms.ambient[1] = std::get<1>(rgb) / 255.0f;
        uniforms.ambient[2] = std::get<2>(rgb) / 255.0f;
        uniforms.ambient[3] = owner_->daynight_overlay
            ? static_cast<float>(darkness) : 0.0f;
        return uniforms;
    }
};

WorldMapCanvasWidget::WorldMapCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 320);
    setMouseTracking(true);
    setAutoFillBackground(false);
    terrain_renderer = new TerrainRhiWidget(this);
    overlay_widget = new WorldMapOverlayWidget(this);
    terrain_renderer->hide();
    terrain_renderer->setGeometry(rect());
    overlay_widget->setGeometry(rect());
    overlay_widget->raise();
    simulation_timer.setInterval(100);
    connect(&simulation_timer, &QTimer::timeout, this, [this]() {
        ++simulation_phase;
        if (simulation_phase % 100 == 0) {
            preview_hour = std::fmod(preview_hour + 0.25, 24.0);
            emit timeOfDayChanged(static_cast<int>(preview_hour),
                                  static_cast<int>((preview_hour -
                                      std::floor(preview_hour)) * 60.0));
        }
        refreshCanvas();
    });
}

WorldMapCanvasWidget::~WorldMapCanvasWidget() = default;

void WorldMapCanvasWidget::setProject(Project* p) {
    project = p;
    undo_actions.clear();
    redo_actions.clear();
    tile_cache.clear();
    world_model.reset();
    world_model_map.clear();
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::setMap(const std::string& map_id) {
    if (current_map != map_id) {
        undo_actions.clear();
        redo_actions.clear();
        current_map = map_id;
        world_model.reset();
        world_model_map.clear();
        selected_node = -1;
        hover_tile = {-1, -1};
        pan_offset = {};
        tile_cache.clear();
        surface_cache = {};
        surface_cache_map.clear();
    }
    refreshCanvas();
}

void WorldMapCanvasWidget::ensureWorldModel() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !has_hd_surface(project->maps.at(current_map))) {
        world_model.reset();
        world_model_map.clear();
        return;
    }
    if (world_model && world_model_map == current_map) return;
    world_model = std::make_unique<editor::WorldModel>(
        editor::WorldModel::fromMap(project->maps.at(current_map), project->biomes));
    world_model_map = current_map;
    ++world_model_generation;
}

void WorldMapCanvasWidget::refreshCanvas() {
    ensureWorldModel();
    const bool gpuTerrain = world_model && terrain_renderer && !editor::isHeadlessRhi();
    if (terrain_renderer) {
        terrain_renderer->setVisible(gpuTerrain);
        if (gpuTerrain) terrain_renderer->update();
    }
    if (overlay_widget) overlay_widget->update();
    QWidget::update();
}

void WorldMapCanvasWidget::setTool(int tool_index) {
    selected_tool = tool_index;
    setCursor(selected_tool > 0 ? Qt::CrossCursor : Qt::ArrowCursor);
}

void WorldMapCanvasWidget::setSelectedTile(int tile_index) {
    selected_tile = tile_index;
}

void WorldMapCanvasWidget::setBrushSize(int size) {
    brush_size = std::clamp(size, 1, 15);
}

void WorldMapCanvasWidget::setSelectedBiome(const std::string& biome_id) {
    selected_biome = biome_id;
}

void WorldMapCanvasWidget::setSelectedNode(int node_index) {
    selected_node = node_index;
    refreshCanvas();
}

void WorldMapCanvasWidget::setTemperatureOverlay(bool enabled) {
    temperature_overlay = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setWindOverlay(bool enabled) {
    wind_overlay = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setCurrentsOverlay(bool enabled) {
    currents_overlay = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setWeatherOverlay(bool enabled) {
    weather_overlay = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setWeatherType(const std::string& weather) {
    preview_weather = weather;
    refreshCanvas();
}

void WorldMapCanvasWidget::setSimulationRunning(bool enabled) {
    if (enabled) simulation_timer.start();
    else simulation_timer.stop();
    refreshCanvas();
}

void WorldMapCanvasWidget::setDayNightOverlay(bool enabled) {
    daynight_overlay = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setContoursEnabled(bool enabled) {
    contours_enabled = enabled;
    refreshCanvas();
}

void WorldMapCanvasWidget::setTimeOfDay(int hour, int minute) {
    preview_hour = std::clamp(hour, 0, 23) + std::clamp(minute, 0, 59) / 60.0;
    refreshCanvas();
}

void WorldMapCanvasWidget::setZoom(qreal zoom) {
    zoom_factor = std::clamp(zoom, 0.05, 16.0);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
}

void WorldMapCanvasWidget::fitToView() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    qreal source_tile_size = 1.0;
    if (!has_hd_surface(map)) {
        const auto tileset = project->tilesets.find(map["tileset"].as_string());
        source_tile_size = tileset != project->tilesets.end() &&
            tileset->second.contains("tile") ? tileset->second["tile"].as_int() : 16;
    }
    constexpr qreal padding = 24.0;
    const qreal fit = std::min((width() - padding * 2) /
                                   (map["w"].as_int() * source_tile_size),
                               (height() - padding * 2) /
                                   (map["h"].as_int() * source_tile_size));
    pan_offset = {};
    setZoom(fit);
}

void WorldMapCanvasWidget::recordEntityEdit() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) map["entities"] = JsonArray{};
    undo_actions.push_back({true, map["entities"]});
    if (undo_actions.size() > 100) undo_actions.erase(undo_actions.begin());
    redo_actions.clear();
}

QRectF WorldMapCanvasWidget::mapBounds() const {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return {};

    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {};
    qreal source_tile_size = 1.0;
    if (!has_hd_surface(map)) {
        const auto tileset = project->tilesets.find(map["tileset"].as_string());
        source_tile_size = tileset != project->tilesets.end() &&
            tileset->second.contains("tile") ? tileset->second["tile"].as_int() : 16;
    }

    const QSizeF size(map_width * source_tile_size * zoom_factor,
                      map_height * source_tile_size * zoom_factor);
    return QRectF((width() - size.width()) / 2.0 + pan_offset.x(),
                  (height() - size.height()) / 2.0 + pan_offset.y(),
                  size.width(), size.height());
}

void WorldMapCanvasWidget::constrainWorldCamera() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    if (!aur::wraps_world_x(map)) return;
    const QRectF bounds = mapBounds();
    const qreal vertical_pan_limit = std::max<qreal>(0.0, (bounds.height() - height()) * 0.5);
    pan_offset.setY(std::clamp(pan_offset.y(), -vertical_pan_limit, vertical_pan_limit));
    if (bounds.width() > 0.0) {
        pan_offset.setX(std::remainder(pan_offset.x(), bounds.width()));
    }
}

QPoint WorldMapCanvasWidget::tileAt(const QPointF& position) const {
    const QRectF bounds = mapBounds();
    if (!project || !project->maps.contains(current_map) ||
        position.y() < bounds.top() || position.y() >= bounds.bottom()) return {-1, -1};

    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const qreal cell = bounds.width() / map_width;
    const int tile_x = static_cast<int>(std::floor((position.x() - bounds.left()) / cell));
    const int tile_y = static_cast<int>(std::floor((position.y() - bounds.top()) / cell));
    if (tile_y < 0 || tile_y >= map_height) return {-1, -1};
    if (aur::wraps_world_x(map)) return {aur::wrapX(tile_x, map_width), tile_y};
    if (tile_x < 0 || tile_x >= map_width) return {-1, -1};
    return {tile_x, tile_y};
}

QImage WorldMapCanvasWidget::tileImage(const std::string& tileset_id, int tile_index) {
    const auto key = std::make_pair(tileset_id, tile_index);
    const auto cached = tile_cache.find(key);
    if (cached != tile_cache.end()) return cached->second;

    const QImage image = make_tileset_tile(*project, tileset_id, tile_index);
    return tile_cache.emplace(key, image).first->second;
}

QImage WorldMapCanvasWidget::buildHdSurface(const Json& map) const {
    if (!map.contains("layers") || !map["layers"].is_object() ||
        !map["layers"].contains("elevation") || !map["layers"]["elevation"].is_array()) {
        return {};
    }

    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return {};
    constexpr int target_pixels_per_tile = 32;
    constexpr std::uint64_t max_surface_pixels = 16 * 1024 * 1024;
    const std::uint64_t tile_count =
        static_cast<std::uint64_t>(map_width) * static_cast<std::uint64_t>(map_height);
    int pixels_per_tile = target_pixels_per_tile;
    while (pixels_per_tile > 1 &&
           tile_count > max_surface_pixels /
               static_cast<std::uint64_t>(pixels_per_tile * pixels_per_tile)) {
        --pixels_per_tile;
    }
    const int image_width = map_width * pixels_per_tile;
    const int image_height = map_height * pixels_per_tile;
    QImage image(image_width, image_height, QImage::Format_RGB32);
    if (image.isNull()) return {};

    const auto& elevation_rows = map["layers"]["elevation"].as_array();
    const JsonArray* moisture_rows = map["layers"].contains("moisture") &&
        map["layers"]["moisture"].is_array() ? &map["layers"]["moisture"].as_array() : nullptr;
    const JsonArray* biome_rows = map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array() ? &map["layers"]["biomes"].as_array() : nullptr;
    const JsonArray* water_rows = map["layers"].contains("water") &&
        map["layers"]["water"].is_array() ? &map["layers"]["water"].as_array() : nullptr;
    const int sea_level = map.contains("worldgen") &&
        map["worldgen"].contains("sea_level") ? map["worldgen"]["sea_level"].as_int() : 500;
    const uint32_t seed = static_cast<uint32_t>(map.contains("worldgen") &&
        map["worldgen"].contains("seed") ? map["worldgen"]["seed"].as_int() : 1);
    const bool wraps_x = aur::wraps_world_x(map);
    const auto wrapped_x = [map_width, wraps_x](int x) {
        return wraps_x ? aur::wrapX(x, map_width) : x;
    };
    const int axial_tilt = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const double axis_radians = axial_tilt * 3.14159265358979323846 / 180.0;
    const double axis_sine = std::sin(axis_radians);
    const double axis_cosine = std::cos(axis_radians);
    const double latitude_scale = std::max(
        0.001, std::abs(axis_sine) + std::abs(axis_cosine));

    std::map<std::string, QColor> biome_colors{
        {"verdant_plains", QColor("#8eae65")},
        {"old_growth_forest", QColor("#426e50")},
        {"tropical_jungle", QColor("#39785b")},
        {"desert_wastes", QColor("#c6a066")},
        {"alpine_highlands", QColor("#858f8b")},
        {"frostfall_tundra", QColor("#c1d1d1")},
        {"mistfen_marsh", QColor("#668b78")},
        {"volcanic_badlands", QColor("#9c594a")},
        {"coast_and_islands", QColor("#d1bc83")},
        {"ancient_ruins", QColor("#92816b")},
        {"crystal_reaches", QColor("#8e80b4")},
        {"sky_isles", QColor("#8ab5c6")}
    };
    if (project) {
        for (const auto& [id, biome] : project->biomes) {
            if (!biome.contains("color") || !biome["color"].is_string()) continue;
            const QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) biome_colors[id] = color;
        }
    }

    const auto field_value = [](const JsonArray* rows, int x, int y, int fallback) {
        if (!rows || y < 0 || y >= static_cast<int>(rows->size())) return fallback;
        const Json& row = (*rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) return fallback;
        return row[static_cast<size_t>(x)].as_int();
    };
    const auto biome_at = [biome_rows, map_height](int x, int y) {
        if (!biome_rows || y < 0 || y >= map_height) return std::string{};
        const Json& row = (*biome_rows)[static_cast<size_t>(y)];
        if (!row.is_array() || x < 0 || x >= static_cast<int>(row.as_array().size())) {
            return std::string{};
        }
        return row[static_cast<size_t>(x)].as_string();
    };
    const auto elevation_at = [&field_value, &elevation_rows](int x, int y) {
        return field_value(&elevation_rows, x, y, 0);
    };
    const auto water_at = [&field_value, water_rows](int x, int y) {
        return field_value(water_rows, x, y, 0);
    };
    const auto is_water_at = [&elevation_at, &water_at, sea_level](int x, int y) {
        return elevation_at(x, y) <= sea_level || water_at(x, y) == 2;
    };
    std::vector<int> shore_distance(
        static_cast<size_t>(map_width) * map_height, map_width + map_height);
    std::queue<int> shore_frontier;
    for (int y = 0; y < map_height; ++y) {
        for (int x = 0; x < map_width; ++x) {
            const bool water = is_water_at(x, y);
            const bool coast = is_water_at(aur::wrapX(x - 1, map_width), y) != water ||
                is_water_at(aur::wrapX(x + 1, map_width), y) != water ||
                (y > 0 && is_water_at(x, y - 1) != water) ||
                (y + 1 < map_height && is_water_at(x, y + 1) != water);
            if (!coast) continue;
            const int index = y * map_width + x;
            shore_distance[static_cast<size_t>(index)] = 1;
            shore_frontier.push(index);
        }
    }
    constexpr std::array<std::pair<int, int>, 8> shore_neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    const int maximum_shore_distance = std::min(6, std::max(map_width, map_height));
    while (!shore_frontier.empty()) {
        const int index = shore_frontier.front();
        shore_frontier.pop();
        const int x = index % map_width;
        const int y = index / map_width;
        const int next_distance = shore_distance[static_cast<size_t>(index)] + 1;
        if (next_distance > maximum_shore_distance) continue;
        const bool water = is_water_at(x, y);
        for (const auto& [dx, dy] : shore_neighbors) {
            const int next_y = y + dy;
            if (next_y < 0 || next_y >= map_height) continue;
            const int next_x = aur::wrapX(x + dx, map_width);
            if (is_water_at(next_x, next_y) != water) continue;
            const int next = next_y * map_width + next_x;
            if (shore_distance[static_cast<size_t>(next)] <= next_distance) continue;
            shore_distance[static_cast<size_t>(next)] = next_distance;
            shore_frontier.push(next);
        }
    }
    const auto sample_field = [&field_value, map_width, map_height](
                                  const JsonArray* rows, double x, double y, int fallback) {
        const int x0_unwrapped = static_cast<int>(std::floor(x));
        const int y0_unclamped = static_cast<int>(std::floor(y));
        const double fx = x - x0_unwrapped;
        const double fy = y - y0_unclamped;
        const int x0 = aur::wrapX(x0_unwrapped, map_width);
        const int x1 = aur::wrapX(x0_unwrapped + 1, map_width);
        const int y0 = std::clamp(y0_unclamped, 0, map_height - 1);
        const int y1 = std::clamp(y0_unclamped + 1, 0, map_height - 1);
        const double top = field_value(rows, x0, y0, fallback) * (1.0 - fx) +
                           field_value(rows, x1, y0, fallback) * fx;
        const double bottom = field_value(rows, x0, y1, fallback) * (1.0 - fx) +
                              field_value(rows, x1, y1, fallback) * fx;
        return top * (1.0 - fy) + bottom * fy;
    };
    const auto mix_color = [](const QColor& a, const QColor& b, double amount) {
        const double t = std::clamp(amount, 0.0, 1.0);
        return QColor(
            qRound(a.red() + (b.red() - a.red()) * t),
            qRound(a.green() + (b.green() - a.green()) * t),
            qRound(a.blue() + (b.blue() - a.blue()) * t));
    };
    const auto smoothstep = [](double low, double high, double value) {
        const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    };
    const auto climate_latitude_at = [map_width, map_height, wraps_x, axis_sine,
                                      axis_cosine, latitude_scale](int x, int y) {
        const double centered_y = map_height > 1
            ? 2.0 * y / (map_height - 1) - 1.0 : 0.0;
        if (wraps_x) return std::clamp(std::abs(centered_y), 0.0, 1.0);
        const double centered_x = map_width > 1
            ? 2.0 * x / (map_width - 1) - 1.0 : 0.0;
        return std::clamp(std::abs(
            (centered_x * axis_sine + centered_y * axis_cosine) / latitude_scale),
            0.0, 1.0);
    };
    const QColor shallow_ocean("#327b88");
    const QColor beach("#d9c58f");
    const auto biome_color = [&biome_colors, &shallow_ocean](const std::string& id) {
        if (id.empty()) return shallow_ocean;
        if (id == "coast_and_islands") return QColor("#7d9862");
        const auto biome = biome_colors.find(id);
        return biome == biome_colors.end() ? QColor("#8eae65") : biome->second;
    };
    const auto depth_color = [&mix_color](double depth) {
        constexpr std::array<double, 6> depths{0.0, 45.0, 140.0, 320.0, 620.0, 1000.0};
        constexpr std::array<const char*, 6> colors{
            "#68b9ad", "#4c9eaa", "#347d98", "#285f80", "#1c4264", "#132d49"
        };
        size_t band = 0;
        while (band + 1 < depths.size() && depth > depths[band + 1]) ++band;
        if (band + 1 == depths.size()) return QColor(colors.back());
        const double blend = (depth - depths[band]) / (depths[band + 1] - depths[band]);
        return mix_color(QColor(colors[band]), QColor(colors[band + 1]), blend);
    };

    for (int py = 0; py < image_height; ++py) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(py));
        const int y = py / pixels_per_tile;
        const double world_y = (py + 0.5) / pixels_per_tile - 0.5;
        for (int px = 0; px < image_width; ++px) {
            const int x = px / pixels_per_tile;
            const double world_x = (px + 0.5) / pixels_per_tile - 0.5;
            const double height = sample_field(&elevation_rows, world_x, world_y, 0);
            const double water_depth = std::max(0.0, sea_level - height);
            const std::string biome_id = biome_at(x, y);
            const double moisture = sample_field(moisture_rows, world_x, world_y, 50);
            const int inland_water = field_value(water_rows, x, y, 0);
            const int shore = shore_distance[static_cast<size_t>(y * map_width + x)];
            const double climate_latitude = climate_latitude_at(x, y);
            const double polar_ice = smoothstep(0.78, 0.97, climate_latitude);
            QColor color;
            if (height <= sea_level) {
                color = depth_color(water_depth);
                if (shore <= 2 && water_depth < 110.0) {
                    color = mix_color(color, QColor("#79c3b2"),
                                      (3.0 - shore) * (110.0 - water_depth) / 440.0);
                }
                color = mix_color(color, QColor("#91b9bd"), polar_ice * 0.78);
            } else if (inland_water > 0) {
                if (inland_water == 2) {
                    color = QColor("#347f91");
                } else {
                    color = QColor("#286f87");
                }
            } else {
                color = biome_color(biome_id);
                const double altitude = std::clamp((height - sea_level) / 340.0, 0.0, 1.0);
                if (shore == 1 && height - sea_level < 100) {
                    color = mix_color(color, beach,
                        std::clamp((100.0 - (height - sea_level)) / 100.0, 0.0, 0.92));
                } else if (shore == 2 && height - sea_level < 48) {
                    color = mix_color(color, beach, 0.16);
                } else if (altitude > 0.78) {
                    color = mix_color(color, QColor("#dce4dc"),
                                      std::clamp((altitude - 0.66) * 1.9, 0.0, 0.68));
                } else if (moisture < 20) {
                    color = mix_color(color, QColor("#c2a16d"),
                                      std::clamp((20.0 - moisture) / 55.0, 0.0, 0.24));
                }
                color = mix_color(color, QColor("#c8d8d5"), polar_ice * 0.92);
            }

            const double west = sample_field(&elevation_rows, world_x - 0.35, world_y, height);
            const double east = sample_field(&elevation_rows, world_x + 0.35, world_y, height);
            const double north = sample_field(&elevation_rows, world_x, world_y - 0.35, height);
            const double south = sample_field(&elevation_rows, world_x, world_y + 0.35, height);
            const bool ocean = height <= sea_level;
            const double relief = ocean
                ? std::clamp(1.0 + (west - east) * 0.00055 +
                             (north - south) * 0.00045, 0.93, 1.07)
                : std::clamp(1.0 + (west - east) * 0.0018 +
                             (north - south) * 0.0015, 0.72, 1.26);
            const uint32_t texture_hash = surface_hash(seed, x / 2, y / 2, 0x68bc21ebu);
            const double texture = ocean || polar_ice > 0.4
                ? 0.0 : static_cast<double>(texture_hash & 7u) - 3.0;
            const double light = std::clamp(relief + texture / 100.0, 0.68, 1.28);
            line[px] = qRgb(
                std::clamp(qRound(color.red() * light), 0, 255),
                std::clamp(qRound(color.green() * light), 0, 255),
                std::clamp(qRound(color.blue() * light), 0, 255));
        }
    }

    QPainter details(&image);
    details.setRenderHint(QPainter::Antialiasing);
    details.scale(pixels_per_tile, pixels_per_tile);
    const QColor wave_color(176, 222, 225, 34);
    const QColor foam_color(230, 243, 224, 96);
    const QColor forest_shadow(32, 62, 53, 112);
    const QColor forest_mid(49, 93, 66, 182);
    const QColor forest_light(98, 131, 77, 145);
    details.setPen(QPen(wave_color, 0.035, Qt::SolidLine, Qt::RoundCap));
    for (int y = 1; y < map_height - 1; y += 3) {
        for (int x = 0; x < map_width; x += 3) {
            if (elevation_at(x, y) > sea_level && water_at(x, y) == 0) continue;
            const uint32_t hash = surface_hash(seed, x, y, 0x02e5be93u);
            const qreal left = x + 0.12 + ((hash >> 3) & 7u) * 0.045;
            const qreal top = y + 0.18 + ((hash >> 8) & 7u) * 0.055;
            const qreal length = 0.35 + ((hash >> 13) & 7u) * 0.045;
            QPainterPath wave;
            wave.moveTo(left, top);
            wave.quadTo(left + length * 0.5, top - 0.09, left + length, top);
            details.drawPath(wave);
        }
    }

    std::vector<std::pair<QPointF, QPointF>> river_segments;
    constexpr std::array<std::pair<int, int>, 8> flow_neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};
    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            if (water_at(x, y) != 1) continue;
            const int source_height = elevation_at(x, y);
            int downstream_x = x;
            int downstream_y = y;
            int lowest_height = source_height;
            for (const auto& [dx, dy] : flow_neighbors) {
                const int next_y = y + dy;
                if (next_y < 0 || next_y >= map_height) continue;
                const int next_x = wrapped_x(x + dx);
                const int next_height = elevation_at(next_x, next_y) <= sea_level
                    ? sea_level : elevation_at(next_x, next_y);
                if (next_height >= lowest_height) continue;
                lowest_height = next_height;
                downstream_x = next_x;
                downstream_y = next_y;
            }
            if (downstream_x == x && downstream_y == y) continue;
            if (water_at(downstream_x, downstream_y) != 1 &&
                water_at(downstream_x, downstream_y) != 2 &&
                elevation_at(downstream_x, downstream_y) > sea_level) continue;

            qreal end_x = downstream_x + 0.5;
            if (downstream_x - x > 1) end_x -= map_width;
            else if (x - downstream_x > 1) end_x += map_width;
            river_segments.emplace_back(QPointF(x + 0.5, y + 0.5),
                                        QPointF(end_x, downstream_y + 0.5));
        }
    }
    details.setPen(QPen(QColor(14, 48, 62, 65), 0.28,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }
    details.setPen(QPen(QColor("#347f91"), 0.17,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }
    details.setPen(QPen(QColor(123, 205, 200, 125), 0.035,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& [source, destination] : river_segments) {
        details.drawLine(source, destination);
    }

    details.setPen(QPen(QColor(132, 202, 205, 145), 0.08, Qt::SolidLine, Qt::RoundCap));
    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            if (water_at(x, y) != 2) continue;
            const uint32_t hash = surface_hash(seed, x, y, 0x6c616b65u);
            if ((hash & 3u) != 0u) continue;
            const qreal left = x + 0.2 + ((hash >> 3) & 7u) * 0.045;
            const qreal top = y + 0.25 + ((hash >> 8) & 7u) * 0.04;
            details.drawLine(QPointF(left, top),
                             QPointF(left + 0.22, top + 0.025));
        }
    }

    for (int y = 1; y < map_height - 1; ++y) {
        for (int x = 0; x < map_width; ++x) {
            const int height = elevation_at(x, y);
            if (height <= sea_level || water_at(x, y) > 0) {
                if (height <= sea_level) {
                    const bool land_north = elevation_at(x, y - 1) > sea_level &&
                        water_at(x, y - 1) == 0;
                    const bool land_east = elevation_at(wrapped_x(x + 1), y) > sea_level &&
                        water_at(wrapped_x(x + 1), y) == 0;
                    const bool land_south = elevation_at(x, y + 1) > sea_level &&
                        water_at(x, y + 1) == 0;
                    const bool land_west = elevation_at(wrapped_x(x - 1), y) > sea_level &&
                        water_at(wrapped_x(x - 1), y) == 0;
                    const uint32_t foam_hash = surface_hash(seed, x, y, 0x9d39247eu);
                    details.setPen(QPen(foam_color, 0.055, Qt::SolidLine, Qt::RoundCap));
                    const qreal inset = 0.08 + ((foam_hash >> 4) & 3u) * 0.025;
                    const qreal span = 0.54 + ((foam_hash >> 9) & 3u) * 0.08;
                    if (land_north) {
                        details.drawLine(QPointF(x + inset, y + 0.07),
                                         QPointF(x + inset + span, y + 0.07));
                    }
                    if (land_east) {
                        details.drawLine(QPointF(x + 0.93, y + inset),
                                         QPointF(x + 0.93, y + inset + span));
                    }
                    if (land_south) {
                        details.drawLine(QPointF(x + inset, y + 0.93),
                                         QPointF(x + inset + span, y + 0.93));
                    }
                    if (land_west) {
                        details.drawLine(QPointF(x + 0.07, y + inset),
                                         QPointF(x + 0.07, y + inset + span));
                    }
                }
                continue;
            }

            const std::string biome_id = biome_at(x, y);
            const uint32_t hash = surface_hash(seed, x, y, 0xa24baed5u);
            const bool forest_biome = biome_id.find("forest") != std::string::npos ||
                biome_id.find("jungle") != std::string::npos;
            const int moisture = field_value(moisture_rows, x, y, 50);
            if ((forest_biome || moisture > 82) && climate_latitude_at(x, y) < 0.78 &&
                height < sea_level + 330 &&
                hash % (forest_biome ? 13u : 17u) < (forest_biome ? 4u : 2u)) {
                const qreal center_x = x + 0.18 + ((hash >> 4) & 7u) * 0.09;
                const qreal center_y = y + 0.18 + ((hash >> 9) & 7u) * 0.09;
                const qreal size = 0.55 + ((hash >> 14) & 7u) * 0.075;
                details.setPen(Qt::NoPen);
                details.setBrush(forest_shadow);
                details.drawEllipse(QPointF(center_x + 0.04, center_y + 0.16),
                                    size * 1.02, size * 0.48);
                details.setBrush(forest_mid);
                details.drawEllipse(QPointF(center_x - size * 0.42, center_y),
                                    size * 0.58, size * 0.62);
                details.drawEllipse(QPointF(center_x + size * 0.02, center_y - size * 0.10),
                                    size * 0.62, size * 0.72);
                details.drawEllipse(QPointF(center_x + size * 0.45, center_y + size * 0.01),
                                    size * 0.55, size * 0.60);
                details.setBrush(forest_light);
                details.drawEllipse(QPointF(center_x - size * 0.20, center_y - size * 0.31),
                                    size * 0.29, size * 0.20);
            }

            if (height < sea_level + 190 ||
                surface_hash(seed, x, y, 0x3c6ef372u) % 9u != 0u) continue;
            const int west = elevation_at(wrapped_x(x - 1), y);
            const int east = elevation_at(wrapped_x(x + 1), y);
            const int north = elevation_at(x, y - 1);
            const int south = elevation_at(x, y + 1);
            if (height < west || height < east || height < north || height < south) continue;

            details.save();
            details.translate(x + 0.5, y + 0.5);
            if (std::abs(east - west) > std::abs(south - north)) details.rotate(90.0);
            const qreal ridge_shift = ((hash >> 10) & 7u) * 0.018;
            const qreal main_peak = -0.57 - ((hash >> 18) & 3u) * 0.045;
            QPolygonF mountain_range{
                QPointF(-0.78, 0.43), QPointF(-0.57, 0.05),
                QPointF(-0.42, -0.29), QPointF(-0.23, 0.04),
                QPointF(-0.02, main_peak), QPointF(0.17, 0.01),
                QPointF(0.39, -0.27), QPointF(0.54, 0.04),
                QPointF(0.78, 0.43)
            };
            for (QPointF& point : mountain_range) point.rx() += ridge_shift;
            details.setPen(QPen(QColor("#514b45"), 0.035));
            details.setBrush(QColor("#827665"));
            details.drawPolygon(mountain_range);

            details.setPen(Qt::NoPen);
            details.setBrush(QColor("#b7a58a"));
            details.drawPolygon(QPolygonF{
                QPointF(-0.57 + ridge_shift, 0.05),
                QPointF(-0.42 + ridge_shift, -0.29),
                QPointF(-0.30 + ridge_shift, 0.17),
                QPointF(-0.45 + ridge_shift, 0.12)
            });
            details.drawPolygon(QPolygonF{
                QPointF(-0.02 + ridge_shift, main_peak),
                QPointF(0.14 + ridge_shift, 0.13),
                QPointF(-0.12 + ridge_shift, 0.17)
            });
            details.drawPolygon(QPolygonF{
                QPointF(0.39 + ridge_shift, -0.27),
                QPointF(0.54 + ridge_shift, 0.04),
                QPointF(0.27 + ridge_shift, 0.16)
            });
            if (height > sea_level + 470) {
                details.setBrush(QColor("#e4e6dc"));
                details.drawPolygon(QPolygonF{
                    QPointF(-0.09 + ridge_shift, main_peak + 0.15),
                    QPointF(-0.02 + ridge_shift, main_peak),
                    QPointF(0.06 + ridge_shift, main_peak + 0.17),
                    QPointF(0.02 + ridge_shift, main_peak + 0.14)
                });
                details.drawPolygon(QPolygonF{
                    QPointF(0.34 + ridge_shift, -0.12),
                    QPointF(0.39 + ridge_shift, -0.27),
                    QPointF(0.46 + ridge_shift, -0.09)
                });
            }
            details.restore();
        }
    }
    return image;
}

void WorldMapCanvasWidget::paintTile(const QPoint& tile) {
    if ((selected_tool != 1 && selected_tool != 2 &&
         selected_tool != 5 && selected_tool != 6) || !project || tile.y() < 0 ||
        !project->maps.contains(current_map)) return;

    Json& map = project->maps[current_map];
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const bool wraps_x = aur::wraps_world_x(map);
    if (tile.y() >= map_height ||
        (!wraps_x && (tile.x() < 0 || tile.x() >= map_width))) return;
    const int center_x = wraps_x ? aur::wrapX(tile.x(), map_width) : tile.x();
    if (!map.contains("layers") || !map["layers"].is_object()) return;
    const bool hdSurface = has_hd_surface(map);
    const int radius = brush_size / 2;
    std::vector<QPoint> stamp;
    std::set<int> stamp_indices;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (radius > 0 && dx * dx + dy * dy > radius * radius + radius) continue;
            const int y = tile.y() + dy;
            if (y < 0 || y >= map_height) continue;
            const int x = wraps_x ? aur::wrapX(center_x + dx, map_width) : center_x + dx;
            if (x < 0 || x >= map_width) continue;
            const int index = y * map_width + x;
            if (stamp_indices.insert(index).second) stamp.emplace_back(x, y);
        }
    }
    if (selected_tool == 5 || selected_tool == 6) {
        ensureWorldModel();
        if (!world_model) return;
        constexpr int heightStep = 20;
        for (const QPoint& position : stamp) {
            const int index = position.y() * world_model->width() + position.x();
            if (stroke_active && stroke_captured_cells.insert(index).second) {
                stroke_model_rects.push_back(world_model->captureRect(
                    editor::WorldModel::Layer::Height, {position.x(), position.y(), 1, 1}));
            }
            const std::uint16_t oldHeight =
                world_model->heights()[static_cast<std::size_t>(index)];
            const int delta = selected_tool == 5 ? heightStep : -heightStep;
            const std::uint16_t newHeight = static_cast<std::uint16_t>(
                std::clamp(static_cast<int>(oldHeight) + delta, 0, 65535));
            const std::array<std::uint16_t, 1> value{newHeight};
            const editor::WorldModel::Rect rect{position.x(), position.y(), 1, 1};
            world_model->setRect(editor::WorldModel::Layer::Height, rect, value);
            world_model->writeRect(map, editor::WorldModel::Layer::Height, rect);
        }
        refreshCanvas();
        return;
    }

    if (selected_tool == 1) {
        if (!hdSurface && !map["layers"].contains("ground")) return;
        if (hdSurface) {
            const int map_width = map["w"].as_int();
            const int map_height = map["h"].as_int();
            if (!map["layers"].contains("terrain_overrides") ||
                !map["layers"]["terrain_overrides"].is_array()) {
                map["layers"]["terrain_overrides"] = JsonArray{};
            }
            JsonArray& overrides = map["layers"]["terrain_overrides"].as_array();
            while (static_cast<int>(overrides.size()) < map_height) {
                JsonArray row;
                for (int x = 0; x < map_width; ++x) row.emplace_back(-1);
                overrides.emplace_back(std::move(row));
            }
            for (int y = 0; y < map_height; ++y) {
                Json& row_value = overrides[static_cast<size_t>(y)];
                if (!row_value.is_array()) {
                    JsonArray row;
                    for (int x = 0; x < map_width; ++x) row.emplace_back(-1);
                    row_value = std::move(row);
                    continue;
                }
                JsonArray& row = row_value.as_array();
                while (static_cast<int>(row.size()) < map_width) row.emplace_back(-1);
            }
        }
        const auto tileset_it = project->tilesets.find(map["tileset"].as_string());
        const Json* tileset = tileset_it == project->tilesets.end()
            ? nullptr : &tileset_it->second;
        const std::map<int, std::string> groups = tileset ? tile_groups(*tileset)
                                                         : std::map<int, std::string>{};
        const auto selected_group = groups.find(selected_tile);
        for (const QPoint& position : stamp) {
            if (hdSurface || (map["layers"].contains("terrain_overrides") &&
                map["layers"]["terrain_overrides"].is_array())) {
                map["layers"]["terrain_overrides"][static_cast<size_t>(position.y())]
                    [static_cast<size_t>(position.x())] = selected_tile;
            }
            if (hdSurface) continue;
            if (selected_group != groups.end() && tileset) {
                aur::paint(map, *tileset, "ground", position.x(), position.y(),
                            selected_group->second);
            } else {
                map["layers"]["ground"][static_cast<size_t>(position.y())]
                    [static_cast<size_t>(position.x())] = selected_tile;
                if (tileset) {
                    for (const auto& [dx, dy] :
                         std::array<std::pair<int, int>, 5>{{{0, 0}, {0, -1}, {1, 0}, {0, 1}, {-1, 0}}}) {
                        retile(map, *tileset, "ground", position.x() + dx,
                               position.y() + dy, groups);
                    }
                }
            }
        }
    } else if (hdSurface) {
        ensureWorldModel();
        if (!world_model) return;
        const auto biomeId = world_model->biomeId(selected_biome);
        if (!biomeId) {
            QMessageBox::warning(this, "Biome paint failed",
                                 "This map uses more than 32 biome colors; the selected biome "
                                 "cannot be represented by the sculpt palette.");
            return;
        }
        for (const QPoint& position : stamp) {
            const int index = position.y() * world_model->width() + position.x();
            if (stroke_active && stroke_captured_cells.insert(index).second) {
                stroke_model_rects.push_back(world_model->captureRect(
                    editor::WorldModel::Layer::Biome, {position.x(), position.y(), 1, 1}));
            }
            const std::array<std::uint8_t, 1> value{*biomeId};
            const editor::WorldModel::Rect rect{position.x(), position.y(), 1, 1};
            world_model->setRect(editor::WorldModel::Layer::Biome, rect, value);
            world_model->writeRect(map, editor::WorldModel::Layer::Biome, rect);
        }
    } else {
        try {
            for (const QPoint& position : stamp) {
                paint_biome(*project, current_map, position.x(), position.y(), selected_biome);
            }
        } catch (const std::exception& error) {
            QMessageBox::warning(this, "Biome paint failed", QString::fromStdString(error.what()));
            setTool(0);
            return;
        }
    }
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::undo() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        undo_actions.empty()) return;
    Json& map = project->maps[current_map];
    const UndoAction action = undo_actions.back();
    undo_actions.pop_back();
    if (action.entities) {
        redo_actions.push_back({true, map["entities"]});
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        UndoAction redo;
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            redo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
        redo_actions.push_back(std::move(redo));
    } else {
        redo_actions.push_back({false, map["layers"], {}});
        map["layers"] = action.state;
    }
    surface_cache = {};
    surface_cache_map.clear();
    refreshCanvas();
}

void WorldMapCanvasWidget::redo() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        redo_actions.empty()) return;
    Json& map = project->maps[current_map];
    const UndoAction action = redo_actions.back();
    redo_actions.pop_back();
    if (action.entities) {
        undo_actions.push_back({true, map["entities"]});
        map["entities"] = action.state;
        emit entitiesChanged();
    } else if (!action.model_rects.empty() && world_model) {
        UndoAction undo;
        for (auto it = action.model_rects.rbegin(); it != action.model_rects.rend(); ++it) {
            undo.model_rects.push_back(world_model->captureRect(it->layer, it->rect));
            world_model->restore(*it);
            world_model->writeRect(map, it->layer, it->rect);
        }
        undo_actions.push_back(std::move(undo));
    } else {
        undo_actions.push_back({false, map["layers"], {}});
        map["layers"] = action.state;
    }
    surface_cache = {};
    surface_cache_map.clear();
    update();
}

void WorldMapCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#171b20"));
}

void WorldMapCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (terrain_renderer) terrain_renderer->setGeometry(rect());
    if (overlay_widget) overlay_widget->setGeometry(rect());
    refreshCanvas();
}

void WorldMapCanvasWidget::paintCanvas(QPainter& painter) {
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const bool gpuTerrain = world_model && terrain_renderer && terrain_renderer->isVisible();
    if (!gpuTerrain) painter.fillRect(rect(), QColor("#171b20"));

    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        painter.setPen(QColor("#aeb8c2"));
        painter.drawText(rect(), Qt::AlignCenter, "Select a world map to begin");
        return;
    }

    const Json& map = project->maps.at(current_map);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    const QRectF bounds = mapBounds();
    const bool hd_surface = has_hd_surface(map);
    const bool wraps_x = aur::wraps_world_x(map);
    if (bounds.isEmpty() || !map.contains("layers") || !map["layers"].is_object() ||
        (!hd_surface && !map["layers"].contains("ground"))) return;

    const JsonArray* ground = !hd_surface && map["layers"]["ground"].is_array()
        ? &map["layers"]["ground"].as_array() : nullptr;
    const qreal cell = bounds.width() / map_width;
    const std::string tileset_id = map["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    const Json* tileset = tileset_it == project->tilesets.end()
        ? nullptr : &tileset_it->second;
    const std::map<int, std::string> groups = tileset ? tile_groups(*tileset)
                                                     : std::map<int, std::string>{};
    const auto is_ocean_tile = [&groups](int tile_index) {
        const auto group = groups.find(tile_index);
        return group != groups.end() && group->second == "ocean";
    };
    const JsonArray* elevation_rows = map["layers"].contains("elevation") &&
        map["layers"]["elevation"].is_array()
        ? &map["layers"]["elevation"].as_array() : nullptr;
    const int sea_level = map.contains("worldgen") &&
        map["worldgen"].contains("sea_level")
        ? map["worldgen"]["sea_level"].as_int() : 500;
    const auto is_ocean_at = [&](int x, int y) {
        if (wraps_x) x = aur::wrapX(x, map_width);
        if (hd_surface && elevation_rows && y >= 0 &&
            y < static_cast<int>(elevation_rows->size())) {
            const Json& row = (*elevation_rows)[static_cast<size_t>(y)];
            if (row.is_array() && x >= 0 &&
                x < static_cast<int>(row.as_array().size())) {
                return row[static_cast<size_t>(x)].as_int() <= sea_level;
            }
        }
        if (!ground || y < 0 || y >= static_cast<int>(ground->size())) return false;
        const Json& row = (*ground)[static_cast<size_t>(y)];
        return row.is_array() && x >= 0 &&
            x < static_cast<int>(row.as_array().size()) &&
            is_ocean_tile(row[static_cast<size_t>(x)].as_int());
    };
    const int first_visible_x =
        static_cast<int>(std::floor((0.0 - bounds.left()) / cell));
    const int last_visible_x =
        static_cast<int>(std::ceil((width() - bounds.left()) / cell));
    const int first_x = wraps_x ? first_visible_x
        : std::clamp(first_visible_x, 0, map_width);
    const int last_x = wraps_x ? last_visible_x
        : std::clamp(last_visible_x, 0, map_width);
    const int first_y = std::clamp(static_cast<int>(std::floor((0.0 - bounds.top()) / cell)),
                                   0, map_height);
    const int last_y = std::clamp(static_cast<int>(std::ceil((height() - bounds.top()) / cell)),
                                  0, map_height);

    if (!gpuTerrain) painter.fillRect(bounds.adjusted(-1, -1, 1, 1), QColor("#080a0c"));
    if (hd_surface) {
        if (!gpuTerrain) {
            if (surface_cache_map != current_map || surface_cache.isNull()) {
                surface_cache = buildHdSurface(map);
                surface_cache_map = current_map;
            }
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            if (wraps_x) {
                const int first_copy = static_cast<int>(
                    std::floor((0.0 - bounds.left()) / bounds.width()));
                const int last_copy = static_cast<int>(
                    std::ceil((width() - bounds.left()) / bounds.width()));
                for (int copy = first_copy; copy < last_copy; ++copy) {
                    QRectF repeated_bounds = bounds;
                    repeated_bounds.moveLeft(bounds.left() + copy * bounds.width());
                    painter.drawImage(repeated_bounds, surface_cache);
                }
            } else {
                painter.drawImage(bounds, surface_cache);
            }
            painter.restore();
        }
        if (map["layers"].contains("terrain_overrides") &&
            map["layers"]["terrain_overrides"].is_array()) {
            const auto& overrides = map["layers"]["terrain_overrides"].as_array();
            for (int y = first_y; y < last_y && y < static_cast<int>(overrides.size()); ++y) {
                const Json& row = overrides[static_cast<size_t>(y)];
                if (!row.is_array()) continue;
                for (int x = first_x; x < last_x; ++x) {
                    const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                    if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                    const int tile_index = row[static_cast<size_t>(source_x)].as_int();
                    if (tile_index < 0) continue;
                    const QRectF tile_rect(bounds.left() + x * cell, bounds.top() + y * cell,
                                           cell, cell);
                    const QImage tile = tileImage(tileset_id, tile_index);
                    if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                }
            }
        }
    } else {
        if (!ground) return;
        for (int y = first_y; y < last_y && y < static_cast<int>(ground->size()); ++y) {
            const auto& row = (*ground)[static_cast<size_t>(y)].as_array();
            for (int x = first_x; x < last_x; ++x) {
                const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.size())) continue;
                int tile_index = row[static_cast<size_t>(source_x)].as_int();
                if (tileset && groups.contains(tile_index) && groups.at(tile_index) == "ocean") {
                    const int mask = mask_at(map, *tileset, "ground", source_x, y, "ocean", groups);
                    const auto& ocean_tiles = (*tileset)["autotiles"]["ocean"]["tiles"].as_array();
                    if (mask >= 0 && static_cast<size_t>(mask) < ocean_tiles.size()) {
                        tile_index = ocean_tiles[static_cast<size_t>(mask)].as_int();
                    }
                }
                const QRectF tile_rect(bounds.left() + x * cell, bounds.top() + y * cell,
                                       cell, cell);
                const QImage tile = tileImage(tileset_id, tile_index);
                if (!tile.isNull()) painter.drawImage(tile_rect, tile);
                else painter.fillRect(tile_rect, QColor("#34404a"));
            }
        }
    }

    if (!hd_surface && map["layers"].contains("biomes") &&
        map["layers"]["biomes"].is_array()) {
        const auto& biome_rows = map["layers"]["biomes"].as_array();
        for (int y = first_y; y < last_y && y < static_cast<int>(biome_rows.size()); ++y) {
            const Json& row = biome_rows[static_cast<size_t>(y)];
            if (!row.is_array()) continue;
            for (int x = first_x; x < last_x; ++x) {
                const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                const std::string biome_id = row[static_cast<size_t>(source_x)].as_string();
                const auto biome_it = project->biomes.find(biome_id);
                if (biome_it == project->biomes.end() || !biome_it->second["color"].is_string()) continue;
                QColor color(QString::fromStdString(biome_it->second["color"].as_string()));
                if (!color.isValid()) continue;
                color.setAlpha(38);
                painter.fillRect(QRectF(bounds.left() + x * cell, bounds.top() + y * cell, cell, cell),
                                 color);
            }
        }
    }

    if (temperature_overlay && map["layers"].contains("temperature") &&
        map["layers"]["temperature"].is_array()) {
        const auto& temperature_rows = map["layers"]["temperature"].as_array();
        const QColor cold("#548ed8");
        const QColor hot("#ee7652");
        for (int y = first_y; y < last_y && y < static_cast<int>(temperature_rows.size()); ++y) {
            const Json& row = temperature_rows[static_cast<size_t>(y)];
            if (!row.is_array()) continue;
            for (int x = first_x; x < last_x; ++x) {
                const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
                if (source_x < 0 || source_x >= static_cast<int>(row.as_array().size())) continue;
                const int value = row[static_cast<size_t>(source_x)].as_int();
                if (value < 0) continue;
                const qreal mix = std::clamp(value / 100.0, 0.0, 1.0);
                QColor color(
                    qRound(cold.red() + (hot.red() - cold.red()) * mix),
                    qRound(cold.green() + (hot.green() - cold.green()) * mix),
                    qRound(cold.blue() + (hot.blue() - cold.blue()) * mix), 92);
                painter.fillRect(QRectF(bounds.left() + x * cell, bounds.top() + y * cell,
                                        cell, cell), color);
            }
        }

        painter.save();
        painter.setPen(QPen(QColor("#f6e5ad"), std::max<qreal>(1.5, cell * 0.07),
                            Qt::DashLine));
        const qreal marker_radius = std::clamp(cell * 0.32, 4.0, 9.0);
        if (wraps_x) {
            painter.setClipRect(QRectF(0.0, bounds.top(), width(), bounds.height()),
                                Qt::IntersectClip);
            const qreal equator_y = bounds.top() + bounds.height() * 0.5;
            painter.drawLine(QPointF(0.0, equator_y), QPointF(width(), equator_y));
            painter.setPen(QPen(QColor("#172432"), 1.5));
            painter.setBrush(QColor("#70b8ff"));
            painter.drawEllipse(QPointF(12.0, bounds.top() + marker_radius),
                                marker_radius, marker_radius);
            painter.setBrush(QColor("#ff9a67"));
            painter.drawEllipse(QPointF(12.0, bounds.bottom() - marker_radius),
                                marker_radius, marker_radius);
            painter.setPen(QColor("#f6f2e8"));
            painter.drawText(QPointF(24.0, bounds.top() + marker_radius + 4), "N");
            painter.drawText(QPointF(24.0, bounds.bottom() - marker_radius + 4), "S");
        } else {
            const int angle_degrees = map.contains("worldgen") &&
                map["worldgen"].contains("axial_tilt")
                ? map["worldgen"]["axial_tilt"].as_int() : 0;
            const qreal angle = angle_degrees * 3.14159265358979323846 / 180.0;
            const QPointF center = bounds.center();
            const qreal direction_x = std::cos(angle);
            const qreal direction_y = -std::sin(angle);
            const qreal normal_x = std::sin(angle);
            const qreal normal_y = std::cos(angle);
            const qreal half_width = bounds.width() * 0.5;
            const qreal half_height = bounds.height() * 0.5;
            const qreal equator_extent = std::min(
                std::abs(direction_x) < 1e-6 ? std::numeric_limits<qreal>::max()
                                             : half_width / std::abs(direction_x),
                std::abs(direction_y) < 1e-6 ? std::numeric_limits<qreal>::max()
                                             : half_height / std::abs(direction_y));
            const qreal pole_extent = std::min(
                std::abs(normal_x) < 1e-6 ? std::numeric_limits<qreal>::max()
                                          : half_width / std::abs(normal_x),
                std::abs(normal_y) < 1e-6 ? std::numeric_limits<qreal>::max()
                                          : half_height / std::abs(normal_y));
            painter.drawLine(center - QPointF(direction_x, direction_y) * equator_extent,
                             center + QPointF(direction_x, direction_y) * equator_extent);
            const QPointF north = center - QPointF(normal_x, normal_y) * pole_extent;
            const QPointF south = center + QPointF(normal_x, normal_y) * pole_extent;
            painter.setPen(QPen(QColor("#172432"), 1.5));
            painter.setBrush(QColor("#70b8ff"));
            painter.drawEllipse(north, marker_radius, marker_radius);
            painter.setBrush(QColor("#ff9a67"));
            painter.drawEllipse(south, marker_radius, marker_radius);
            painter.setPen(QColor("#f6f2e8"));
            painter.drawText(north + QPointF(marker_radius + 3, 4), "N");
            painter.drawText(south + QPointF(marker_radius + 3, 4), "S");
        }
        painter.restore();
    }

    const int worldgen_seed = map.contains("worldgen") &&
        map["worldgen"].contains("seed") ? map["worldgen"]["seed"].as_int()
        : map.contains("seed") ? map["seed"].as_int() : 1;
    const int axis_degrees = map.contains("worldgen") &&
        map["worldgen"].contains("axial_tilt") ? map["worldgen"]["axial_tilt"].as_int() : 0;
    const qreal axis = axis_degrees * 3.14159265358979323846 / 180.0;
    const qreal axis_sine = std::sin(axis);
    const qreal axis_cosine = std::cos(axis);
    const qreal latitude_scale = std::max<qreal>(
        0.001, std::abs(axis_sine) + std::abs(axis_cosine));
    const qreal east_x = wraps_x ? 1.0 : axis_cosine;
    const qreal east_y = wraps_x ? 0.0 : -axis_sine;
    const qreal north_x = wraps_x ? 0.0 : axis_sine;
    const qreal north_y = wraps_x ? 1.0 : axis_cosine;

    if ((wind_overlay || currents_overlay) && cell > 0) {
        const int stride = std::max(1, static_cast<int>(std::round(28.0 / std::max(1.0, cell))));
        const auto draw_flow = [&](int x, int y, bool current) {
            const int source_x = wraps_x ? aur::wrapX(x, map_width) : x;
            const double nx = (source_x + 0.5) / map_width * 2.0 - 1.0;
            const double ny = (y + 0.5) / map_height * 2.0 - 1.0;
            const double latitude = wraps_x ? ny
                : (nx * axis_sine + ny * axis_cosine) / latitude_scale;
            double vx = 0.0;
            double vy = 0.0;
            if (current) {
                if (wraps_x) {
                    const double longitude = 2.0 * 3.14159265358979323846 *
                        (source_x + 0.5) / map_width;
                    const double gyre = std::sin(latitude * 3.14159265358979323846);
                    vx = std::cos(longitude) * gyre;
                    vy = -std::sin(longitude) * gyre * 0.35;
                } else {
                    const double center_x =
                        (x / static_cast<double>(map_width) < 0.5) ? 0.28 : 0.72;
                    const double center_y = latitude >= 0.0 ? 0.34 : 0.66;
                    const double dx = x / static_cast<double>(map_width) - center_x;
                    const double dy = y / static_cast<double>(map_height) - center_y;
                    const double spin = latitude >= 0.0 ? 1.0 : -1.0;
                    vx = -dy * spin;
                    vy = dx * spin;
                }
            } else {
                const double abs_latitude = std::abs(latitude);
                const double zonal = abs_latitude < 0.28 || abs_latitude > 0.68 ? -1.0 : 1.0;
                const double meridional = latitude == 0.0 ? 0.0 : -std::copysign(0.24, latitude);
                vx = east_x * zonal + north_x * meridional;
                vy = east_y * zonal + north_y * meridional;
            }
            const double magnitude = std::hypot(vx, vy);
            if (magnitude < 0.001) return;
            vx /= magnitude;
            vy /= magnitude;
            const QPointF center(bounds.left() + (x + 0.5) * cell,
                                 bounds.top() + (y + 0.5) * cell);
            const qreal length = std::clamp(cell * 0.65, 5.0, 15.0);
            const QPointF start = center - QPointF(vx, vy) * (length * 0.5);
            const QPointF end = center + QPointF(vx, vy) * (length * 0.5);
            const QColor flow_color = current ? QColor("#67d4e4") : QColor("#fff0b0");
            painter.setPen(QPen(flow_color, std::clamp(cell * 0.09, 1.0, 2.0),
                                Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(start, end);
            const QPointF side(-vy, vx);
            painter.drawLine(end, end - QPointF(vx, vy) * length * 0.32 + side * length * 0.22);
            painter.drawLine(end, end - QPointF(vx, vy) * length * 0.32 - side * length * 0.22);
            if (simulation_timer.isActive()) {
                const int phase = (simulation_phase + source_x * 7 + y * 13) % 80;
                const qreal travel = phase / 79.0 - 0.5;
                const QPointF particle = center + QPointF(vx, vy) * (travel * length);
                painter.setPen(Qt::NoPen);
                painter.setBrush(flow_color.lighter(145));
                painter.drawEllipse(particle, std::clamp(cell * 0.12, 1.2, 2.6),
                                    std::clamp(cell * 0.12, 1.2, 2.6));
            }
        };
        painter.save();
        const int first_flow_copy = wraps_x
            ? static_cast<int>(std::floor(static_cast<double>(first_x) / map_width)) : 0;
        const int last_flow_copy = wraps_x
            ? static_cast<int>(std::ceil(static_cast<double>(last_x) / map_width)) : 1;
        if (wind_overlay) {
            for (int y = stride / 2; y < map_height; y += stride) {
                for (int copy = first_flow_copy; copy < last_flow_copy; ++copy) {
                    for (int x = stride / 2; x < map_width; x += stride) {
                        const int world_x = copy * map_width + x;
                        if (world_x >= first_x && world_x < last_x) draw_flow(world_x, y, false);
                    }
                }
            }
        }
        if (currents_overlay) {
            for (int y = stride / 2; y < map_height; y += stride) {
                for (int copy = first_flow_copy; copy < last_flow_copy; ++copy) {
                    for (int x = stride / 2; x < map_width; x += stride) {
                        const int world_x = copy * map_width + x;
                        if (world_x >= first_x && world_x < last_x &&
                            is_ocean_at(world_x, y)) {
                            draw_flow(world_x, y, true);
                        }
                    }
                }
            }
        }
        painter.restore();
    }

    if (weather_overlay && cell > 0) {
        const int stride = std::max(8, static_cast<int>(std::round(72.0 / std::max(1.0, cell))));
        const QRectF weather_bounds = wraps_x
            ? QRectF(0.0, bounds.top(), width(), bounds.height()) : bounds;
        const JsonArray* temperature_rows = map["layers"].contains("temperature") &&
            map["layers"]["temperature"].is_array()
            ? &map["layers"]["temperature"].as_array() : nullptr;
        painter.save();
        painter.setClipRect(weather_bounds.intersected(QRectF(rect())), Qt::IntersectClip);
        if (preview_weather == "cloudy") {
            painter.fillRect(weather_bounds, QColor(53, 65, 75, 28));
        } else if (preview_weather == "rain") {
            painter.fillRect(weather_bounds, QColor(50, 87, 108, 34));
        } else if (preview_weather == "snow") {
            painter.fillRect(weather_bounds, QColor(203, 222, 234, 38));
        } else if (preview_weather == "storm") {
            painter.fillRect(weather_bounds, QColor(24, 34, 47, 76));
        } else if (preview_weather == "fog") {
            painter.fillRect(weather_bounds, QColor(199, 215, 223, 72));
        }
        if (preview_weather == "storm" && simulation_timer.isActive() &&
            simulation_phase % 90 < 3) {
            painter.fillRect(weather_bounds, QColor(220, 232, 240, 68));
        }
        const int first_weather_copy = wraps_x
            ? static_cast<int>(std::floor(static_cast<double>(first_x) / map_width)) : 0;
        const int last_weather_copy = wraps_x
            ? static_cast<int>(std::ceil(static_cast<double>(last_x) / map_width)) : 1;
        for (int y = stride / 2; y < map_height; y += stride) {
            for (int copy = first_weather_copy; copy < last_weather_copy; ++copy) {
            for (int local_x = stride / 2; local_x < map_width; local_x += stride) {
                const int world_x = copy * map_width + local_x;
                if (world_x < first_x || world_x >= last_x) continue;
                const int x = wraps_x ? aur::wrapX(world_x, map_width) : world_x;
                if (preview_weather == "clear" || preview_weather == "fog") continue;
                const uint32_t simulation_step = static_cast<uint32_t>(
                    simulation_timer.isActive() ? simulation_phase / 100 : 0);
                const uint32_t hash = static_cast<uint32_t>(worldgen_seed) ^
                    static_cast<uint32_t>(x) * 73856093u ^
                    static_cast<uint32_t>(y) * 19349663u ^
                    simulation_step * 83492791u ^
                    static_cast<uint32_t>(std::floor(preview_hour / 3.0)) * 2654435761u;
                const uint32_t mixed = (hash ^ (hash >> 13)) * 1274126177u;
                const uint32_t weather_roll = mixed ^ (mixed >> 16);
                const bool ocean = is_ocean_at(x, y);
                std::string climate;
                const JsonArray* biome_rows = map["layers"].contains("biomes") &&
                    map["layers"]["biomes"].is_array()
                    ? &map["layers"]["biomes"].as_array() : nullptr;
                if (biome_rows && y < static_cast<int>(biome_rows->size()) &&
                    (*biome_rows)[static_cast<size_t>(y)].is_array() &&
                    x < static_cast<int>((*biome_rows)[static_cast<size_t>(y)].as_array().size())) {
                    const std::string biome_id = (*biome_rows)[static_cast<size_t>(y)]
                        [static_cast<size_t>(x)].as_string();
                    const auto biome = project->biomes.find(biome_id);
                    if (biome != project->biomes.end() && biome->second["climate"].is_string()) {
                        climate = biome->second["climate"].as_string();
                    }
                }
                int temperature_value = 50;
                if (temperature_rows && y < static_cast<int>(temperature_rows->size()) &&
                    (*temperature_rows)[static_cast<size_t>(y)].is_array() &&
                    x < static_cast<int>((*temperature_rows)[static_cast<size_t>(y)].as_array().size())) {
                    temperature_value = (*temperature_rows)[static_cast<size_t>(y)]
                        [static_cast<size_t>(x)].as_int();
                }
                const std::string weather = preview_weather == "climate"
                    ? climate_weather(project, climate, weather_roll) : preview_weather;
                if (weather == "clear") continue;
                const uint32_t position_hash = static_cast<uint32_t>(worldgen_seed) ^
                    static_cast<uint32_t>(x) * 73856093u ^
                    static_cast<uint32_t>(y) * 19349663u;
                const uint32_t position_mixed =
                    (position_hash ^ (position_hash >> 13)) * 1274126177u;
                const uint32_t position_roll = position_mixed ^ (position_mixed >> 16);
                const uint32_t coverage = weather == "storm" ? 92u
                    : weather == "cloudy" ? 86u : 72u;
                if (position_roll % 100u >= coverage) continue;
                const bool snow = weather == "snow" ||
                    ((weather == "rain" || weather == "storm") &&
                     (climate == "arctic" ||
                      (temperature_value >= 0 && temperature_value < 30)));
                const bool precipitation =
                    weather == "rain" || weather == "snow" || weather == "storm";
                const qreal jitter_scale = stride * cell * 0.28;
                const qreal jitter_x =
                    (static_cast<qreal>(position_roll & 0xffffu) / 65535.0 - 0.5) *
                    jitter_scale;
                const qreal jitter_y =
                    (static_cast<qreal>(position_roll >> 16) / 65535.0 - 0.5) *
                    jitter_scale;
                const QPointF center(bounds.left() + (world_x + 0.5) * cell + jitter_x,
                                     bounds.top() + (y + 0.5) * cell + jitter_y);
                const qreal size_scale = 0.78 +
                    static_cast<qreal>((position_roll >> 8) & 0xffu) / 255.0 * 0.42;
                const qreal size = std::clamp(cell * 1.8 * size_scale, 12.0, 30.0);
                const qreal drift = simulation_timer.isActive()
                    ? std::sin((simulation_phase + x * 5 + y * 3) * 0.04) * size * 0.25 : 0.0;
                const QPointF cloud_center = center + QPointF(drift, 0.0);
                QColor cloud_color = weather == "storm" ? QColor("#53616c")
                    : snow ? QColor("#cfe5ef") : QColor("#d3d9d5");
                cloud_color.setAlpha(145 + static_cast<int>((position_roll >> 24) % 90u));
                QColor cloud_outline = weather == "storm" ? QColor("#26323c")
                                                           : QColor("#263746");
                cloud_outline.setAlpha(145 + static_cast<int>((position_roll >> 24) % 90u));
                painter.setPen(QPen(cloud_outline,
                                    std::max<qreal>(1.0, cell * 0.06)));
                painter.setBrush(cloud_color);
                painter.drawEllipse(QRectF(cloud_center.x() - size * 0.28,
                    cloud_center.y() - size * 0.02, size * 0.42, size * 0.32));
                painter.drawEllipse(QRectF(cloud_center.x() - size * 0.08,
                    cloud_center.y() - size * 0.20, size * 0.48, size * 0.50));
                painter.drawEllipse(QRectF(cloud_center.x() + size * 0.16,
                    cloud_center.y() - size * 0.02, size * 0.40, size * 0.32));
                if (precipitation && preview_weather == "climate") {
                    painter.setPen(QPen(snow ? QColor("#effaff") : QColor("#6dc5e6"),
                                        std::max<qreal>(1.0, cell * 0.07), Qt::SolidLine,
                                        Qt::RoundCap));
                    const qreal phase = simulation_timer.isActive()
                        ? std::fmod(simulation_phase * 0.45 + x * 3 + y, size) : size * 0.45;
                    for (int drop = -1; drop <= 1; ++drop) {
                        const qreal drop_x = cloud_center.x() + drop * size * 0.25;
                        const qreal drop_y = cloud_center.y() + size * 0.22 + phase * 0.15;
                        painter.drawLine(QPointF(drop_x, drop_y),
                                         QPointF(drop_x - size * 0.08, drop_y + size * 0.24));
                    }
                }
                }
            }
        }
        if (preview_weather == "rain" || preview_weather == "snow" ||
            preview_weather == "storm") {
            const QRectF visible_bounds = weather_bounds.intersected(QRectF(rect()));
            constexpr int spacing = 40;
            const int phase = simulation_timer.isActive()
                ? simulation_phase % spacing : spacing / 2;
            const bool snow = preview_weather == "snow";
            painter.setPen(QPen(
                snow ? QColor(239, 249, 255, 210)
                     : preview_weather == "storm" ? QColor(92, 175, 216, 190)
                                                  : QColor(105, 187, 224, 175),
                snow ? 1.0 : 1.3, Qt::SolidLine, Qt::RoundCap));
            const qreal start_y = visible_bounds.top() +
                std::fmod(phase + spacing, spacing);
            for (qreal y = start_y; y < visible_bounds.bottom(); y += spacing) {
                const int row = static_cast<int>(
                    std::floor((y - visible_bounds.top()) / spacing));
                const qreal row_offset = (row & 1) != 0 ? spacing * 0.5 : 0.0;
                for (qreal x = visible_bounds.left() + row_offset;
                     x < visible_bounds.right(); x += spacing) {
                    const int column = static_cast<int>(std::floor(
                        (x - visible_bounds.left() - row_offset) / spacing));
                    const uint32_t particle_hash = static_cast<uint32_t>(worldgen_seed) ^
                        static_cast<uint32_t>(row) * 73856093u ^
                        static_cast<uint32_t>(column) * 19349663u;
                    const uint32_t particle_mixed =
                        (particle_hash ^ (particle_hash >> 13)) * 1274126177u;
                    const uint32_t particle_roll = particle_mixed ^ (particle_mixed >> 16);
                    const qreal jitter_x =
                        (static_cast<qreal>(particle_roll & 0xffu) / 255.0 - 0.5) *
                        spacing * 0.5;
                    const qreal jitter_y =
                        (static_cast<qreal>((particle_roll >> 8) & 0xffu) / 255.0 - 0.5) *
                        spacing * 0.35;
                    const qreal particle_x = x + jitter_x;
                    const qreal particle_y = y + jitter_y;
                    if (snow) {
                        const qreal drift = std::sin(
                            (simulation_phase + static_cast<int>(particle_x * 3 + particle_y)) *
                            0.025) * 3.0;
                        painter.drawLine(QPointF(particle_x + drift - 2.0, particle_y),
                                         QPointF(particle_x + drift + 2.0, particle_y));
                        painter.drawLine(QPointF(particle_x + drift, particle_y - 2.0),
                                         QPointF(particle_x + drift, particle_y + 2.0));
                    } else {
                        painter.drawLine(QPointF(particle_x, particle_y),
                                         QPointF(particle_x - 4.0, particle_y + 10.0));
                    }
                }
            }
        }
        painter.restore();
    }

    if (daynight_overlay && (!world_model || editor::isHeadlessRhi())) {
        GameClock lighting = project_clock(project);
        lighting.set_time(preview_hour);
        const auto [rgb, darkness] = lighting.ambient();
        QColor tint(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb));
        tint.setAlphaF(std::clamp(darkness, 0.0, 1.0));
        const QRectF lighting_bounds = wraps_x
            ? QRectF(0.0, bounds.top(), width(), bounds.height()) : bounds;
        painter.fillRect(lighting_bounds, tint);
    }

    if (map.contains("entities") && map["entities"].is_array()) {
        const auto& entities = map["entities"].as_array();
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& entity = entities[index];
            int x = entity["x"].as_int();
            const int y = entity["y"].as_int();
            if (y < 0 || y >= map_height || (!wraps_x && (x < 0 || x >= map_width))) continue;
            if (wraps_x) x = aur::wrapX(x, map_width);
            const qreal marker_x = bounds.left() + (x + 0.5) * cell;
            const int first_marker_copy = wraps_x
                ? static_cast<int>(std::floor((0.0 - marker_x) / bounds.width())) : 0;
            const int last_marker_copy = wraps_x
                ? static_cast<int>(std::ceil((width() - marker_x) / bounds.width())) : 1;
            const bool is_exit = entity.contains("type") && entity["type"].as_string() == "exit";
            const QColor marker = is_exit ? QColor("#d596ff") : QColor("#ffd27a");
            const qreal radius = std::clamp(cell * 0.32, 4.0, 8.0);
            for (int copy = first_marker_copy; copy < last_marker_copy; ++copy) {
                const QPointF center(marker_x + copy * bounds.width(),
                                     bounds.top() + (y + 0.5) * cell);
                painter.setPen(QPen(index == static_cast<size_t>(selected_node)
                                        ? Qt::white : QColor("#202329"),
                                    index == static_cast<size_t>(selected_node) ? 2.0 : 1.0));
                painter.setBrush(marker);
                if (is_exit) {
                    QPolygonF diamond;
                    diamond << QPointF(center.x(), center.y() - radius)
                            << QPointF(center.x() + radius, center.y())
                            << QPointF(center.x(), center.y() + radius)
                            << QPointF(center.x() - radius, center.y());
                    painter.drawPolygon(diamond);
                } else {
                    painter.drawEllipse(center, radius, radius);
                }

                QString label;
                if (is_exit && entity.contains("target") && entity["target"].contains("map")) {
                    label = QString::fromStdString(entity["target"]["map"].as_string());
                } else if (entity.contains("id")) {
                    label = QString::fromStdString(entity["id"].as_string());
                }
                if (!label.isEmpty() && zoom_factor >= 0.75) {
                    const QFontMetrics metrics(painter.font());
                    const QRect textRect = metrics.boundingRect(label).adjusted(-4, -2, 4, 2);
                    const QPoint labelOrigin(static_cast<int>(center.x() + radius + 3),
                                             static_cast<int>(center.y() - textRect.height() / 2));
                    painter.fillRect(QRect(labelOrigin, textRect.size()), QColor(14, 17, 21, 210));
                    painter.setPen(QColor("#f2f4f6"));
                    painter.drawText(labelOrigin.x() + 4, labelOrigin.y() + metrics.ascent(), label);
                }
            }
        }
    }

    if (hover_tile.x() >= 0 && hover_tile.x() < map_width &&
        hover_tile.y() >= 0 && hover_tile.y() < map_height) {
        qreal hover_x = bounds.left() + hover_tile.x() * cell;
        if (wraps_x) {
            hover_x += std::round((width() * 0.5 - hover_x) / bounds.width()) * bounds.width();
        }
        const QRectF hover(hover_x,
                           bounds.top() + hover_tile.y() * cell, cell, cell);
        painter.setPen(QPen(QColor("#ffffff"), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(hover);
    }

    painter.setPen(QColor("#c1c8ce"));
    const QString zoomText = hd_surface
        ? QString("%1 × %2 tiles  ·  %3  ·  1 art px = %4 screen px")
              .arg(map_width).arg(map_height)
              .arg(QString::fromStdString(map["tileset"].as_string()))
              .arg(QString::number(zoom_factor, 'g', 3))
        : QString("%1 × %2 tiles  ·  %3  ·  %4%")
              .arg(map_width).arg(map_height)
              .arg(QString::fromStdString(map["tileset"].as_string()))
              .arg(qRound(zoom_factor * 100.0));
    painter.drawText(QRectF(16, 12, width() - 32, 20),
                     Qt::AlignLeft | Qt::AlignVCenter, zoomText);
}

void WorldMapCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning = true;
        pan_start_position = event->position();
        pan_start_offset = pan_offset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        stroke_active = false;
        stroke_model_rects.clear();
        stroke_captured_cells.clear();
        const QPoint tile = tileAt(event->position());
        if ((selected_tool == 3 || selected_tool == 4) &&
            tile.x() >= 0 && tile.y() >= 0) {
            emit entityPlacementRequested(tile, selected_tool);
            event->accept();
            return;
        }
        if (selected_tool == 0 && project && project->maps.contains(current_map) &&
            tile.x() >= 0 && tile.y() >= 0) {
            const Json& map = project->maps.at(current_map);
            if (!map.contains("entities") || !map["entities"].is_array()) {
                emit entitySelected(-1);
                return;
            }
            const auto& entities = map["entities"].as_array();
            int nearest = -1;
            qreal nearest_distance = std::numeric_limits<qreal>::max();
            for (size_t index = 0; index < entities.size(); ++index) {
                const Json& entity = entities[index];
                const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
                if (type != "location" && type != "exit") continue;
                const qreal dx = entity["x"].as_int() - tile.x();
                const qreal dy = entity["y"].as_int() - tile.y();
                const qreal distance = std::hypot(dx, dy);
                if (distance <= 1.5 && distance < nearest_distance) {
                    nearest = static_cast<int>(index);
                    nearest_distance = distance;
                }
            }
            emit entitySelected(nearest);
        }
        const bool modelStroke = selected_tool == 5 || selected_tool == 6 ||
            (selected_tool == 2 && project && project->maps.contains(current_map) &&
             has_hd_surface(project->maps.at(current_map)));
        if ((selected_tool == 1 || (selected_tool == 2 && !modelStroke)) && project &&
            project->maps.contains(current_map) && tile.x() >= 0 && tile.y() >= 0) {
            undo_actions.push_back({false, project->maps[current_map]["layers"]});
            if (undo_actions.size() > 100) undo_actions.erase(undo_actions.begin());
            redo_actions.clear();
        }
        if ((selected_tool == 1 || selected_tool == 2 || modelStroke) &&
            project && project->maps.contains(current_map) && tile.x() >= 0 && tile.y() >= 0) {
            stroke_active = true;
            stroke_last_tile = tile;
        }
        paintTile(tile);
    }
}

void WorldMapCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning && event->buttons().testFlag(Qt::MiddleButton)) {
        pan_offset = pan_start_offset + event->position() - pan_start_position;
        constrainWorldCamera();
        refreshCanvas();
        return;
    }
    const QPoint tile = tileAt(event->position());
    if (tile != hover_tile) {
        hover_tile = tile;
        refreshCanvas();
    }
    if (stroke_active && event->buttons().testFlag(Qt::LeftButton) &&
        tile.x() >= 0 && tile.y() >= 0 && tile != stroke_last_tile) {
        const QPoint start = stroke_last_tile;
        const bool wraps_x = project && project->maps.contains(current_map) &&
            aur::wraps_world_x(project->maps.at(current_map));
        const int width = wraps_x
            ? project->maps.at(current_map)["w"].as_int() : 0;
        const int delta_x = wraps_x
            ? aur::wrapped_delta_x(aur::wrapX(start.x(), width),
                                   aur::wrapX(tile.x(), width), width)
            : tile.x() - start.x();
        const int steps = std::max(std::abs(delta_x),
                                   std::abs(tile.y() - start.y()));
        for (int step = 1; step <= steps; ++step) {
            const double fraction = static_cast<double>(step) / steps;
            paintTile(QPoint(qRound(start.x() + delta_x * fraction),
                             qRound(start.y() + (tile.y() - start.y()) * fraction)));
        }
        stroke_last_tile = tile;
    }
}

void WorldMapCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (!stroke_model_rects.empty()) {
            UndoAction action;
            action.model_rects = std::move(stroke_model_rects);
            undo_actions.push_back(std::move(action));
            if (undo_actions.size() > 100) undo_actions.erase(undo_actions.begin());
            redo_actions.clear();
        }
        stroke_model_rects.clear();
        stroke_captured_cells.clear();
        stroke_active = false;
        stroke_last_tile = {-1, -1};
    }
    if (event->button() == Qt::MiddleButton) {
        panning = false;
        setCursor(selected_tool > 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void WorldMapCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const QPointF cursor = event->position();
    const QRectF old_bounds = mapBounds();
    const Json& map = project->maps.at(current_map);
    const qreal old_cell = old_bounds.width() / map["w"].as_int();
    const QPointF map_position((cursor.x() - old_bounds.left()) / old_cell,
                               (cursor.y() - old_bounds.top()) / old_cell);
    qreal updated_zoom = zoom_factor;
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        constexpr std::array<qreal, 5> snapSteps{1.0, 2.0, 4.0, 8.0, 16.0};
        int snapIndex = 0;
        for (int index = 1; index < static_cast<int>(snapSteps.size()); ++index) {
            if (std::abs(std::log(snapSteps[static_cast<std::size_t>(index)] / zoom_factor)) <
                std::abs(std::log(snapSteps[static_cast<std::size_t>(snapIndex)] / zoom_factor))) {
                snapIndex = index;
            }
        }
        if (event->angleDelta().y() > 0) {
            while (snapIndex + 1 < static_cast<int>(snapSteps.size()) &&
                   snapSteps[static_cast<std::size_t>(snapIndex)] <= zoom_factor + 0.001) {
                ++snapIndex;
            }
        } else {
            while (snapIndex > 0 && snapSteps[static_cast<std::size_t>(snapIndex)] >=
                   zoom_factor - 0.001) {
                --snapIndex;
            }
        }
        updated_zoom = snapSteps[static_cast<std::size_t>(snapIndex)];
    } else {
        const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        updated_zoom = std::clamp(zoom_factor * factor, 0.05, 16.0);
    }
    if (updated_zoom == zoom_factor) return;
    zoom_factor = updated_zoom;
    const QRectF new_bounds = mapBounds();
    const qreal new_cell = new_bounds.width() / map["w"].as_int();
    pan_offset += cursor - QPointF(new_bounds.left() + map_position.x() * new_cell,
                                   new_bounds.top() + map_position.y() * new_cell);
    constrainWorldCamera();
    refreshCanvas();
    emit zoomChanged(zoom_factor);
    event->accept();
}

void WorldMapCanvasWidget::leaveEvent(QEvent* event) {
    hover_tile = {-1, -1};
    refreshCanvas();
    QWidget::leaveEvent(event);
}

WorldPanel::WorldPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("WORLD MAP", this);
    title->setStyleSheet("font-size: 15px; font-weight: 700; color: #e8edf2; letter-spacing: 1px;");
    header->addWidget(title);
    header->addSpacing(14);
    header->addWidget(new QLabel("Map:", this));
    mapCombo = new QComboBox(this);
    mapCombo->setMinimumWidth(220);
    header->addWidget(mapCombo);
    header->addSpacing(12);
    header->addWidget(new QLabel("Tool:", this));
    toolCombo = new QComboBox(this);
    toolCombo->addItems({"Inspect", "Paint terrain", "Paint biome",
                         "Place location", "Place exit", "Raise height", "Lower height"});
    header->addWidget(toolCombo);
    auto* undoButton = new QPushButton("Undo", this);
    auto* redoButton = new QPushButton("Redo", this);
    header->addWidget(undoButton);
    header->addWidget(redoButton);
    header->addSpacing(10);
    header->addWidget(new QLabel("Zoom:", this));
    zoomSpin = new QSpinBox(this);
    zoomSpin->setRange(5, 1600);
    zoomSpin->setSingleStep(25);
    zoomSpin->setValue(100);
    zoomSpin->setSuffix("%");
    header->addWidget(zoomSpin);
    auto* fitButton = new QPushButton("Fit", this);
    header->addWidget(fitButton);
    auto* overlayButton = new QToolButton(this);
    overlayButton->setText("Climate layers");
    overlayButton->setPopupMode(QToolButton::InstantPopup);
    auto* overlayMenu = new QMenu(overlayButton);
    QAction* temperatureAction = overlayMenu->addAction("Temperature / equator / poles");
    temperatureAction->setCheckable(true);
    QAction* windAction = overlayMenu->addAction("Wind");
    windAction->setCheckable(true);
    QAction* currentsAction = overlayMenu->addAction("Ocean currents");
    currentsAction->setCheckable(true);
    QAction* weatherAction = overlayMenu->addAction("Weather");
    weatherAction->setCheckable(true);
    QAction* dayNightAction = overlayMenu->addAction("Day/night lighting");
    dayNightAction->setCheckable(true);
    dayNightAction->setChecked(true);
    QAction* contoursAction = overlayMenu->addAction("Terrain contours and pixel grid");
    contoursAction->setCheckable(true);
    contoursAction->setChecked(true);
    overlayMenu->addSeparator();
    QAction* simulationAction = overlayMenu->addAction("Run simulation");
    simulationAction->setCheckable(true);
    overlayButton->setMenu(overlayMenu);
    header->addWidget(new QLabel("Weather:", this));
    weatherCombo = new QComboBox(this);
    weatherCombo->addItem("Climate", "climate");
    weatherCombo->addItem("Clear", "clear");
    weatherCombo->addItem("Cloudy", "cloudy");
    weatherCombo->addItem("Rain", "rain");
    weatherCombo->addItem("Snow", "snow");
    weatherCombo->addItem("Storm", "storm");
    weatherCombo->addItem("Fog", "fog");
    weatherCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    weatherCombo->setMinimumContentsLength(7);
    weatherCombo->setToolTip(
        "Preview a climate-driven weather mix or force a weather condition on the world map.");
    timeOfDayEdit = new QTimeEdit(QTime(8, 0), this);
    timeOfDayEdit->setDisplayFormat("HH:mm");
    timeOfDayEdit->setToolTip("Preview world lighting and terrain shading at this local time.");
    header->addWidget(overlayButton);
    header->addWidget(weatherCombo);
    header->addWidget(new QLabel("Time:", this));
    header->addWidget(timeOfDayEdit);
    header->addStretch(1);
    generateWorldButton = new QPushButton("Generate new world…", this);
    header->addWidget(generateWorldButton);
    mapInfo = new QLabel(this);
    mapInfo->setStyleSheet("color: #aeb8c2;");
    header->addWidget(mapInfo);
    layout->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* sidebar = new QWidget(splitter);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(0, 0, 8, 0);
    sidebar_layout->setSpacing(6);

    auto* biome_title = new QLabel("BIOMES", sidebar);
    biome_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(biome_title);
    biomeList = new QListWidget(sidebar);
    biomeList->setMaximumHeight(190);
    sidebar_layout->addWidget(biomeList);
    newBiomeButton = new QPushButton("Create biome…", sidebar);
    sidebar_layout->addWidget(newBiomeButton);

    auto* terrain_title = new QLabel("TERRAIN", sidebar);
    terrain_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(terrain_title);
    auto* landformRow = new QHBoxLayout();
    landformCombo = new QComboBox(sidebar);
    landformCombo->addItems({"Selected tile", "River", "Lake", "Beach", "Mountain range",
                             "Hills", "Cave"});
    brushSizeSpin = new QSpinBox(sidebar);
    brushSizeSpin->setRange(1, 15);
    brushSizeSpin->setSingleStep(2);
    brushSizeSpin->setValue(1);
    brushSizeSpin->setToolTip("Odd brush width in map tiles; use a larger brush for lakes and ranges.");
    landformRow->addWidget(landformCombo, 1);
    landformRow->addWidget(brushSizeSpin);
    sidebar_layout->addLayout(landformRow);
    auto* tileToolsRow = new QHBoxLayout();
    tileCategoryCombo = new QComboBox(sidebar);
    tileCategoryCombo->addItem("All terrain", "all");
    tileCategoryCombo->addItem("Biome starters", "biomes");
    tileCategoryCombo->addItem("Surface", "surface");
    tileCategoryCombo->addItem("Coasts", "coast");
    tileCategoryCombo->addItem("Underwater", "underwater");
    tileCategoryCombo->addItem("Elevation", "elevation");
    tileCategoryCombo->addItem("Structures", "structures");
    tileCategoryCombo->setToolTip("Filter the terrain tile palette by environment.");
    auto* addStarterTilesButton = new QPushButton("Add starter set", sidebar);
    addStarterTilesButton->setToolTip(
        "Add 9 general terrain tiles and original terrain for all 12 project biomes.");
    tileToolsRow->addWidget(tileCategoryCombo, 1);
    tileToolsRow->addWidget(addStarterTilesButton);
    sidebar_layout->addLayout(tileToolsRow);
    tilePalette = new QListWidget(sidebar);
    tilePalette->setSelectionMode(QAbstractItemView::SingleSelection);
    tilePalette->setMaximumHeight(190);
    sidebar_layout->addWidget(tilePalette);
    auto* tileHelp = new QLabel(
        "Select a tile, then Paint terrain and click/drag. HD maps layer tiles over relief.", sidebar);
    tileHelp->setWordWrap(true);
    tileHelp->setStyleSheet("color: #8d98a3;");
    sidebar_layout->addWidget(tileHelp);

    auto* nodes_title = new QLabel("LOCATIONS & EXITS", sidebar);
    nodes_title->setStyleSheet("font-weight: 700; color: #c5cdd5; letter-spacing: 1px;");
    sidebar_layout->addWidget(nodes_title);
    nodeList = new QListWidget(sidebar);
    sidebar_layout->addWidget(nodeList, 1);
    auto* node_buttons = new QHBoxLayout();
    addLocationButton = new QPushButton("Add Location", sidebar);
    addExitButton = new QPushButton("Add Exit", sidebar);
    node_buttons->addWidget(addLocationButton);
    node_buttons->addWidget(addExitButton);
    sidebar_layout->addLayout(node_buttons);
    auto* node_actions = new QHBoxLayout();
    editNodeButton = new QPushButton("Edit", sidebar);
    deleteNodeButton = new QPushButton("Delete", sidebar);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    node_actions->addWidget(editNodeButton);
    node_actions->addWidget(deleteNodeButton);
    sidebar_layout->addLayout(node_actions);
    splitter->addWidget(sidebar);

    mapCanvas = new WorldMapCanvasWidget(splitter);
    mapCanvas->setProject(project);
    splitter->addWidget(mapCanvas);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({235, 760});
    layout->addWidget(splitter, 1);
    connect(undoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::undo);
    connect(redoButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::redo);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, mapCanvas, &WorldMapCanvasWidget::redo);

    auto* footer = new QLabel("Wheel: zoom · Ctrl+wheel: snap 1/2/4/8/16× · middle-drag: pan · "
                              "Keys 1-7: tool · Ctrl+Z/Y: undo/redo · "
                              "Raise/Lower height or paint biome · Add Location/Exit, then click · "
                              "Fit: show the whole world · Save with Ctrl+S.", this);
    footer->setStyleSheet("color: #8d98a3;");
    layout->addWidget(footer);

    connect(mapCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) selectMap(mapCombo->itemData(index).toString().toStdString());
    });
    connect(toolCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { mapCanvas->setTool(index); });
    for (int key = 1; key <= 7; ++key) {
        QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_0 + key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key]() {
            toolCombo->setCurrentIndex(key - 1);
        });
    }
    toolCombo->setToolTip("Tool (keys 1-7: Inspect, Paint terrain, Paint biome, "
                          "Place location, Place exit, Raise height, Lower height)");
    connect(brushSizeSpin, &QSpinBox::valueChanged, mapCanvas,
            &WorldMapCanvasWidget::setBrushSize);
    connect(tileCategoryCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { filterTerrainTiles(); });
    connect(addStarterTilesButton, &QPushButton::clicked, this,
            [this]() { addStarterTerrainTiles(); });
    connect(landformCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        static const std::array<int, 7> tiles = {-1, 7, 7, 6, 4, 5, 72};
        static const std::array<int, 7> sizes = {1, 1, 5, 3, 5, 3, 1};
        if (index <= 0 || index >= static_cast<int>(tiles.size())) return;
        if (!project || !project->maps.contains(current_map)) return;
        const auto tileset_it = project->tilesets.find(
            project->maps[current_map]["tileset"].as_string());
        if (tileset_it == project->tilesets.end() ||
            tiles[index] >= static_cast<int>(tileset_it->second["tiles"].as_array().size())) {
            QMessageBox::warning(this, "Landform brush",
                                 "This world tileset is missing the required landform tile.");
            landformCombo->setCurrentIndex(0);
            return;
        }
        mapCanvas->setSelectedTile(tiles[index]);
        brushSizeSpin->setValue(sizes[index]);
        toolCombo->setCurrentIndex(1);
        for (int row = 0; row < tilePalette->count(); ++row) {
            if (tilePalette->item(row)->data(Qt::UserRole).toInt() == tiles[index]) {
                tilePalette->setCurrentRow(row);
                break;
            }
        }
    });
    connect(zoomSpin, &QSpinBox::valueChanged, this, [this](int value) {
        mapCanvas->setZoom(value / 100.0);
    });
    connect(fitButton, &QPushButton::clicked, mapCanvas, &WorldMapCanvasWidget::fitToView);
    connect(temperatureAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setTemperatureOverlay);
    connect(windAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setWindOverlay);
    connect(currentsAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setCurrentsOverlay);
    connect(weatherAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setWeatherOverlay);
    connect(weatherCombo, &QComboBox::currentIndexChanged, this,
            [this, weatherAction](int index) {
                const std::string weather =
                    weatherCombo->itemData(index).toString().toStdString();
                mapCanvas->setWeatherType(weather);
                if (weather != "climate" && !weatherAction->isChecked()) {
                    weatherAction->setChecked(true);
                }
            });
    connect(dayNightAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setDayNightOverlay);
    connect(contoursAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setContoursEnabled);
    connect(simulationAction, &QAction::toggled, mapCanvas,
            &WorldMapCanvasWidget::setSimulationRunning);
    connect(timeOfDayEdit, &QTimeEdit::timeChanged, this, [this](const QTime& time) {
        mapCanvas->setTimeOfDay(time.hour(), time.minute());
    });
    connect(mapCanvas, &WorldMapCanvasWidget::timeOfDayChanged, this,
            [this](int hour, int minute) {
                const QSignalBlocker blocker(timeOfDayEdit);
                timeOfDayEdit->setTime(QTime(hour, minute));
            });
    connect(mapCanvas, &WorldMapCanvasWidget::zoomChanged, this, [this](qreal zoom) {
        const QSignalBlocker blocker(zoomSpin);
        zoomSpin->setValue(qRound(zoom * 100.0));
    });
    connect(tilePalette, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const int tile = tilePalette->item(row)->data(Qt::UserRole).toInt();
        mapCanvas->setSelectedTile(tile);
        if (landformCombo->currentIndex() != 0) landformCombo->setCurrentIndex(0);
    });
    connect(biomeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        selected_biome = biomeList->item(row)->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    });
    connect(nodeList, &QListWidget::currentRowChanged, this, [this](int row) {
        const QListWidgetItem* item = row >= 0 ? nodeList->item(row) : nullptr;
        const int entity_index = item ? item->data(Qt::UserRole).toInt() : -1;
        mapCanvas->setSelectedNode(entity_index);
        editNodeButton->setEnabled(entity_index >= 0);
        deleteNodeButton->setEnabled(entity_index >= 0);
    });
    connect(nodeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { editWorldEntity(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entitySelected, this, [this](int entity_index) {
        int row = -1;
        for (int index = 0; index < nodeList->count(); ++index) {
            if (nodeList->item(index)->data(Qt::UserRole).toInt() == entity_index) {
                row = index;
                break;
            }
        }
        nodeList->setCurrentRow(row);
    });
    connect(mapCanvas, &WorldMapCanvasWidget::entitiesChanged, this,
            [this]() { refreshMapContents(); });
    connect(mapCanvas, &WorldMapCanvasWidget::entityPlacementRequested, this,
            [this](const QPoint& tile, int tool) { createWorldEntity(tile, tool); });
    connect(addLocationButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(3);
    });
    connect(addExitButton, &QPushButton::clicked, this, [this]() {
        toolCombo->setCurrentIndex(4);
    });
    connect(editNodeButton, &QPushButton::clicked, this, [this]() { editWorldEntity(); });
    connect(deleteNodeButton, &QPushButton::clicked, this, [this]() { deleteWorldEntity(); });
    connect(newBiomeButton, &QPushButton::clicked, this, [this]() { createBiome(); });
    connect(generateWorldButton, &QPushButton::clicked, this, [this]() {
        if (!project || !project->tilesets.contains("world") ||
            !project->tilesets["world"].contains("tiles") ||
            project->tilesets["world"]["tiles"].as_array().size() < 8) {
            QMessageBox::warning(this, "Generate world", "World generation needs a world tileset with at least 8 terrain tiles.");
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle("Generate a new world");
        QFormLayout form(&dialog);
        auto* namingHint = new QLabel(
            "Create an original spherical world with editable terrain and a shaded, high-detail surface. "
            "The seed reproduces the same landforms; repeated seeds get a numbered copy.",
            &dialog);
        namingHint->setWordWrap(true);
        form.addRow(namingHint);
        auto* topology = new QComboBox(&dialog);
        topology->addItem("Natural continents", "continents");
        topology->addItem("Island chains", "archipelago");
        topology->addItem("One vast supercontinent", "supercontinent");
        topology->addItem("Fractured / mountainous", "fractured");
        topology->setToolTip("Choose the broad landform style. A new seed changes the actual geography.");
        form.addRow("World topology:", topology);
        auto* width = new QSpinBox(&dialog);
        width->setRange(24, 2560);
        width->setValue(256);
        auto* height = new QSpinBox(&dialog);
        height->setRange(16, 1280);
        height->setValue(128);
        form.addRow("Width (tiles):", width);
        form.addRow("Height (tiles):", height);
        auto* resolutionHint = new QLabel(
            "Default is 256 × 128 tiles (the Sylvi diorama scale). Larger maps remain available "
            "up to 2560 × 1280, but can use several GB of editor memory.",
            &dialog);
        resolutionHint->setWordWrap(true);
        form.addRow(resolutionHint);
        auto* seed = new QSpinBox(&dialog);
        seed->setRange(-1000000000, 1000000000);
        seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        auto* randomSeedButton = new QPushButton("Randomize", &dialog);
        auto* seedRow = new QHBoxLayout();
        seedRow->addWidget(seed);
        seedRow->addWidget(randomSeedButton);
        form.addRow("Seed:", seedRow);
        connect(randomSeedButton, &QPushButton::clicked, &dialog, [seed]() {
            seed->setValue(QRandomGenerator::global()->bounded(-1000000000, 1000000000));
        });
        auto* water = new QSpinBox(&dialog);
        water->setRange(0, 100);
        water->setValue(50);
        water->setSuffix("%");
        form.addRow("Water level:", water);
        auto* vegetation = new QSpinBox(&dialog);
        vegetation->setRange(0, 100);
        vegetation->setValue(65);
        vegetation->setSuffix("%");
        form.addRow("Vegetation:", vegetation);
        auto* decay = new QSpinBox(&dialog);
        decay->setRange(0, 100);
        decay->setValue(15);
        decay->setSuffix("%");
        form.addRow("Decay / ruins:", decay);
        auto* axialTilt = new QSpinBox(&dialog);
        axialTilt->setRange(0, 359);
        axialTilt->setValue(0);
        axialTilt->setSuffix("°");
        axialTilt->setToolTip("0° places the equator horizontally; this rotates the equator and pole axis on the map.");
        form.addRow("Axial tilt / equator angle:", axialTilt);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;

        const int seed_value = seed->value();
        const std::string seed_text = std::to_string(seed_value);
        const std::string base_id = make_biome_id("new_world_seed_" + seed_text);
        const std::string base_name = "New World " + seed_text;
        std::string id = base_id;
        std::string name = base_name;
        int copy_number = 2;
        while (project->maps.contains(id)) {
            id = base_id + "_" + std::to_string(copy_number);
            name = base_name + " (" + std::to_string(copy_number) + ")";
            ++copy_number;
        }
        const int requested_width = width->value();
        const int requested_height = height->value();
        const std::string requested_topology = topology->currentData().toString().toStdString();
        const int requested_water = water->value();
        const int requested_vegetation = vegetation->value();
        const int requested_decay = decay->value();
        const int requested_tilt = axialTilt->value();
        const Project* const source_project = project;
        const std::uint64_t request = ++generationRequest;
        if (generationCancelled) generationCancelled->store(true);
        generationCancelled = std::make_shared<std::atomic_bool>(false);
        const auto cancelled = generationCancelled;
        auto* watcher = new QFutureWatcher<Json>(this);
        generationProgress = new QProgressDialog(
            "Generating spherical world…", "Cancel", 0, 0, this);
        generationProgress->setWindowTitle("Generate world");
        generationProgress->setWindowModality(Qt::WindowModal);
        generationProgress->setMinimumDuration(0);
        generationProgress->setAutoClose(false);
        generationProgress->setAutoReset(false);
        connect(generationProgress, &QProgressDialog::canceled, this, [cancelled]() {
            cancelled->store(true);
        });
        connect(watcher, &QFutureWatcher<Json>::finished, this,
                [this, watcher, cancelled, request, source_project, id, name]() {
            generationProgress->hide();
            if (request == generationRequest && project == source_project && !cancelled->load()) {
                try {
                    project->maps[id] = watcher->result();
                    current_map = id;
                    refreshGraph();
                } catch (const std::exception& error) {
                    QMessageBox::warning(this, "World generation failed",
                                         QString::fromStdString(error.what()));
                }
            }
            watcher->deleteLater();
            generationProgress->deleteLater();
            generationProgress = nullptr;
        });
        watcher->setFuture(QtConcurrent::run(
            [id, name, requested_width, requested_height, seed_value, requested_topology,
             requested_water, requested_vegetation, requested_decay, requested_tilt,
             cancelled]() {
                return generate_world_map(id, name, requested_width, requested_height,
                    seed_value, requested_topology, requested_water, requested_vegetation,
                    requested_decay, requested_tilt, cancelled.get());
            }));
    });

    refreshGraph();
}

void WorldPanel::setProject(Project* p) {
    project = p;
    mapCanvas->setProject(p);
    refreshGraph();
}

void WorldPanel::refreshGraph() {
    if (!mapCombo) return;
    mapCombo->blockSignals(true);
    mapCombo->clear();

    if (project) {
        for (const auto& [map_id, map] : project->maps) {
            if (map.contains("kind") && map["kind"].as_string() == "world") {
                const std::string name = map.contains("name") ? map["name"].as_string() : map_id;
                mapCombo->addItem(QString::fromStdString(name), QString::fromStdString(map_id));
            }
        }
    }

    int selected_index = -1;
    for (int index = 0; index < mapCombo->count(); ++index) {
        if (mapCombo->itemData(index).toString().toStdString() == current_map) {
            selected_index = index;
            break;
        }
    }
    if (selected_index < 0 && mapCombo->count() > 0) selected_index = 0;
    mapCombo->setCurrentIndex(selected_index);
    mapCombo->blockSignals(false);

    if (selected_index >= 0) selectMap(mapCombo->itemData(selected_index).toString().toStdString());
    else {
        current_map.clear();
        mapCanvas->setMap({});
        tilePalette->clear();
        biomeList->clear();
        nodeList->clear();
        mapInfo->setText("No world maps in project");
    }
}

void WorldPanel::selectMap(const std::string& map_id) {
    current_map = map_id;
    mapCanvas->setMap(map_id);
    refreshMapContents();
}

void WorldPanel::refreshMapContents() {
    tilePalette->clear();
    biomeList->clear();
    nodeList->clear();
    mapCanvas->setSelectedNode(-1);
    editNodeButton->setEnabled(false);
    deleteNodeButton->setEnabled(false);
    if (!project || current_map.empty() || !project->maps.contains(current_map)) {
        mapInfo->clear();
        return;
    }

    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    QString info = QString("%1 × %2 · %3")
        .arg(map["w"].as_int()).arg(map["h"].as_int())
        .arg(QString::fromStdString(tileset_id));
    if (map.contains("worldgen") && map["worldgen"].contains("style") &&
        map["worldgen"]["style"].as_string() == "hd2d") {
        info += QString(" · HD · %1").arg(QString::fromStdString(
            map["worldgen"]["topology"].as_string()));
    }
    mapInfo->setText(info);

    const auto tileset = project->tilesets.find(tileset_id);
    if (tileset != project->tilesets.end()) {
        const auto& tiles = tileset->second["tiles"].as_array();
        tilePalette->setIconSize(QSize(40, 40));
        for (size_t index = 0; index < tiles.size(); ++index) {
            const Json& tile = tiles[index];
            const std::string name = tile.contains("name")
                ? tile["name"].as_string() : "Tile " + std::to_string(index);
            auto* item = new QListWidgetItem(
                QString("%1  %2").arg(index, 2, 10, QChar('0')).arg(QString::fromStdString(name)),
                tilePalette);
            const QImage preview = make_tileset_tile(*project, tileset_id, static_cast<int>(index));
            if (!preview.isNull()) {
                item->setIcon(QIcon(QPixmap::fromImage(preview).scaled(
                    40, 40, Qt::KeepAspectRatio, Qt::FastTransformation)));
            }
            item->setData(Qt::UserRole, static_cast<int>(index));
            item->setData(Qt::UserRole + 1,
                          QString::fromStdString(terrain_tile_category(name)));
        }
    }
    if (tilePalette->count() > 0) tilePalette->setCurrentRow(0);
    filterTerrainTiles();

    for (const auto& [biome_id, biome] : project->biomes) {
        if (!biome.contains("tileset") || biome["tileset"].as_string() != tileset_id) continue;
        const std::string name = biome.contains("name") ? biome["name"].as_string() : biome_id;
        auto* item = new QListWidgetItem(QString::fromStdString(name), biomeList);
        item->setData(Qt::UserRole, QString::fromStdString(biome_id));
        if (biome.contains("color")) {
            QColor color(QString::fromStdString(biome["color"].as_string()));
            if (color.isValid()) item->setForeground(color);
        }
        if (biome_id == selected_biome) biomeList->setCurrentItem(item);
    }
    if (biomeList->count() > 0 && biomeList->currentRow() < 0) biomeList->setCurrentRow(0);
    if (biomeList->currentItem()) {
        selected_biome = biomeList->currentItem()->data(Qt::UserRole).toString().toStdString();
        mapCanvas->setSelectedBiome(selected_biome);
    }

    if (map.contains("entities") && map["entities"].is_array()) {
        const auto& entities = map["entities"].as_array();
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& entity = entities[index];
            const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
            if (type != "location" && type != "exit") continue;
            std::string label = type == "exit" ? "Exit" : "Location";
            if (entity.contains("target") && entity["target"].contains("map")) {
                label += " → " + entity["target"]["map"].as_string();
                if (entity["target"].contains("spawn") && entity["target"]["spawn"].is_string() &&
                    !entity["target"]["spawn"].as_string().empty()) {
                    label += " / " + entity["target"]["spawn"].as_string();
                }
            } else if (entity.contains("id")) {
                label += " · " + entity["id"].as_string();
            }
            label += "  (" + std::to_string(entity["x"].as_int()) + ", " +
                     std::to_string(entity["y"].as_int()) + ")";
            auto* item = new QListWidgetItem(QString::fromStdString(label), nodeList);
            item->setData(Qt::UserRole, static_cast<int>(index));
        }
    }
}

void WorldPanel::filterTerrainTiles() {
    if (!tilePalette || !tileCategoryCombo) return;
    const QString category = tileCategoryCombo->currentData().toString();
    int first_visible = -1;
    for (int row = 0; row < tilePalette->count(); ++row) {
        QListWidgetItem* item = tilePalette->item(row);
        const bool visible = category == "all" ||
            item->data(Qt::UserRole + 1).toString() == category;
        item->setHidden(!visible);
        if (visible && first_visible < 0) first_visible = row;
    }
    const int selected_row = tilePalette->currentRow();
    if (selected_row < 0 || tilePalette->item(selected_row)->isHidden()) {
        tilePalette->setCurrentRow(first_visible);
    }
}

void WorldPanel::addStarterTerrainTiles() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const std::string tileset_id = project->maps.at(current_map)["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end() ||
        !tileset_it->second.contains("tiles") ||
        !tileset_it->second["tiles"].is_array()) {
        QMessageBox::warning(this, "Add starter terrain tiles",
                             "The selected world map has no editable tileset.");
        return;
    }

    JsonArray& tiles = tileset_it->second["tiles"].as_array();
    const Json* palettes = tileset_it->second.contains("palettes") &&
        tileset_it->second["palettes"].is_array()
        ? &tileset_it->second["palettes"] : nullptr;
    const int palette_count = palettes ? static_cast<int>(palettes->as_array().size()) : 0;
    int added = 0;
    for (const Json& starter : generate_starter_world_terrain_tiles(palette_count)) {
        const bool exists = std::any_of(tiles.begin(), tiles.end(), [&starter](const Json& tile) {
            return tile.is_object() && tile.contains("name") &&
                tile["name"].as_string() == starter["name"].as_string();
        });
        if (exists) continue;

        tiles.push_back(starter);
        ++added;
    }

    if (added == 0) {
        QMessageBox::information(this, "Add starter terrain tiles",
                                 "This tileset already has the complete starter terrain set.");
        return;
    }
    refreshMapContents();
}

void WorldPanel::createBiome() {
    if (!project || current_map.empty() || !project->maps.contains(current_map)) return;
    const Json& map = project->maps.at(current_map);
    const std::string tileset_id = map["tileset"].as_string();
    const auto tileset_it = project->tilesets.find(tileset_id);
    if (tileset_it == project->tilesets.end() ||
        !tileset_it->second.contains("tiles") || tileset_it->second["tiles"].as_array().empty()) {
        QMessageBox::warning(this, "Create biome", "The selected map has no usable tileset.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Create biome");
    QFormLayout form(&dialog);
    QLineEdit nameEdit(&dialog);
    nameEdit.setPlaceholderText("e.g. Moonlit Coast");
    form.addRow("Name:", &nameEdit);

    QComboBox climateCombo(&dialog);
    climateCombo.addItems({"temperate", "tropical", "arid", "arctic", "humid", "volcanic", "arcane"});
    form.addRow("Climate:", &climateCombo);

    QComboBox baseTileCombo(&dialog);
    const auto& tiles = tileset_it->second["tiles"].as_array();
    for (size_t index = 0; index < tiles.size(); ++index) {
        const std::string name = tiles[index].contains("name")
            ? tiles[index]["name"].as_string() : "Tile " + std::to_string(index);
        baseTileCombo.addItem(QString("%1 · %2").arg(index).arg(QString::fromStdString(name)),
                              static_cast<int>(index));
    }
    if (tilePalette->currentRow() >= 0) baseTileCombo.setCurrentIndex(tilePalette->currentRow());
    form.addRow("Base terrain:", &baseTileCombo);

    auto* colorEdit = new QLineEdit("#75b96b", &dialog);
    auto* colorButton = new QPushButton("Choose…", &dialog);
    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(colorEdit);
    colorRow->addWidget(colorButton);
    form.addRow("Map tint:", colorRow);
    connect(colorButton, &QPushButton::clicked, &dialog, [&dialog, colorEdit]() {
        const QColor selected = QColorDialog::getColor(QColor(colorEdit->text()), &dialog,
                                                       "Biome overlay color");
        if (selected.isValid()) colorEdit->setText(selected.name());
    });

    QLineEdit descriptionEdit(&dialog);
    descriptionEdit.setPlaceholderText("Optional notes about this region");
    form.addRow("Description:", &descriptionEdit);
    auto add_percent_field = [&form, &dialog](const QString& label, int value) {
        auto* field = new QSpinBox(&dialog);
        field->setRange(0, 100);
        field->setValue(value);
        field->setSuffix("%");
        form.addRow(label, field);
        return field;
    };
    QSpinBox* vegetation = add_percent_field("Vegetation:", 60);
    QSpinBox* decay = add_percent_field("Decay:", 10);
    QSpinBox* waterLevel = add_percent_field("Water level:", 50);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const std::string name = nameEdit.text().trimmed().toStdString();
    const QColor color(colorEdit->text());
    if (name.empty() || !color.isValid()) {
        QMessageBox::warning(this, "Create biome", "Enter a biome name and a valid color.");
        return;
    }

    std::string id = make_biome_id(name);
    if (id.empty()) {
        QMessageBox::warning(this, "Create biome", "The name must contain at least one letter or number.");
        return;
    }
    const std::string base_id = id;
    int suffix = 2;
    while (project->biomes.contains(id)) id = base_id + "_" + std::to_string(suffix++);

    Json biome;
    biome["name"] = name;
    biome["climate"] = climateCombo.currentText().toStdString();
    biome["tileset"] = tileset_id;
    biome["base_tile"] = baseTileCombo.currentData().toInt();
    biome["color"] = color.name().toStdString();
    biome["description"] = descriptionEdit.text().trimmed().toStdString();
    biome["vegetation"] = vegetation->value();
    biome["decay"] = decay->value();
    biome["water_level"] = waterLevel->value();
    project->biomes[id] = biome;
    selected_biome = id;
    refreshMapContents();
}

void WorldPanel::createWorldEntity(const QPoint& tile, int type) {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        (type != 3 && type != 4)) return;
    Json& map = project->maps[current_map];
    if (tile.x() < 0 || tile.y() < 0 || tile.x() >= map["w"].as_int() ||
        tile.y() >= map["h"].as_int()) return;
    const JsonArray no_entities;
    const JsonArray& existing_entities = map.contains("entities") && map["entities"].is_array()
        ? map["entities"].as_array() : no_entities;
    int added_index = -1;

    if (type == 3) {
        bool accepted = false;
        const QString entered_id = QInputDialog::getText(
            this, "Add world location", "Location ID:", QLineEdit::Normal,
            "new_location", &accepted).trimmed();
        if (!accepted) return;
        const std::string id = make_biome_id(entered_id.toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Add world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (const Json& entity : existing_entities) {
            if (entity.contains("id") && entity["id"].is_string() &&
                entity["id"].as_string() == id) {
                QMessageBox::warning(this, "Add world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(Json(JsonObject{
            {"id", id}, {"type", "location"}, {"x", tile.x()}, {"y", tile.y()}
        }));
        added_index = static_cast<int>(entities.size()) - 1;
    } else {
        QDialog dialog(this);
        dialog.setWindowTitle("Add world exit");
        QFormLayout form(&dialog);
        QComboBox targetMap(&dialog);
        for (const auto& [map_id, target] : project->maps) {
            const std::string name = target.contains("name") && target["name"].is_string()
                ? target["name"].as_string() : map_id;
            targetMap.addItem(QString::fromStdString(name), QString::fromStdString(map_id));
        }
        if (targetMap.count() == 0) {
            QMessageBox::warning(this, "Add world exit", "The project has no target maps.");
            return;
        }
        form.addRow("Destination map:", &targetMap);
        QLineEdit targetSpawn(&dialog);
        targetSpawn.setPlaceholderText("Optional spawn ID");
        form.addRow("Destination spawn:", &targetSpawn);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;

        JsonObject target{{"map", targetMap.currentData().toString().toStdString()}};
        const std::string spawn = targetSpawn.text().trimmed().toStdString();
        if (!spawn.empty()) target["spawn"] = spawn;
        mapCanvas->recordEntityEdit();
        JsonArray& entities = map["entities"].as_array();
        entities.emplace_back(Json(JsonObject{
            {"effect", "fade"}, {"h", 1}, {"target", Json(target)},
            {"type", "exit"}, {"w", 1}, {"x", tile.x()}, {"y", tile.y()}
        }));
        added_index = static_cast<int>(entities.size()) - 1;
    }

    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == added_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::editWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    Json& entity = entities[static_cast<size_t>(entity_index)];
    const std::string type = entity.contains("type") ? entity["type"].as_string() : "location";
    if (type != "location" && type != "exit") return;

    QDialog dialog(this);
    dialog.setWindowTitle(type == "exit" ? "Edit world exit" : "Edit world location");
    QFormLayout form(&dialog);
    QLineEdit idEdit(&dialog);
    if (entity.contains("id") && entity["id"].is_string()) {
        idEdit.setText(QString::fromStdString(entity["id"].as_string()));
    }
    QSpinBox x(&dialog);
    QSpinBox y(&dialog);
    x.setRange(0, map["w"].as_int() - 1);
    y.setRange(0, map["h"].as_int() - 1);
    x.setValue(entity["x"].as_int());
    y.setValue(entity["y"].as_int());
    if (type == "location") {
        form.addRow("Location ID:", &idEdit);
    }
    form.addRow("X (tiles):", &x);
    form.addRow("Y (tiles):", &y);

    QComboBox targetMap(&dialog);
    QLineEdit targetSpawn(&dialog);
    if (type == "exit") {
        for (const auto& [map_id, target] : project->maps) {
            const std::string name = target.contains("name") && target["name"].is_string()
                ? target["name"].as_string() : map_id;
            targetMap.addItem(QString::fromStdString(name), QString::fromStdString(map_id));
        }
        if (entity.contains("target") && entity["target"].contains("map")) {
            const int target_index = targetMap.findData(
                QString::fromStdString(entity["target"]["map"].as_string()));
            if (target_index >= 0) targetMap.setCurrentIndex(target_index);
        }
        if (entity.contains("target") && entity["target"].contains("spawn") &&
            entity["target"]["spawn"].is_string()) {
            targetSpawn.setText(QString::fromStdString(entity["target"]["spawn"].as_string()));
        }
        targetSpawn.setPlaceholderText("Optional spawn ID");
        form.addRow("Destination map:", &targetMap);
        form.addRow("Destination spawn:", &targetSpawn);
    }
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    std::string id;
    if (type == "location") {
        id = make_biome_id(idEdit.text().trimmed().toStdString());
        if (id.empty()) {
            QMessageBox::warning(this, "Edit world location",
                                 "Enter a location ID containing at least one letter or number.");
            return;
        }
        for (size_t index = 0; index < entities.size(); ++index) {
            const Json& other = entities[index];
            if (static_cast<int>(index) != entity_index && other.contains("id") &&
                other["id"].is_string() && other["id"].as_string() == id) {
                QMessageBox::warning(this, "Edit world location",
                                     "That location ID is already in use on this map.");
                return;
            }
        }
    } else if (targetMap.count() == 0) {
        QMessageBox::warning(this, "Edit world exit", "The project has no target maps.");
        return;
    }

    mapCanvas->recordEntityEdit();
    if (type == "location") {
        entity["id"] = id;
    } else {
        entity["target"]["map"] = targetMap.currentData().toString().toStdString();
        entity["target"]["spawn"] = targetSpawn.text().trimmed().toStdString();
    }
    entity["x"] = x.value();
    entity["y"] = y.value();
    refreshMapContents();
    for (int row = 0; row < nodeList->count(); ++row) {
        if (nodeList->item(row)->data(Qt::UserRole).toInt() == entity_index) {
            nodeList->setCurrentRow(row);
            break;
        }
    }
}

void WorldPanel::deleteWorldEntity() {
    if (!project || current_map.empty() || !project->maps.contains(current_map) ||
        !nodeList->currentItem()) return;
    Json& map = project->maps[current_map];
    if (!map.contains("entities") || !map["entities"].is_array()) return;
    JsonArray& entities = map["entities"].as_array();
    const int entity_index = nodeList->currentItem()->data(Qt::UserRole).toInt();
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    const auto answer = QMessageBox::question(
        this, "Delete world marker", "Delete the selected location or exit?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    mapCanvas->recordEntityEdit();
    entities.erase(entities.begin() + entity_index);
    refreshMapContents();
}

} // namespace aur
