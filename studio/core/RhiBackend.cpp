#include "core/RhiBackend.hpp"

#include <QByteArray>
#include <QtCore/qglobal.h>
#include <rhi/qrhi.h>

namespace aur::editor {
namespace {

RhiBackend configuredBackend = RhiBackend::D3D12;
bool backendConfigured = false;

} // namespace

RhiBackend configureRhiBackendBeforeApplication() {
    if (backendConfigured) return configuredBackend;

    if (qgetenv("RHI_POLICY_HEADLESS") == QByteArrayLiteral("1")) {
        qputenv("QT_RHI_BACKEND", QByteArrayLiteral("null"));
        configuredBackend = RhiBackend::HeadlessNull;
        backendConfigured = true;
        return configuredBackend;
    }

    QByteArray requested = qgetenv("AUR_RHI_BACKEND").toLower();
    if (requested.isEmpty()) requested = qgetenv("QT_RHI_BACKEND").toLower();
    if (requested.isEmpty() || requested == QByteArrayLiteral("auto")) {
        requested = QByteArrayLiteral("d3d12");
    }

    if (requested == QByteArrayLiteral("vulkan")) {
        configuredBackend = RhiBackend::Vulkan;
    } else if (requested == QByteArrayLiteral("d3d12")) {
        configuredBackend = RhiBackend::D3D12;
    } else {
        qFatal("Aur graphics policy allows only Vulkan or D3D12; "
               "set AUR_RHI_BACKEND to 'vulkan' or 'd3d12'.");
    }

    qputenv("QT_RHI_BACKEND", requested);
    backendConfigured = true;
    return configuredBackend;
}

QRhiWidget::Api configuredRhiWidgetApi() {
    if (!backendConfigured) {
        qFatal("RHI backend must be configured before QApplication construction");
    }
    switch (configuredBackend) {
    case RhiBackend::Vulkan:
        return QRhiWidget::Api::Vulkan;
    case RhiBackend::D3D12:
        return QRhiWidget::Api::Direct3D12;
    case RhiBackend::HeadlessNull:
        return QRhiWidget::Api::Null;
    }
    qFatal("Unknown configured QRhi backend");
    return QRhiWidget::Api::Null;
}

void assertConfiguredRhiBackend(QRhi* rhi) {
    if (!rhi) qFatal("Aur QRhi widget did not create an RHI instance");
    const QRhi::Implementation actual = rhi->backend();
    if (configuredBackend == RhiBackend::Vulkan && actual != QRhi::Vulkan) {
        qFatal("Aur requested Vulkan but QRhi created a different backend");
    }
    if (configuredBackend == RhiBackend::D3D12 && actual != QRhi::D3D12) {
        qFatal("Aur requested D3D12 but QRhi created a different backend");
    }
    if (configuredBackend == RhiBackend::HeadlessNull && actual != QRhi::Null) {
        qFatal("Aur headless mode requested Null QRhi but created a different backend");
    }
}

bool isHeadlessRhi() noexcept {
    return backendConfigured && configuredBackend == RhiBackend::HeadlessNull;
}

} // namespace aur::editor
