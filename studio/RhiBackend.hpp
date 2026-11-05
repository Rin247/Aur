#pragma once

#include <QtWidgets/qrhiwidget.h>

class QRhi;

namespace aur::editor {

enum class RhiBackend {
    Vulkan,
    D3D12,
    HeadlessNull
};

RhiBackend configureRhiBackendBeforeApplication();
QRhiWidget::Api configuredRhiWidgetApi();
void assertConfiguredRhiBackend(QRhi* rhi);
bool isHeadlessRhi() noexcept;

} // namespace aur::editor
