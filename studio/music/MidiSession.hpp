#pragma once

#include <QObject>
#include <QComboBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <atomic>
#include <map>
#include <vector>
#include "music/MidiDeviceIo.hpp"
#include "aur/json.hpp"

namespace aur
{

    class MidiSession : public QObject
    {
        Q_OBJECT
    public:
        struct HeldMidiNote
        {
            QString songId;
            int trackIndex = -1;
            int startStep = 0;
            int pitch = 0;
            int velocity = 0;
        };
        struct MidiPlaybackEvent
        {
            qint64 timeMs = 0;
            int priority = 0;
            quint32 message = 0;
        };

        explicit MidiSession(QObject *parent = nullptr);
        ~MidiSession() override;

        MidiDeviceIo &devices() { return midiDevices; }

        // Clock source for playback scheduling from the mixer's sample clock.
        void attachClock(const std::atomic<std::int64_t> *frames,
                         const std::atomic<int> *bpm,
                         const std::atomic<bool> *active)
        {
            playheadFrames = frames;
            playheadBpm = bpm;
            playheadActive = active;
        }

        void refreshDevices(QComboBox *inputCombo, QComboBox *outputCombo);
        void toggleInput(QComboBox *inputCombo, QPushButton *inputButton);
        void toggleOutput(QComboBox *outputCombo, QPushButton *outputButton);
        void setRecording(bool enabled);
        void setCountInBars(int bars) { countInBars = std::clamp(bars, 0, 8); }
        int countIn() const { return countInBars; }
        qint64 countInRemainingMs() const;
        bool inCountIn() const;
        void flushHeldNotes();
        void receiveMessage(quint32 message, qint64 timestampMs = 0);
        void startOutput(const Json &song, const QString &songId);
        void stopOutput();
        bool recording() const { return midiRecording; }
        bool outputActive() const { return midiOutputTimer->isActive(); }
        void setSelectedTrack(int track) { selectedTrack = track; }
        void setRecordTarget(const QString& songId) { recordSongId = songId; }

    signals:
        void noteRecorded(const QString &songId, int trackIndex,
                          int startStep, int pitch, int length, int velocity);
        void sysExReceived(const QByteArray &data, qint64 timestampMs);
        void statusChanged(const QString &status);
        void outputError(const QString &message);
        void outputStopped();

    private:
        int recordStep();
        void commitRecordedNote(const HeldMidiNote &note, int endStep);
        void updateStatus();
        QString sendShortMessage(quint32 message);
        void pumpOutput();

        MidiDeviceIo midiDevices;
        QTimer *midiOutputTimer = nullptr;
        QElapsedTimer midiRecordClock;
        QElapsedTimer midiPlaybackClock;
        QComboBox *inputCombo = nullptr;
        QComboBox *outputCombo = nullptr;
        std::map<std::pair<int, int>, std::vector<HeldMidiNote>> heldMidiNotes;
        std::vector<MidiPlaybackEvent> midiPlaybackEvents;
        qint64 midiLoopDurationMs = 0;
        qint64 midiPlaybackIteration = 0;
        size_t nextMidiPlaybackEvent = 0;
        bool midiRecording = false;
        bool midiOutputFailureShown = false;
        int countInBars = 0;
        qint64 countInEndMs = 0;
        QString midiOutputSongId;
        QString recordSongId;
        int selectedTrack = -1;
        // External sample-clock source (owned by the mixer); null = wall timer.
        const std::atomic<std::int64_t> *playheadFrames = nullptr;
        const std::atomic<int> *playheadBpm = nullptr;
        const std::atomic<bool> *playheadActive = nullptr;
        std::int64_t clockAnchorFrames = 0; // mixer frame when playback began
        std::vector<MidiPlaybackEvent> pendingHardwareEvents; // sorted by timeMs
    };

} // namespace aur
