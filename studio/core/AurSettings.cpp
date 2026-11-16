#include "core/AurSettings.hpp"

#include "core/AurPaths.hpp"
#include "theme/ThemeContract.hpp"

#include <QMetaType>
#include <QRegularExpression>
#include <QSettings>
#include <QtGlobal>
#include <cmath>

namespace aur
{
    namespace
    {
        constexpr double kZoomMin = 1.0;
        constexpr double kZoomMax = 2.0;
        constexpr int kUiScaleMin = 75;
        constexpr int kUiScaleMax = 200;

        QVariant normalize_theme_stored(const QVariant &raw)
        {
            if (!raw.isValid()) return raw;
            const QString s = raw.toString().trimmed();
            if (s.isEmpty()) return raw;
            bool isInt = false;
            const int ordinal = s.toInt(&isInt);
            if (isInt)
            {
                const char *mapped = theme::legacyModeIdFor(ordinal);
                if (mapped) return QString::fromLatin1(mapped);
            }
            const std::string utf = s.toUtf8().toStdString();
            const std::string_view norm = theme::normalize_mode_alias(utf);
            return QString::fromUtf8(norm.data(), static_cast<int>(norm.size()));
        }
    }

    AurSettings &AurSettings::instance()
    {
        static AurSettings store;
        return store;
    }

    AurSettings::AurSettings(QObject *parent) : QObject(parent)
    {
        buildDefinitions();
        for (auto it = defs_.constBegin(); it != defs_.constEnd(); ++it)
            values_.insert(it.key(), it.value().defaultValue);
    }

    QStringList AurSettings::themeModes() const
    {
        QStringList modes;
        for (const char *mode : theme::kThemeModes)
            modes << QString::fromLatin1(mode);
        return modes;
    }

    void AurSettings::buildDefinitions()
    {
        const auto add = [this](const char *key, const QVariant &defaultValue,
                                const QVariant &minimum, const QVariant &maximum,
                                const QStringList &allowed, const QString &pattern) {
            Definition def;
            def.defaultValue = defaultValue;
            def.minimum = minimum;
            def.maximum = maximum;
            def.allowed = allowed;
            def.pattern = pattern;
            defs_.insert(QString::fromLatin1(key), def);
        };
        const auto flag = [&add](const char *key, bool value) {
            add(key, QVariant(value), QVariant(), QVariant(), QStringList(), QString());
        };
        const auto num = [&add](const char *key, int value, int lo, int hi) {
            add(key, QVariant(value), QVariant(lo), QVariant(hi), QStringList(), QString());
        };
        const auto real = [&add](const char *key, double value, double lo, double hi) {
            add(key, QVariant(value), QVariant(lo), QVariant(hi), QStringList(), QString());
        };
        const auto choice = [&add](const char *key, const char *value, const QStringList &options) {
            add(key, QVariant(QString::fromLatin1(value)), QVariant(), QVariant(), options, QString());
        };

        choice("appearance/themeMode", "dark", themeModes());
        real("appearance/zoom", 1.0, kZoomMin, kZoomMax);
        add("appearance/accentColor", QVariant(QStringLiteral("#3b82f6")),
            QVariant(), QVariant(), QStringList(), QStringLiteral("^#[0-9a-fA-F]{6}$"));
        choice("appearance/colorBlindMode", "none",
               {QStringLiteral("none"), QStringLiteral("protanopia"),
                QStringLiteral("deuteranopia"), QStringLiteral("tritanopia"),
                QStringLiteral("achromatopsia")});
        num("appearance/uiScale", 100, kUiScaleMin, kUiScaleMax);
        flag("appearance/dyslexiaFont", false);

        choice("graphics/backend", "d3d12",
               {QStringLiteral("d3d12"), QStringLiteral("vulkan")});

        flag("audio/enabled", true);
        num("audio/masterVolume", 80, 0, 100);

        num("window/tab", -1, -1, 8);
    }

