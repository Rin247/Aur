#include "MidiDeviceIo.hpp"

#ifdef _WIN32
#include <windows.h>
#include <array>
#endif

namespace aur {
namespace {

#ifdef _WIN32
QString inputError(MMRESULT result) {
    std::array<wchar_t, MAXERRORLENGTH> text{};
    if (midiInGetErrorTextW(result, text.data(), static_cast<UINT>(text.size())) == MMSYSERR_NOERROR) {
        return QString::fromWCharArray(text.data());
    }
    return QString("Windows MIDI input error %1").arg(result);
}

QString outputError(MMRESULT result) {
    std::array<wchar_t, MAXERRORLENGTH> text{};
    if (midiOutGetErrorTextW(result, text.data(), static_cast<UINT>(text.size())) == MMSYSERR_NOERROR) {
        return QString::fromWCharArray(text.data());
    }
    return QString("Windows MIDI output error %1").arg(result);
}
#endif

} // namespace

MidiDeviceIo::MidiDeviceIo(QObject* parent) : QObject(parent) {}

MidiDeviceIo::~MidiDeviceIo() {
    closeInput();
    closeOutput();
}

QVector<MidiDeviceInfo> MidiDeviceIo::inputDevices() {
    QVector<MidiDeviceInfo> devices;
#ifdef _WIN32
    const UINT count = midiInGetNumDevs();
    devices.reserve(static_cast<qsizetype>(count));
    for (UINT id = 0; id < count; ++id) {
        MIDIINCAPSW capabilities{};
        if (midiInGetDevCapsW(id, &capabilities, sizeof(capabilities)) == MMSYSERR_NOERROR) {
            devices.push_back({id, QString::fromWCharArray(capabilities.szPname)});
        }
    }
#endif
    return devices;
}

QVector<MidiDeviceInfo> MidiDeviceIo::outputDevices() {
    QVector<MidiDeviceInfo> devices;
#ifdef _WIN32
    const UINT count = midiOutGetNumDevs();
    devices.reserve(static_cast<qsizetype>(count));
    for (UINT id = 0; id < count; ++id) {
        MIDIOUTCAPSW capabilities{};
        if (midiOutGetDevCapsW(id, &capabilities, sizeof(capabilities)) == MMSYSERR_NOERROR) {
            devices.push_back({id, QString::fromWCharArray(capabilities.szPname)});
        }
    }
#endif
    return devices;
}

bool MidiDeviceIo::inputConnected() const {
#ifdef _WIN32
    return input != nullptr;
#else
    return false;
#endif
}

bool MidiDeviceIo::outputConnected() const {
#ifdef _WIN32
    return output != nullptr;
#else
    return false;
#endif
}

QString MidiDeviceIo::openInput(unsigned int deviceId) {
#ifdef _WIN32
    const QString closeError = closeInput();
    if (!closeError.isEmpty()) return closeError;
    HMIDIIN openedInput = nullptr;
    MMRESULT result = midiInOpen(
        &openedInput, deviceId,
        reinterpret_cast<DWORD_PTR>(&MidiDeviceIo::inputCallback),
        reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) return inputError(result);
    result = midiInStart(openedInput);
    if (result != MMSYSERR_NOERROR) {
        midiInClose(openedInput);
        return inputError(result);
    }
    input = openedInput;
    return {};
#else
    Q_UNUSED(deviceId);
    return "MIDI hardware is not supported on this platform.";
#endif
}

QString MidiDeviceIo::openOutput(unsigned int deviceId) {
#ifdef _WIN32
    const QString closeError = closeOutput();
    if (!closeError.isEmpty()) return closeError;
    HMIDIOUT openedOutput = nullptr;
    const MMRESULT result = midiOutOpen(&openedOutput, deviceId, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) return outputError(result);
    output = openedOutput;
    return {};
#else
    Q_UNUSED(deviceId);
    return "MIDI hardware is not supported on this platform.";
#endif
}

QString MidiDeviceIo::closeInput() {
#ifdef _WIN32
    if (!input) return {};
    const HMIDIIN closingInput = input;
    input = nullptr;
    MMRESULT firstError = midiInStop(closingInput);
    const MMRESULT resetResult = midiInReset(closingInput);
    if (firstError == MMSYSERR_NOERROR && resetResult != MMSYSERR_NOERROR) {
        firstError = resetResult;
    }
    const MMRESULT closeResult = midiInClose(closingInput);
    if (firstError == MMSYSERR_NOERROR && closeResult != MMSYSERR_NOERROR) {
        firstError = closeResult;
    }
    return firstError == MMSYSERR_NOERROR ? QString{} : inputError(firstError);
#else
    return {};
#endif
}

QString MidiDeviceIo::closeOutput() {
#ifdef _WIN32
    if (!output) return {};
    const HMIDIOUT closingOutput = output;
    output = nullptr;
    const MMRESULT result = midiOutClose(closingOutput);
    return result == MMSYSERR_NOERROR ? QString{} : outputError(result);
#else
    return {};
#endif
}

QString MidiDeviceIo::sendShortMessage(quint32 message) {
#ifdef _WIN32
    if (!output) return "No MIDI output device is connected.";
    const MMRESULT result = midiOutShortMsg(output, static_cast<DWORD>(message));
    return result == MMSYSERR_NOERROR ? QString{} : outputError(result);
#else
    Q_UNUSED(message);
    return "MIDI hardware is not supported on this platform.";
#endif
}

#ifdef _WIN32
void CALLBACK MidiDeviceIo::inputCallback(HMIDIIN, UINT message, DWORD_PTR instance,
                                          DWORD_PTR messageData, DWORD_PTR) {
    if (message != MIM_DATA || instance == 0) return;
    auto* device = reinterpret_cast<MidiDeviceIo*>(instance);
    emit device->messageReceived(static_cast<quint32>(messageData));
}
#endif

} // namespace aur
