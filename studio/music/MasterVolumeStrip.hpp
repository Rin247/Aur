#pragma once

// MasterVolumeStrip.hpp - the master volume control.
//
// One slider on the studio's one volume: the DAW and the playtest move the
// same number, and the number is the app's.

#include <QString>
#include <QWidget>

class QSlider;

namespace aur
{

    class SharedAudio;

    class MasterVolumeStrip : public QWidget
    {
        Q_OBJECT
    public:
        explicit MasterVolumeStrip(SharedAudio *shared, const QString &label = QString(),
                                   QWidget *parent = nullptr);

    private:
        SharedAudio *shared = nullptr;
        QSlider *slider = nullptr;
    };

} // namespace aur