    QVariant AurSettings::validate(const QString &key, const QVariant &stored, bool *rejected) const
    {
        const auto it = defs_.constFind(key);
        if (it == defs_.constEnd()) return stored;
        const Definition &def = it.value();

        const QVariant candidateValue =
            (key == QLatin1String("appearance/themeMode")) ? normalize_theme_stored(stored) : stored;

        if (!def.allowed.isEmpty())
        {
            if (candidateValue.typeId() == QMetaType::QString)
            {
                const QString text = candidateValue.toString().toLower();
                if (def.allowed.contains(text)) return text;
            }
            else
            {
                for (const QVariant &candidate : def.allowed)
                    if (candidate == candidateValue) return candidateValue;
            }
            if (rejected) *rejected = true;
            return def.defaultValue;
        }

        if (!def.pattern.isEmpty())
        {
            const QString text = candidateValue.toString();
            if (QRegularExpression(def.pattern).match(text).hasMatch())
                return text.toLower();
            if (rejected) *rejected = true;
            return def.defaultValue;
        }

        switch (def.defaultValue.typeId())
        {
        case QMetaType::Bool:
            if (candidateValue.typeId() == QMetaType::Bool) return candidateValue;
            if (candidateValue.typeId() == QMetaType::QString)
            {
                const QString text = candidateValue.toString().trimmed().toLower();
                if (text == QLatin1String("true") || text == QLatin1String("1")) return true;
                if (text == QLatin1String("false") || text == QLatin1String("0")) return false;
            }
            if (rejected) *rejected = true;
            return def.defaultValue;

        case QMetaType::QString:
            if (candidateValue.typeId() == QMetaType::QString)
                return candidateValue.toString();
            if (rejected) *rejected = true;
            return def.defaultValue;

        default:
            break;
        }

        const bool staysReal = (def.defaultValue.typeId() == QMetaType::Double);
        bool ok = false;
        const double raw = candidateValue.toDouble(&ok);
        if (!ok || !std::isfinite(raw))
        {
            if (rejected) *rejected = true;
            return def.defaultValue;
        }
        const double lo = def.minimum.isValid() ? def.minimum.toDouble() : -1e18;
        const double hi = def.maximum.isValid() ? def.maximum.toDouble() : 1e18;
        const double clamped = qBound(lo, raw, hi);
        if (clamped != raw && rejected) *rejected = true;
        if (staysReal) return clamped;
        return static_cast<int>(std::lround(clamped));
    }

    QVariant AurSettings::value(const QString &key) const
    {
        if (values_.contains(key)) return values_.value(key);
        const auto it = defs_.constFind(key);
        return it != defs_.constEnd() ? it.value().defaultValue : QVariant();
    }

    void AurSettings::setValue(const QString &key, const QVariant &val)
    {
        bool rejected = false;
        const QVariant accepted = validate(key, val, &rejected);
        if (!rejected) rejected_keys_.remove(key);
        if (values_.value(key) == accepted) return;
        values_.insert(key, accepted);
        if (!settings_read_) return;
        save();
        emit settingChanged(key);
        if (key.startsWith(QLatin1String("appearance/")))
            emit themeChanged();
    }

    void AurSettings::resetToDefaults()
    {
        for (auto it = defs_.constBegin(); it != defs_.constEnd(); ++it)
            values_.insert(it.key(), it.value().defaultValue);
        rejected_keys_.clear();
        if (settings_read_)
        {
            save();
            emit settingChanged(QString());
            emit themeChanged();
        }
    }

    void AurSettings::load()
    {
        if (settings_read_) return;
        for (auto it = defs_.constBegin(); it != defs_.constEnd(); ++it)
            values_.insert(it.key(), it.value().defaultValue);
        rejected_keys_.clear();

        QSettings store(aur::settingsPath(), QSettings::IniFormat);

        struct LegacyMap
        {
            const char *legacyKey;
            const char *canonicalKey;
        };
        constexpr LegacyMap kLegacyKeys[] = {
            {"theme/mode", "appearance/themeMode"},
            {"theme/accent", "appearance/accentColor"},
            {"theme/uiScale", "appearance/uiScale"},
            {"theme/dyslexiaFont", "appearance/dyslexiaFont"},
            {"theme/colorBlindMode", "appearance/colorBlindMode"},
            {"audio/master_volume", "audio/masterVolume"},
        };

        for (auto it = defs_.constBegin(); it != defs_.constEnd(); ++it)
        {
            const QString &key = it.key();
            QVariant raw = store.value(key);
            if (!raw.isValid())
            {
                for (const LegacyMap &lm : kLegacyKeys)
                {
                    if (key == QLatin1String(lm.canonicalKey))
                    {
                        raw = store.value(QString::fromLatin1(lm.legacyKey));
                        break;
                    }
                }
            }
            if (!raw.isValid()) continue;
            bool rejected = false;
            const QVariant accepted = validate(key, raw, &rejected);
            if (rejected)
            {
                rejected_keys_.insert(key);
                qWarning("AurSettings: rejected '%s' value '%s'; using default.",
                         qPrintable(key), qPrintable(raw.toString()));
            }
            values_.insert(key, accepted);
        }

        settings_read_ = true;
        if (!rejected_keys_.isEmpty()) save();
    }

    void AurSettings::save() const
    {
        if (!settings_read_) return;
        QSettings store(aur::settingsPath(), QSettings::IniFormat);
        for (auto it = values_.constBegin(); it != values_.constEnd(); ++it)
            store.setValue(it.key(), it.value());
        // Keep legacy keys in sync for external tools or scripts.
        store.setValue(QStringLiteral("theme/mode"), themeMode());
        store.setValue(QStringLiteral("theme/accent"), accentColor());
        store.setValue(QStringLiteral("audio/master_volume"), masterVolume());
    }

}
