#pragma once
#include "world/WorldModel.hpp"
#include "aur/project.hpp"

#include <QtWidgets/qrhiwidget.h>
#include <rhi/qrhi.h>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aur {

struct TerrainView {
    editor::WorldModel* world_model = nullptr;
    std::string map_id;
    std::uint64_t generation = 0;
    const Project* project = nullptr;
    QRectF bounds;
    QSize viewport_size;
    qreal zoom = 1.0;
    double hour = 8.0;
    bool daynight = true;
    bool contours = true;
    bool globe = false;
    double globe_yaw = 0.0;
    double globe_pitch = 0.0;
};

class TerrainRhiWidget final : public QRhiWidget {
public:
    explicit TerrainRhiWidget(std::function<TerrainView()> provider, QWidget* parent = nullptr);

protected:
    void initialize(QRhiCommandBuffer*) override;
    void render(QRhiCommandBuffer* commandBuffer) override;
    void releaseResources() override;

private:
    struct alignas(16) WorldModelUniforms {
        float mapRect[4];
        float world[4];
        float viewportLight[4];
        float effects[4];
        float environment[4];
        float ambient[4];
    };

    struct alignas(16) GlobeUniforms {
        float globe[4];
        float viewport[4];
    };

    std::function<TerrainView()> view_provider_;
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
    std::unique_ptr<QRhiTexture> globeTexture_;
    std::unique_ptr<QRhiBuffer> globeUniform_;
    std::unique_ptr<QRhiShaderResourceBindings> globeBindings_;
    std::unique_ptr<QRhiGraphicsPipeline> globePipeline_;
    std::string globeMap_;
    std::uint64_t globeGeneration_ = 0;
    QImage globePendingImage_;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    std::string textureMap_;
    std::uint64_t textureGeneration_ = 0;
    std::uint64_t uploadedRevision_ = 0;
    bool resourcesReady_ = false;
    bool biomeLutPendingUpload_ = false;

    TerrainView view() const;
    static QShader loadShader(const QString& path);
    void ensureResources();
    void createBiomeLut();
    void createBindingsAndPipeline();
    void createGlobePipeline();
    void uploadGlobeSurface(QRhiResourceUpdateBatch* updates, const TerrainView& view);
    void renderGlobe(QRhiCommandBuffer* commandBuffer);
    void uploadDirtyRects(QRhiResourceUpdateBatch* updates);
    WorldModelUniforms makeUniforms() const;

    template<typename T>
    static void uploadRect(QRhiResourceUpdateBatch* updates, QRhiTexture* texture,
                           const std::vector<T>& values, int worldWidth,
                           const editor::WorldModel::Rect& rect);
};

}
