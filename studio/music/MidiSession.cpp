#include "MidiSession.hpp"

#include <QComboBox>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>

#include <algorithm>

namespace aur
{

    namespace
    {
        constexpr int kDefaultBpm = 120;

        int clampedInt(const Json &value, int fallback, int low, int high)
        {
            return value.is_number() ? std::clamp(value.as_int(), low, high) : fallback;
        }

        int clampedField(const Json &object, const char *key, int fallback, int low, int high)
        {
            return object.contains(key) ? clampedInt(object[key], fallback, low, high) : fallback;
        }
    } // namespace

    MidiSession::MidiSession(QObject *parent) : QObject(parent)
    {
        midiOutputTimer = new QTimer(this);
        midiOutputTimer->setInterval(2);
        connect(midiOutputTimer, &QTimer::timeout, this,
                [this]()
                { pumpOutput(); });
        connect(&midiDevices, &MidiDeviceIo::messageReceived, this,
                [this](quint32 message)
                { receiveMessage(message); });
    }

    MidiSession::~MidiSession()
    {
        midiDevices.closeInput();
        midiDevices.closeOutput();
    }

    void MidiSession::refreshDevices(QComboBox *inputCombo, QComboBox *outputCombo)
    {
        this->inputCombo = inputCombo;
        this->outputCombo = outputCombo;
        const int inputId = inputCombo->currentData().toInt();
        const int outputId = outputCombo->currentData().toInt();
        const QSignalBlocker inputBlocker(inputCombo);
        const QSignalBlocker outputBlocker(outputCombo);
        inputCombo->clear();
        outputCombo->clear();
        inputCombo->addItem("Select MIDI input…", -1);
        outputCombo->addItem("Select MIDI output…", -1);
        for (const MidiDeviceInfo &device : MidiDeviceIo::inputDevices())
        {
            inputCombo->addItem(device.name, static_cast<int>(device.id));
        }
        for (const MidiDeviceInfo &device : MidiDeviceIo::outputDevices())
        {
            outputCombo->addItem(device.name, static_cast<int>(device.id));
        }
        const int inputIndex = inputCombo->findData(inputId);
        const int outputIndex = outputCombo->findData(outputId);
        inputCombo->setCurrentIndex(inputIndex >= 0 ? inputIndex : 0);
        outputCombo->setCurrentIndex(outputIndex >= 0 ? outputIndex : 0);
        updateStatus();
    }

    void MidiSession::toggleInput(QComboBox *combo, QPushButton *button)
    {
        if (midiDevices.inputConnected())
        {
            setRecording(false);
            const QString error = midiDevices.closeInput();
            button->setText("Connect");
            updateStatus();
            if (!error.isEmpty())
                QMessageBox::warning(button->parentWidget(), "MIDI Input", error);
            return;
        }
        bool validId = false;
        const uint deviceId = combo->currentData().toUInt(&validId);
        if (!validId)
        {
            QMessageBox::information(button->parentWidget(), "MIDI Input",
                                     "Connect a MIDI input device first.");
            return;
        }
        const QString error = midiDevices.openInput(deviceId);
        if (!error.isEmpty())
        {
            QMessageBox::warning(button->parentWidget(), "MIDI Input", error);
            return;
        }
        button->setText("Disconnect");
        updateStatus();
    }

    void MidiSession::toggleOutput(QComboBox *combo, QPushButton *button)
    {
        if (midiDevices.outputConnected())
        {
            stopOutput();
            const QString error = midiDevices.closeOutput();
            button->setText("Connect");
            updateStatus();
            if (!error.isEmpty())
                QMessageBox::warning(button->parentWidget(), "MIDI Output", error);
            return;
        }
        bool validId = false;
        const uint deviceId = combo->currentData().toUInt(&validId);
        if (!validId)
        {
            QMessageBox::information(button->parentWidget(), "MIDI Output",
                                     "Connect a MIDI output device first.");
            return;
        }
        const QString error = midiDevices.openOutput(deviceId);
        if (!error.isEmpty())
        {
            QMessageBox::warning(button->parentWidget(), "MIDI Output", error);
            return;
        }
        button->setText("Disconnect");
        updateStatus();
    }

