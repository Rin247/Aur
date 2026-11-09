#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QString>
#include <functional>
#include <map>
#include "music/MidiSession.hpp"
#include "music/PianoRollCanvas.hpp"
#include "music/VelocityLaneStrip.hpp"
#include "music/ArrangementCanvas.hpp"
#include "music/ArrangementCanvas.hpp"
#include "music/ScaleSelector.hpp"
#include "music/ClipLauncherStates.hpp"
#include "music/QtAudioOutput.hpp"
#include "SongHistory.hpp"
#include "aur/audio/RealtimeMixer.hpp"
#include "aur/project.hpp"

namespace aur
{

    class MusicPanel : public QWidget
    {
        Q_OBJECT
        Project *project = nullptr;
        QComboBox *songCombo = nullptr;
        QListWidget *trackList = nullptr;
        PianoRollCanvasWidget *pianoRoll = nullptr;

        VelocityLaneStrip *velocityStrip = nullptr;
        ArrangementCanvas *arrangementCanvas = nullptr;
        ScaleSelector *scaleSelector = nullptr;
        ClipLauncherStates clipLauncher;
        int launcherPlayingScene = -1;
        int recordingScene = -1; // -1 = record to track only
        void syncScaleSelector(const Json &song);
        QLineEdit *songName = nullptr;
        QTabWidget *editorTabs = nullptr;
        QTableWidget *sessionGrid = nullptr;
        QSpinBox *tempo = nullptr;
        QComboBox *previewSynth = nullptr;
        QComboBox *instrument = nullptr;
        QSpinBox *bank = nullptr;
        QSpinBox *channel = nullptr;
        QSpinBox *volume = nullptr;
        QSpinBox *pan = nullptr;
        QPushButton *playButton = nullptr;
        QPushButton *playSceneButton = nullptr;
        QComboBox *midiInputCombo = nullptr;
        QComboBox *midiOutputCombo = nullptr;
        QPushButton *midiInputButton = nullptr;
        QPushButton *midiOutputButton = nullptr;
        QPushButton *recordMidiButton = nullptr;
        QSpinBox *countInBarsSpin = nullptr;
        QLabel *midiStatusLabel = nullptr;
        // Master FX chain strip (P8.3). One combo picks the effect type, the
        // list shows the chain with bypass/remove buttons, and the label shows
        // post-chain level + CPU budget while playing.
        QComboBox *fxTypeCombo = nullptr;
        QListWidget *fxChainList = nullptr;
        QPushButton *fxAddButton = nullptr;
        QPushButton *fxBypassButton = nullptr;
        QPushButton *fxRemoveButton = nullptr;
        QPushButton *fxUpButton = nullptr;
        QPushButton *fxDownButton = nullptr;
        QLabel *fxStatusLabel = nullptr;
        audio::QtAudioOutput *audioOutput = nullptr;
        MidiSession *midiSession = nullptr;
        editor::SongHistory songHistory;
        QString currentSongId;
        int selectedTrack = -1;
        int selectedScene = -1;
        int activeScene = -1;
        bool updatingControls = false;

        int clampedInt(const Json &value, int fallback, int low, int high) const;
        Json *currentSong();
        const Json *currentSong() const;
        void saveCurrentSong(const Json &song);
        void refreshTracks();
        void selectSong(const QString &id);
        void updateTrackProperties();
        void updateTrackProperty(const char *key, int value);
        void beginTrackEdit();
        void refreshFxChain();
        std::vector<audio::SongDocument::EffectSlot> currentChain() const;
        void writeChain(const std::vector<audio::SongDocument::EffectSlot> &chain);
        void refreshSessionGrid();
        void toggleClip(int sceneIndex, int trackIndex);
        Json makeScenePreview(int sceneIndex);
        void playPreview(const Json *previewSong = nullptr);
        void stopPreview();
        void playArrangement();
        void exportArrangementMidi();
        void importMidi();
        void exportMidi();

    public:
        explicit MusicPanel(Project *project = nullptr, QWidget *parent = nullptr);
        ~MusicPanel() override;
        void setProject(Project *p);
        void refreshSongs();
        void generateQuickSong(const std::string &style);
        bool undoSongEdit();
        bool redoSongEdit();

    signals:
        void undoStateChanged(bool canUndo, bool canRedo);
    };

} // namespace aur
