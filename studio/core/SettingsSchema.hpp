#pragma once

#include <QString>
#include <QVariantMap>

namespace aur
{

    // Carried over from Hydra/src/SettingsSchema.h: scope resolution for settings
    // (map > project > global; only absence inherits).
    struct ScopedValues
    {
        QVariantMap global;
        QVariantMap project;
        QVariantMap map;
    };

    inline QVariant resolveSetting(const QString &key, const ScopedValues &values,
                                   const QVariant &fallback = QVariant())
    {
        if (values.map.contains(key))
            return values.map.value(key);
        if (values.project.contains(key))
            return values.project.value(key);
        if (values.global.contains(key))
            return values.global.value(key);
        return fallback;
    }

    inline bool isOverridden(const QString &key, const ScopedValues &values, bool mapScope)
    {
        return mapScope ? values.map.contains(key) : values.project.contains(key);
    }

}