    void MidiSession::setRecording(bool enabled)
    {
        midiRecording = enabled;
        if (enabled)
        {
            heldMidiNotes.clear();
            midiRecordClock.start();
        }
        updateStatus();
    }

    int MidiSession::recordStep()
    {
        const qint64 elapsed = midiRecordClock.isValid() ? midiRecordClock.elapsed() : 0;
        const qint64 steps = qRound64(elapsed * kDefaultBpm * 4.0 / 60000.0);
        return static_cast<int>(std::clamp<qint64>(steps, 0, 1000000));
    }

    void MidiSession::commitRecordedNote(const HeldMidiNote &note, int endStep)
    {
        const int length = std::max(1, endStep - note.startStep);
        emit noteRecorded(note.songId, note.trackIndex,
                          note.startStep, note.pitch, length, note.velocity);
    }

    void MidiSession::flushHeldNotes()
    {
        const int endStep = recordStep();
        for (const auto &[key, notes] : heldMidiNotes)
        {
            Q_UNUSED(key);
            for (const HeldMidiNote &note : notes)
                commitRecordedNote(note, endStep);
        }
        heldMidiNotes.clear();
    }

    void MidiSession::updateStatus()
    {
        QStringList connections;
        if (midiDevices.inputConnected() && inputCombo)
            connections.push_back("In: " + inputCombo->currentText());
        if (midiDevices.outputConnected() && outputCombo)
            connections.push_back("Out: " + outputCombo->currentText());
        if (midiRecording)
            connections.push_back("Recording");
        emit statusChanged(connections.isEmpty()
                               ? QString("No MIDI devices connected")
                               : connections.join(" · "));
    }

    void MidiSession::receiveMessage(quint32 message)
    {
        if (!midiRecording)
            return;
        const int status = static_cast<int>(message & 0xff);
        const int kind = status & 0xf0;
        if (kind != 0x80 && kind != 0x90)
            return;
        const int channelNumber = status & 0x0f;
        const int pitch = static_cast<int>((message >> 8) & 0x7f);
        const int velocity = static_cast<int>((message >> 16) & 0x7f);
        const auto key = std::make_pair(channelNumber, pitch);
        const bool noteOn = kind == 0x90 && velocity > 0;
        if (noteOn)
        {
            heldMidiNotes[key].push_back(
                {recordSongId, selectedTrack, recordStep(), pitch, velocity});
            return;
        }
        auto held = heldMidiNotes.find(key);
        if (held == heldMidiNotes.end() || held->second.empty())
            return;
        const HeldMidiNote note = held->second.back();
        held->second.pop_back();
        if (held->second.empty())
            heldMidiNotes.erase(held);
        commitRecordedNote(note, recordStep());
    }

