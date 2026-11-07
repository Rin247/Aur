#include "QtAudioOutput.hpp"

#include <QAudioDevice>
#include <QAudioFormat>

namespace aur
{
    namespace audio
    {

        QtAudioOutput::QtAudioOutput(QObject *parent) : QObject(parent)
        {
            device = QMediaDevices::defaultAudioOutput();
        }

        QtAudioOutput::~QtAudioOutput()
        {
            stop();
        }

        void QtAudioOutput::start(int sampleRate)
        {
            stop();
            if (device.isNull())
                return;
            QAudioFormat format;
            format.setSampleRate(sampleRate);
            format.setChannelCount(2);
            format.setSampleFormat(QAudioFormat::Float);
            sink = new QAudioSink(device, format, this);
            sink->setBufferSize(sampleRate / 10); // 100 ms of float stereo
            mixer.setSampleRate(sampleRate);
            sink->start([this](QSpan<float> span)
                        {
                if (span.empty())
                    return;
                mixer.render(span.data(), static_cast<int>(span.size() / 2));
            });
        }

        void QtAudioOutput::stop()
        {
            if (sink)
            {
                sink->stop();
                sink->deleteLater();
                sink = nullptr;
            }
        }

    } // namespace audio
} // namespace aur
