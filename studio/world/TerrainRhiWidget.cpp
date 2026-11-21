#include "TerrainRhiWidget.hpp"
#include "core/RhiBackend.hpp"
#include "world/HdSurface.hpp"
#include "aur/clock.hpp"
#include "aur/game.hpp"
#include "aur/world_topology.hpp"
#include <QColor>
#include <QFile>
#include <rhi/qshader.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace aur {

TerrainRhiWidget::TerrainRhiWidget(std::function<TerrainView()> provider, QWidget* parent)
    : QRhiWidget(parent), view_provider_(std::move(provider)) {
    setApi(editor::configuredRhiWidgetApi());
    setSampleCount(1);
    setColorBufferFormat(TextureFormat::RGBA8);
    connect(this, &QRhiWidget::renderFailed, this, []() {
        qFatal("Aur could not initialize the configured QRhi rendering backend");
    });
}

TerrainView TerrainRhiWidget::view() const {
    return view_provider_ ? view_provider_() : TerrainView{};
}

void TerrainRhiWidget::initialize(QRhiCommandBuffer*) {
    editor::assertConfiguredRhiBackend(rhi());
    if (editor::isHeadlessRhi()) return;
    const QRhiDriverInfo driver = rhi()->driverInfo();
    qInfo().noquote() << "Aur world sculpt backend:" << rhi()->backendName()
                      << "device:" << driver.deviceName;
    ensureResources(view());
}