    void MidiSession::startOutput(const Json &song, const QString &songId)
    {
        stopOutput();
        if (!midiDevices.outputConnected() || !song.contains("tracks") ||
            !song["tracks"].is_array())
            return;
        midiOutputSongId = songId;
        midiPlaybackEvents.clear();
        midiPlaybackIteration = 0;
        nextMidiPlaybackEvent = 0;
        midiOutputFailureShown = false;
        const int bpm = clampedField(song, "tempo", kDefaultBpm, 30, 240);
        const double millisecondsPerStep = 60000.0 / (bpm * 4.0);
        int totalSteps = clampedField(song, "bars", 8, 1, 128) * 16;
        const auto message = [](int status, int data1, int data2 = 0)
        {
            return static_cast<quint32>((status & 0xff) | ((data1 & 0x7f) << 8) |
                                        ((data2 & 0x7f) << 16));
        };
        for (size_t trackIndex = 0; trackIndex < song["tracks"].as_array().size(); ++trackIndex)
        {
            const Json &track = song["tracks"][trackIndex];
            const int channelNumber = clampedField(track, "channel", 0, 0, 15);
            const int bankValue = clampedField(track, "bank", 0, 0, 16383);
            const int programValue = clampedField(track, "program", 0, 0, 127);
            const int volumeValue = clampedField(track, "volume", 90, 0, 127);
            const int panValue = clampedField(track, "pan", 64, 0, 127);
            const int channelStatus = channelNumber;
            midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 0, bankValue / 128)});
            midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 32, bankValue % 128)});
            midiPlaybackEvents.push_back({0, 0, message(0xc0 | channelStatus, programValue)});
            midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 7, volumeValue)});
            midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 10, panValue)});
            if (!track.contains("notes") || !track["notes"].is_array())
                continue;
            for (const Json &note : track["notes"].as_array())
            {
                if (!note.is_array() || note.as_array().size() < 3)
                    continue;
                const int startStep = std::clamp(note[0].as_int(), 0, 4095);
                const int pitch = std::clamp(note[1].as_int(), 0, 127);
                const int velocity = note.as_array().size() >= 4
                                         ? std::clamp(note[3].as_int(), 0, 127)
                                         : 90;
                const int length = std::clamp(note[2].as_int(), 1, 4096);
                const int endStep = std::min(4096, startStep + length);
                totalSteps = std::max(totalSteps, endStep);
                midiPlaybackEvents.push_back({qRound64(startStep * millisecondsPerStep), 2,
                                              message(0x90 | channelStatus, pitch, velocity)});
                midiPlaybackEvents.push_back({qRound64(endStep * millisecondsPerStep), 1,
                                              message(0x80 | channelStatus, pitch)});
            }
        }
        midiLoopDurationMs = std::clamp<qint64>(
            qRound64(totalSteps * millisecondsPerStep), 1, 180000);
        for (MidiPlaybackEvent &event : midiPlaybackEvents)
        {
            event.timeMs = std::clamp<qint64>(event.timeMs, 0, midiLoopDurationMs - 1);
        }
        std::stable_sort(midiPlaybackEvents.begin(), midiPlaybackEvents.end(),
                         [](const MidiPlaybackEvent &left, const MidiPlaybackEvent &right)
                         {
                             if (left.timeMs != right.timeMs)
                                 return left.timeMs < right.timeMs;
                             return left.priority < right.priority;
                         });
        midiPlaybackClock.start();
        midiOutputTimer->start();
    }

    void MidiSession::pumpOutput()
    {
        if (!midiDevices.outputConnected() || midiPlaybackEvents.empty() ||
            midiLoopDurationMs <= 0)
        {
            midiOutputTimer->stop();
            return;
        }
        const qint64 now = midiPlaybackClock.elapsed();
        while (true)
        {
            if (nextMidiPlaybackEvent >= midiPlaybackEvents.size())
            {
                ++midiPlaybackIteration;
                nextMidiPlaybackEvent = 0;
            }
            const MidiPlaybackEvent &event = midiPlaybackEvents[nextMidiPlaybackEvent];
            const qint64 due = midiPlaybackIteration * midiLoopDurationMs + event.timeMs;
            if (now < due)
                return;
            const QString error = sendShortMessage(event.message);
            if (!error.isEmpty())
            {
                midiOutputTimer->stop();
                midiPlaybackEvents.clear();
                if (!midiOutputFailureShown)
                {
                    midiOutputFailureShown = true;
                    emit outputError(error);
                }
                return;
            }
            ++nextMidiPlaybackEvent;
        }
    }

    void MidiSession::stopOutput()
    {
        midiOutputTimer->stop();
        midiPlaybackEvents.clear();
        nextMidiPlaybackEvent = 0;
        midiLoopDurationMs = 0;
        if (!midiDevices.outputConnected())
            return;
        for (int channelNumber = 0; channelNumber < 16; ++channelNumber)
        {
            const quint32 allNotesOff = static_cast<quint32>(
                0xb0 | channelNumber | (123 << 8));
            const QString error = sendShortMessage(allNotesOff);
            if (!error.isEmpty())
            {
                if (!midiOutputFailureShown)
                {
                    midiOutputFailureShown = true;
                    emit outputError(error);
                }
                break;
            }
        }
    }

    QString MidiSession::sendShortMessage(quint32 message)
    {
        return midiDevices.sendShortMessage(message);
    }

} // namespace aur
