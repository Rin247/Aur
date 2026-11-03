#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QSpinBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <QResizeEvent>
#include <functional>
#include <map>
#include <vector>
#include "MidiDeviceIo.hpp"
#include "aur/project.hpp"

namespace aur {

class PianoRollCanvasWidget : public QWidget {
    Json song;
    int trackIndex = -1;
    int selectedNote = -1;
    int dragOriginStep = 0;
    int dragOriginPitch = 0;
    int dragOriginLength = 1;
    int firstVisibleStep = 0;
    QPoint dragStartPos;
    bool resizingNote = false;
    std::function<void(const Json&)> onSongChanged;

    int noteAt(const QPoint& pos) const;
    void notifySongChanged();

public:
    explicit PianoRollCanvasWidget(QWidget* parent = nullptr);
    void setSong(const Json& song, int selectedTrack);
    void setSelectedTrack(int selectedTrack);
    void setVisibleStep(int firstStep);
    int contentWidth() const;
    void setChangeCallback(std::function<void(const Json&)> callback);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};

class MusicPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* songCombo = nullptr;
    QListWidget* trackList = nullptr;
    PianoRollCanvasWidget* pianoRoll = nullptr;
    QLineEdit* songName = nullptr;
    QTabWidget* editorTabs = nullptr;
    QTableWidget* sessionGrid = nullptr;
    QSpinBox* tempo = nullptr;
    QComboBox* previewSynth = nullptr;
    QComboBox* instrument = nullptr;
    QSpinBox* bank = nullptr;
    QSpinBox* channel = nullptr;
    QSpinBox* volume = nullptr;
    QSpinBox* pan = nullptr;
    QPushButton* playButton = nullptr;
    QPushButton* playSceneButton = nullptr;
    QComboBox* midiInputCombo = nullptr;
    QComboBox* midiOutputCombo = nullptr;
    QPushButton* midiInputButton = nullptr;
    QPushButton* midiOutputButton = nullptr;
    QPushButton* recordMidiButton = nullptr;
    QLabel* midiStatusLabel = nullptr;
    QMediaPlayer* player = nullptr;
    QAudioOutput* audioOutput = nullptr;
    QTemporaryDir previewDirectory;
    MidiDeviceIo midiDevices;
    QTimer* midiOutputTimer = nullptr;
    QElapsedTimer midiRecordClock;
    QElapsedTimer midiPlaybackClock;
    struct HeldMidiNote {
        QString songId;
        int trackIndex = -1;
        int startStep = 0;
        int pitch = 0;
        int velocity = 0;
    };
    struct MidiPlaybackEvent {
        qint64 timeMs = 0;
        int priority = 0;
        quint32 message = 0;
    };
    std::map<std::pair<int, int>, std::vector<HeldMidiNote>> heldMidiNotes;
    std::vector<MidiPlaybackEvent> midiPlaybackEvents;
    qint64 midiLoopDurationMs = 0;
    qint64 midiPlaybackIteration = 0;
    size_t nextMidiPlaybackEvent = 0;
    bool midiRecording = false;
    bool midiOutputFailureShown = false;
    QString midiOutputSongId;
    int previewCount = 0;
    QString currentSongId;
    int selectedTrack = -1;
    int selectedScene = -1;
    int activeScene = -1;
    bool updatingControls = false;

    int midiRecordStep();
    void commitRecordedMidiNote(const HeldMidiNote& note, int endStep);
    void flushHeldMidiNotes();
    void updateMidiStatus();
    Json* currentSong();
    void saveCurrentSong(const Json& song);
    void refreshTracks();
    void selectSong(const QString& id);
    void updateTrackProperties();
    void updateTrackProperty(const char* key, int value);
    void refreshSessionGrid();
    void toggleClip(int sceneIndex, int trackIndex);
    Json makeScenePreview(int sceneIndex);
    void playPreview(const Json* previewSong = nullptr);
    void stopPreview();
    void refreshMidiDevices();
    void toggleMidiInput();
    void toggleMidiOutput();
    void setMidiRecording(bool enabled);
    void receiveMidiMessage(quint32 message);
    void startMidiOutput(const Json& song);
    void pumpMidiOutput();
    void stopMidiOutput();
    QString sendMidiMessage(quint32 message);
    void importMidi();
    void exportMidi();

public:
    explicit MusicPanel(Project* project = nullptr, QWidget* parent = nullptr);
    ~MusicPanel() override;
    void setProject(Project* p);
    void refreshSongs();
    void generateQuickSong(const std::string& style);
};

} // namespace aur
