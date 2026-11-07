#pragma once

#include <QObject>
#include <QComboBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <map>
#include <vector>
#include "MidiDeviceIo.hpp"
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

        void refreshDevices(QComboBox *inputCombo, QComboBox *outputCombo);
        void toggleInput(QComboBox *inputCombo, QPushButton *inputButton);
        void toggleOutput(QComboBox *outputCombo, QPushButton *outputButton);
        void setRecording(bool enabled);
        void flushHeldNotes();
        void receiveMessage(quint32 message);
        void startOutput(const Json &song, const QString &songId);
        void stopOutput();
        bool recording() const { return midiRecording; }
        bool outputActive() const { return midiOutputTimer->isActive(); }
        void setSelectedTrack(int track) { selectedTrack = track; }
        void setRecordTarget(const QString& songId) { recordSongId = songId; }

    signals:
        void noteRecorded(const QString &songId, int trackIndex,
                          int startStep, int pitch, int length, int velocity);
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
        QString midiOutputSongId;
        QString recordSongId;
        int selectedTrack = -1;
    };

} // namespace aur
