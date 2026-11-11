#pragma once

#include <QWidget>
#include <QComboBox>

#include <QSpinBox>

#include <QSlider>

#include <QLabel>

#include <QPushButton>

#include <QListWidget>

#include <functional>

#include "aur/audio/Arpeggiator.hpp"

#include "aur/audio/SongDocument.hpp"

#include "aur/audio/StepSequencer.hpp"

#include "aur/json.hpp"

namespace aur
{

    // The performance strip: swing, humanise, arpeggiator, pattern chain and the
    // pattern list. Every control is keyboard-navigable and writes back into the song.
    class GrooveBoxPanel : public QWidget
    {
        Q_OBJECT
    public:
        explicit GrooveBoxPanel(QWidget *parent = nullptr);

        void setSong(const Json &song);
        void setChangeCallback(std::function<void(const Json &)> callback);
        // Called when the user asks the panel to play or stop.
        void setTransportHook(std::function<void(bool play)> hook);
        // The patterns the panel shows; the panel can add and remove them.
        void refreshPatterns();

        audio::ArpSettings arpSettings() const;
        audio::GrooveSettings grooveSettings() const;

    signals:
        void historyChanged();
        void transportRequested(bool play);

    private:
        void applySong(const Json &song);
        void writeGroove();
        void writeArp();
        void writeChain();
        void addPattern();
        void removePattern();
        void onChainChanged();
        void moveChain(int delta);
        void addSelectedToChain();
        void onTransport(bool play);

        Json song;
        std::vector<audio::StepPattern> patterns;
        QSlider *swing = nullptr;
        QSlider *humanize = nullptr;
        QSlider *grooveSlider = nullptr;
        QComboBox *arpMode = nullptr;
        QComboBox *arpRate = nullptr;
        QSpinBox *arpGate = nullptr;
        QSpinBox *arpOctave = nullptr;
        QListWidget *patternList = nullptr;
        QListWidget *chainList = nullptr;
        QPushButton *playButton = nullptr;
        QLabel *statusLabel = nullptr;
        std::function<void(const Json &)> onSongChanged;
        std::function<void(bool)> onTransportRequested;
        bool updating = false;
    };

} // namespace aur
