#pragma once

#include <QWidget>
#include <QComboBox>

#include <QListWidget>

#include <QPushButton>

#include <QLabel>

#include <functional>

#include "aur/audio/SongDocument.hpp"

#include "aur/json.hpp"

namespace aur
{

    // The master effect chain strip: one combo picks the effect type, the list
    // shows the chain with bypass/remove/reorder buttons, the label shows the level.
    class FxChainStrip : public QWidget
    {
        Q_OBJECT
    public:
        explicit FxChainStrip(QWidget *parent = nullptr);

        void setSong(const Json *song);
        // The chain as currently edited; the owner writes it back to the song.
        std::vector<audio::SongDocument::EffectSlot> chain() const;
        // Called every 100 ms while the transport runs.
        void updateMeters(double chainLeft, double chainRight, long long budget);

    signals:
        void chainChanged();

    private:
        std::vector<audio::SongDocument::EffectSlot> currentChain() const;
        void writeChain(const std::vector<audio::SongDocument::EffectSlot> &chain);
        void refresh();

        QComboBox *typeCombo = nullptr;
        QListWidget *chainList = nullptr;
        QPushButton *addButton = nullptr;
        QPushButton *bypassButton = nullptr;
        QPushButton *removeButton = nullptr;
        QPushButton *upButton = nullptr;
        QPushButton *downButton = nullptr;
        QLabel *statusLabel = nullptr;
        Json song;
        bool hasSong = false;
    };

} // namespace aur
