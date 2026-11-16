#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <array>

constexpr unsigned int kMidiSysExBufferCount = 4;
constexpr unsigned int kMidiSysExBufferSize = 4096;

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace aur {

struct MidiDeviceInfo {
    unsigned int id = 0;
    QString name;
};

class MidiDeviceIo : public QObject {
    Q_OBJECT

#ifdef _WIN32
    HMIDIIN input = nullptr;
    HMIDIOUT output = nullptr;
    struct SysExBuffer {
        MIDIHDR header{};
        std::array<BYTE, kMidiSysExBufferSize> data{};
    };
    std::array<SysExBuffer, kMidiSysExBufferCount> sysExBuffers{};
    bool sysExPrepared = false;
    bool prepareSysExBuffers();
    void releaseSysExBuffers();
    static void CALLBACK inputCallback(HMIDIIN handle, UINT message,
                                       DWORD_PTR instance, DWORD_PTR messageData,
                                       DWORD_PTR timestamp);
#endif

public:
    explicit MidiDeviceIo(QObject* parent = nullptr);
    ~MidiDeviceIo() override;

    static QVector<MidiDeviceInfo> inputDevices();
    static QVector<MidiDeviceInfo> outputDevices();
    bool inputConnected() const;
    bool outputConnected() const;
    QString openInput(unsigned int deviceId);
    QString openOutput(unsigned int deviceId);
    QString closeInput();
    QString closeOutput();
    QString sendShortMessage(quint32 message);

signals:
    void messageReceived(quint32 message, qint64 timestampMs);
    void sysExReceived(const QByteArray &data, qint64 timestampMs);
};

} // namespace aur
