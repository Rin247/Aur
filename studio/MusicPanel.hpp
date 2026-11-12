#pragma once

#include <QWidget>
#include <QComboBox>

#include <QListWidget>

#include <QPushButton>
#include <QSlider>

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

#include "music/ScaleSelector.hpp"

#include "music/ClipLauncherStates.hpp"

#include "music/QtAudioOutput.hpp"

#include "music/FxChainStrip.hpp"

#include "music/StepSequencerCanvas.hpp"

#include "music/GrooveBoxPanel.hpp"

#include "SongHistory.hpp"

#include "aur/audio/RealtimeMixer.hpp"

#include "aur/project.hpp"

namespace aur
{

    class SharedAudio;

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
        // Master FX chain strip (P8.3) lives in its own widget.
        FxChainStrip *fxStrip = nullptr;
        // Groovebox layer: the 128-step grid and the performance strip.
        StepSequencerCanvas *stepCanvas = nullptr;
        GrooveBoxPanel *groovePanel = nullptr;
        audio::QtAudioOutput *audioOutput = nullptr;
        SharedAudio *shared = nullptr;
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
        void refreshGroovebox();
        void toggleClip(int sceneIndex, int trackIndex);
        Json makeScenePreview(int sceneIndex);
        void playPreview(const Json *previewSong = nullptr);
        void stopPreview();
        void hideEvent(QHideEvent *event) override;
        void playArrangement();
        void exportArrangementMidi();
        bool flattenedArrangement(Json &flat) const;
        void importMidi();
        void exportMidi();

    public:
        explicit MusicPanel(Project *project = nullptr, SharedAudio *shared = nullptr,
                           QWidget *parent = nullptr);
        ~MusicPanel() override;
        void setProject(Project *p);
        void refreshSongs();
        void generateQuickSong(const std::string &style);
        bool undoSongEdit();
        bool redoSongEdit();
        void commitSongEdit(const Json &song);
        // Shows one of the DAW's own tabs (the render sweep uses it).
        void selectEditorTab(const QString &name);

    signals:
        void undoStateChanged(bool canUndo, bool canRedo);
    };

} // namespace aur