void TerrainRhiWidget::render(QRhiCommandBuffer* commandBuffer) {
    editor::assertConfiguredRhiBackend(rhi());
    if (editor::isHeadlessRhi()) return;
    const TerrainView current = view();
    if (current.globe) {
        renderGlobe(commandBuffer, current);
        return;
    }
    if (!current.world_model || !current.world_model->width() ||
        !current.world_model->height()) {
        commandBuffer->beginPass(renderTarget(), QColor("#171b20"),
                                 QRhiDepthStencilClearValue{});
        commandBuffer->endPass();
        return;
    }
    ensureResources(current);
    if (!resourcesReady_) return;

    QRhiResourceUpdateBatch* updates = rhi()->nextResourceUpdateBatch();
    uploadDirtyRects(updates, current);
    const WorldModelUniforms uniforms = makeUniforms(current);
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

void TerrainRhiWidget::releaseResources() {
    resourcesReady_ = false;
    pipeline_.reset();
    globePipeline_.reset();
    globeBindings_.reset();
    globeUniform_.reset();
    globeTexture_.reset();
    globeDetailTexture_.reset();
    globeDetailSampler_.reset();
    globeMap_.clear();
    globeGeneration_ = 0;
    globeRevision_ = 0;
    globeBaseDensity_ = -1;
    globePendingImage_ = QImage();
    globeDetailKey_ = {};
    globeDetailPendingImage_ = QImage();
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

QShader TerrainRhiWidget::loadShader(const QString& path) {
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

void TerrainRhiWidget::ensureResources(const TerrainView& current) {
    if (!rhi() || !renderTarget()) return;
    if (!current.world_model) return;
    const int width = current.world_model->width();
    const int height = current.world_model->height();
    if (width <= 0 || height <= 0) return;
    if (textureWidth_ == width && textureHeight_ == height &&
        textureMap_ == current.map_id &&
        textureGeneration_ == current.generation && resourcesReady_) return;

    releaseResources();
    if (!rhi()->isTextureFormatSupported(QRhiTexture::R16) ||
        !rhi()->isTextureFormatSupported(QRhiTexture::R8)) {
        qFatal("The selected QRhi backend does not support R16 and R8 world data textures");
    }
    heightTexture_.reset(rhi()->newTexture(QRhiTexture::R16, QSize(width, height)));
    biomeTexture_.reset(rhi()->newTexture(QRhiTexture::R8, QSize(width, height)));
    waterTexture_.reset(rhi()->newTexture(QRhiTexture::R8, QSize(width, height)));
    biomeLutTexture_.reset(rhi()->newTexture(QRhiTexture::RGBA8, QSize(32, 8)));
    const QRhiSampler::AddressMode addressU = current.world_model->wrapsX()
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
    createBiomeLut(current);
    createBindingsAndPipeline();
    textureWidth_ = width;
    textureHeight_ = height;
    resourcesReady_ = true;
    uploadedRevision_ = 0;
    textureMap_ = current.map_id;
    textureGeneration_ = current.generation;
    update();
}

void TerrainRhiWidget::createBiomeLut(const TerrainView& current) {
    biome_lut_ = QImage(32, 8, QImage::Format_RGBA8888);
    biome_lut_.fill(QColor(116, 134, 106, 0));
    for (std::size_t id = 0; id < current.world_model->biomeNames().size() && id < 32; ++id) {
        const std::string& biomeId = current.world_model->biomeNames()[id];
        const bool forestBiome = biomeId.find("forest") != std::string::npos ||
            biomeId.find("jungle") != std::string::npos;
        QColor base("#74866a");
        if (current.project) {
            const auto biome = current.project->biomes.find(biomeId);
            if (biome != current.project->biomes.end() &&
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

void TerrainRhiWidget::createGlobePipeline() {
    if (!globeUniform_) {
        globeUniform_.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                             sizeof(GlobeUniforms)));
        if (!globeUniform_->create()) qFatal("Failed to create the globe uniform buffer");
    }
    if (!globeDetailSampler_) {
        globeDetailSampler_.reset(rhi()->newSampler(
            QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if (!globeDetailSampler_->create()) qFatal("Failed to create the globe detail sampler");
    }
    QRhiTexture* detailTex = globeDetailTexture_ ? globeDetailTexture_.get() : globeTexture_.get();
    globeBindings_.reset(rhi()->newShaderResourceBindings());
    globeBindings_->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage, globeUniform_.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            globeTexture_.get(), linearSampler_.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage,
            detailTex, globeDetailSampler_.get())
    });
    if (!globeBindings_->create()) qFatal("Failed to bind the globe resources");

    globePipeline_.reset(rhi()->newGraphicsPipeline());
    globePipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
    globePipeline_->setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(":/aur/shaders/world_sculpt.vert.qsb")},
        {QRhiShaderStage::Fragment, loadShader(":/aur/shaders/globe.frag.qsb")}
    });
    globePipeline_->setShaderResourceBindings(globeBindings_.get());
    globePipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    if (!globePipeline_->create()) qFatal("Failed to create the globe pipeline");
}

void TerrainRhiWidget::renderGlobe(QRhiCommandBuffer* commandBuffer, const TerrainView& current) {
    if (!current.project || !current.project->maps.contains(current.map_id)) {
        commandBuffer->beginPass(renderTarget(), QColor("#171b20"), QRhiDepthStencilClearValue{});
        commandBuffer->endPass();
        return;
    }
    ensureResources(current);
    if (!linearSampler_) return;

    const Json& map = current.project->maps.at(current.map_id);
    const int map_width = map["w"].as_int();
    const int map_height = map["h"].as_int();
    if (map_width <= 0 || map_height <= 0) return;

    const std::uint64_t revision = current.world_model ? current.world_model->revision() : 0;
    const world::LodTier tier = world::selectLodTier(map, current.bounds.width());
    const int base_density = (tier == world::LodTier::Coarse) ? 1 : 0;

    if (!globeTexture_ || globeMap_ != current.map_id ||
        globeGeneration_ != current.generation || globeRevision_ != revision ||
        globeBaseDensity_ != base_density) {
        const QImage surface = world::build_hd_surface(map, current.project, QRect(), base_density);
        if (surface.isNull()) return;
        if (!globeTexture_ || globeTexture_->pixelSize() != surface.size()) {
            globeTexture_.reset(rhi()->newTexture(QRhiTexture::BGRA8, surface.size()));
            if (!globeTexture_->create()) qFatal("Failed to create the globe texture");
            globePipeline_.reset();
            globeBindings_.reset();
        }
        globePendingImage_ = surface;
        globeMap_ = current.map_id;
        globeGeneration_ = current.generation;
        globeRevision_ = revision;
        globeBaseDensity_ = base_density;
    }

    if (tier == world::LodTier::CloseRegion) {
        const double ppt = world::pixelsPerTile(current.bounds.width(), map_width);
        const int detail_density = world::closeRegionDensity(ppt);
        const QRect vis_tiles = world::globeCloseRegionTileBounds(
            current.globe_yaw, current.globe_pitch, ppt, current.viewport_size,
            map_width, map_height, 0);
        if (!globeDetailTexture_ ||
            !globeDetailKey_.contains(current.map_id, revision, vis_tiles, detail_density)) {
            const QRect pad_tiles = world::globeCloseRegionTileBounds(
                current.globe_yaw, current.globe_pitch, ppt, current.viewport_size,
                map_width, map_height, 4);
            const QImage detail = world::build_hd_surface(map, current.project, pad_tiles, detail_density);
            if (!detail.isNull()) {
                if (!globeDetailTexture_ || globeDetailTexture_->pixelSize() != detail.size()) {
                    globeDetailTexture_.reset(rhi()->newTexture(QRhiTexture::BGRA8, detail.size()));
                    if (!globeDetailTexture_->create()) qFatal("Failed to create the globe detail texture");
                    globePipeline_.reset();
                    globeBindings_.reset();
                }
                globeDetailPendingImage_ = detail;
                globeDetailKey_ = {current.map_id, revision, pad_tiles, detail_density};
            }
        }
    }

    if (!globePipeline_ || !globeBindings_) createGlobePipeline();

    QRhiResourceUpdateBatch* updates = rhi()->nextResourceUpdateBatch();
    if (!globePendingImage_.isNull()) {
        updates->uploadTexture(globeTexture_.get(), globePendingImage_);
        globePendingImage_ = QImage();
    }
    if (!globeDetailPendingImage_.isNull() && globeDetailTexture_) {
        updates->uploadTexture(globeDetailTexture_.get(), globeDetailPendingImage_);
        globeDetailPendingImage_ = QImage();
    }
    const float globe_zoom = static_cast<float>(std::max<qreal>(0.05, current.zoom_multiplier));
    const float blend_t = std::clamp((globe_zoom - 3.0f) / 3.0f, 0.0f, 1.0f);
    const float curvature = 1.0f - blend_t * blend_t * (3.0f - 2.0f * blend_t);
    const QSize pixel_size = renderTarget()->pixelSize();
    const bool has_detail = (tier == world::LodTier::CloseRegion) && globeDetailTexture_ &&
        !globeDetailKey_.tile_bounds.isEmpty();

    GlobeUniforms uniforms{};
    uniforms.globe[0] = static_cast<float>(current.globe_yaw);
    uniforms.globe[1] = static_cast<float>(current.globe_pitch);
    uniforms.globe[2] = globe_zoom;
    uniforms.globe[3] = curvature;
    uniforms.viewport[0] = static_cast<float>(pixel_size.width());
    uniforms.viewport[1] = static_cast<float>(pixel_size.height());
    uniforms.viewport[2] = rhi()->isYUpInNDC() ? 1.0f : 0.0f;
    uniforms.viewport[3] = has_detail ? 1.0f : 0.0f;
    if (has_detail) {
        const int wrapped_left = aur::wrapX(globeDetailKey_.tile_bounds.left(), map_width);
        uniforms.detailRect[0] = static_cast<float>(wrapped_left) / static_cast<float>(map_width);
        uniforms.detailRect[1] =
            static_cast<float>(globeDetailKey_.tile_bounds.top()) / static_cast<float>(map_height);
        uniforms.detailRect[2] =
            static_cast<float>(globeDetailKey_.tile_bounds.width()) / static_cast<float>(map_width);
        uniforms.detailRect[3] =
            static_cast<float>(globeDetailKey_.tile_bounds.height()) / static_cast<float>(map_height);
    }
    updates->updateDynamicBuffer(globeUniform_.get(), 0, sizeof(uniforms), &uniforms);

    commandBuffer->beginPass(renderTarget(), QColor("#171b20"),
                             QRhiDepthStencilClearValue{}, updates);
    commandBuffer->setGraphicsPipeline(globePipeline_.get());
    commandBuffer->setViewport(QRhiViewport(
        0, 0, static_cast<float>(pixel_size.width()), static_cast<float>(pixel_size.height())));
    commandBuffer->setShaderResources(globeBindings_.get());
    commandBuffer->draw(3);
    commandBuffer->endPass();
}

void TerrainRhiWidget::createBindingsAndPipeline() {
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
void TerrainRhiWidget::uploadRect(QRhiResourceUpdateBatch* updates, QRhiTexture* texture,
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

void TerrainRhiWidget::uploadDirtyRects(QRhiResourceUpdateBatch* updates, const TerrainView& current) {
    editor::WorldModel& model = *current.world_model;
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
        } else if (dirty.layer == editor::WorldModel::Layer::Biome) {
            uploadRect(updates, biomeTexture_.get(), model.biomes(), model.width(), dirty.rect);
        } else if (dirty.layer == editor::WorldModel::Layer::Water) {
            uploadRect(updates, waterTexture_.get(), model.waterTypes(), model.width(), dirty.rect);
        }
    }
    if (biomeLutPendingUpload_) {
        updates->uploadTexture(biomeLutTexture_.get(), biome_lut_);
        biomeLutPendingUpload_ = false;
    }
    uploadedRevision_ = model.revision() + 1;
}

TerrainRhiWidget::WorldModelUniforms TerrainRhiWidget::makeUniforms(const TerrainView& current) const {
    const QRectF bounds = current.bounds;
    const editor::WorldModel& model = *current.world_model;
    WorldModelUniforms uniforms{};
    uniforms.mapRect[0] = static_cast<float>(bounds.left());
    uniforms.mapRect[1] = static_cast<float>(bounds.top());
    uniforms.mapRect[2] = static_cast<float>(std::max<qreal>(1.0, bounds.width()));
    uniforms.mapRect[3] = static_cast<float>(std::max<qreal>(1.0, bounds.height()));
    uniforms.world[0] = static_cast<float>(model.width());
    uniforms.world[1] = static_cast<float>(model.height());
    uniforms.world[2] = static_cast<float>(model.seaLevel()) / 65535.0f;
    uniforms.world[3] = 0.05f;
    uniforms.viewportLight[0] = static_cast<float>(current.viewport_size.width());
    uniforms.viewportLight[1] = static_cast<float>(current.viewport_size.height());
    uniforms.viewportLight[2] = 1.0f;
    uniforms.viewportLight[3] = static_cast<float>(current.zoom);
    uniforms.effects[0] = current.contours ? 1.0f : 0.0f;
    uniforms.effects[1] = 1.0f;
    const qreal nearestIntegerZoom = std::round(current.zoom);
    uniforms.effects[2] = current.zoom >= 1.0 &&
        std::abs(current.zoom - nearestIntegerZoom) < 0.001 ? 1.0f : 0.0f;
    uniforms.effects[3] = model.wrapsX() ? 1.0f : 0.0f;
    GameClock lighting = project_clock(current.project);
    lighting.set_time(current.hour);
    const auto [rgb, darkness] = lighting.ambient();
    uniforms.environment[0] = static_cast<float>(current.hour);
    uniforms.environment[1] = current.daynight ? 1.0f : 0.0f;
    uniforms.environment[2] = rhi()->isYUpInNDC() ? 1.0f : 0.0f;
    uniforms.ambient[0] = std::get<0>(rgb) / 255.0f;
    uniforms.ambient[1] = std::get<1>(rgb) / 255.0f;
    uniforms.ambient[2] = std::get<2>(rgb) / 255.0f;
    uniforms.ambient[3] = current.daynight
        ? static_cast<float>(darkness) : 0.0f;
    return uniforms;
}

}
