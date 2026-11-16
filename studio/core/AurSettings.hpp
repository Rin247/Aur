#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace aur
{

    // Carried over from Hydra/src/HydraSettings.h: one schema-validated QSettings
    // store with the settingsRead startup guard.
    class AurSettings : public QObject
    {
        Q_OBJECT
    public:
        struct Definition
        {
            QVariant defaultValue;
            QVariant minimum;
            QVariant maximum;
            QStringList allowed;
            QString pattern;
        };

        static AurSettings &instance();
        explicit AurSettings(QObject *parent = nullptr);

        void load();
        void save() const;
        void resetToDefaults();

        bool settingsRead() const { return settings_read_; }
        const QSet<QString> &rejectedKeys() const { return rejected_keys_; }
        QStringList themeModes() const;

        QVariant validate(const QString &key, const QVariant &stored,
                          bool *rejected = nullptr) const;
        QVariant value(const QString &key) const;
        void setValue(const QString &key, const QVariant &val);

        QString themeMode() const { return value(QStringLiteral("appearance/themeMode")).toString(); }
        void setThemeMode(const QString &mode) { setValue(QStringLiteral("appearance/themeMode"), mode); }

        QString accentColor() const { return value(QStringLiteral("appearance/accentColor")).toString(); }
        void setAccentColor(const QString &hex) { setValue(QStringLiteral("appearance/accentColor"), hex); }

        QString colorBlindMode() const { return value(QStringLiteral("appearance/colorBlindMode")).toString(); }
        void setColorBlindMode(const QString &mode) { setValue(QStringLiteral("appearance/colorBlindMode"), mode); }

        int uiScale() const { return value(QStringLiteral("appearance/uiScale")).toInt(); }
        void setUiScale(int pct) { setValue(QStringLiteral("appearance/uiScale"), pct); }

        double zoom() const { return value(QStringLiteral("appearance/zoom")).toDouble(); }
        void setZoom(double factor) { setValue(QStringLiteral("appearance/zoom"), factor); }

        bool dyslexiaFont() const { return value(QStringLiteral("appearance/dyslexiaFont")).toBool(); }
        void setDyslexiaFont(bool on) { setValue(QStringLiteral("appearance/dyslexiaFont"), on); }

        QString graphicsBackend() const { return value(QStringLiteral("graphics/backend")).toString(); }
        void setGraphicsBackend(const QString &api) { setValue(QStringLiteral("graphics/backend"), api); }

        int masterVolume() const { return value(QStringLiteral("audio/masterVolume")).toInt(); }
        void setMasterVolume(int vol) { setValue(QStringLiteral("audio/masterVolume"), vol); }

        bool audioEnabled() const { return value(QStringLiteral("audio/enabled")).toBool(); }
        void setAudioEnabled(bool on) { setValue(QStringLiteral("audio/enabled"), on); }

        int windowTab() const { return value(QStringLiteral("window/tab")).toInt(); }
        void setWindowTab(int tab) { setValue(QStringLiteral("window/tab"), tab); }

    signals:
        void settingChanged(const QString &key);
        void themeChanged();

    private:
        void buildDefinitions();

        QHash<QString, Definition> defs_;
        QVariantMap values_;
        QSet<QString> rejected_keys_;
        bool settings_read_ = false;
    };

}
