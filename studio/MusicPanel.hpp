#pragma once

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <map>

#include "music/SongHistory.hpp"
#include "aur/audio/RealtimeMixer.hpp"
#include "aur/project.hpp"
#include "music/ArrangementCanvas.hpp"
#include "music/ClipLauncherStates.hpp"
#include "music/FxChainStrip.hpp"
#include "music/GrooveBoxPanel.hpp"
#include "music/MidiSession.hpp"
#include "music/MidiStrip.hpp"
#include "music/PianoRollCanvas.hpp"
#include "music/QtAudioOutput.hpp"
#include "music/ScaleSelector.hpp"
#include "music/StepSequencerCanvas.hpp"
#include "music/VelocityLaneStrip.hpp"

namespace aur
{

    class SharedAudio;

    class MusicPanel : public QWidget
    {
        Q_OBJECT
        enum class Transport { Stopped, Playing, Paused };
        Project *project = nullptr;
        QComboBox *songCombo = nullptr;
        QListWidget *trackList = nullptr;
        PianoRollCanvasWidget *pianoRoll = nullptr;
        VelocityLaneStrip *velocityStrip = nullptr;
        ArrangementCanvas *arrangementCanvas = nullptr;
        ScaleSelector *scaleSelector = nullptr;
        ClipLauncherStates clipLauncher;
        int launcherPlayingScene = -1;
        int recordingScene = -1;
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
        MidiStrip *midiStrip = nullptr;
        FxChainStrip *fxStrip = nullptr;
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
        Transport transport = Transport::Stopped;
        bool midiOutputWasOn = false;
        Json lastPlayedSong;

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
        void buildEditorTabs(QSplitter *splitter);
        void setTransport(Transport state);
        std::vector<audio::SongDocument::EffectSlot> currentChain() const;
        void writeChain(const std::vector<audio::SongDocument::EffectSlot> &chain);
        void refreshSessionGrid();
        void toggleClip(int sceneIndex, int trackIndex);
        Json makeScenePreview(int sceneIndex);
        void playPreview(const Json *previewSong = nullptr);
        void pausePreview();
        void resumePreview();
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
        Q_INVOKABLE void selectPageTab(const QString &name);

    signals:
        void undoStateChanged(bool canUndo, bool canRedo);
    };

} // namespace aur
