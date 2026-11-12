#include "MasterVolumeStrip.hpp"

#include "SharedAudio.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>

namespace aur
{

    MasterVolumeStrip::MasterVolumeStrip(SharedAudio *sharedAudio, const QString &label,
                                         QWidget *parent)
        : QWidget(parent), shared(sharedAudio)
    {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        row->addWidget(new QLabel(label, this));
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 100);
        slider->setValue(shared ? shared->masterVolume() : 100);
        slider->setFixedWidth(140);
        slider->setToolTip("The studio's master volume (the whole mix)");
        row->addWidget(slider);
        // The label would otherwise take the extra width and push the
        // slider to the far side of the panel.
        row->addStretch();
        connect(slider, &QSlider::valueChanged, this, [this](int value) {
            if (shared)
                shared->setMasterVolume(value);
        });
        if (shared)
        {
            connect(shared, &SharedAudio::masterVolumeChanged, this, [this](int percent) {
                if (!slider || slider->value() == percent)
                    return;
                const QSignalBlocker blocker(slider);
                slider->setValue(percent);
            });
        }
    }

} // namespace aur
