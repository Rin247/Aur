#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace aur
{

    class MidiSession;

    class MidiStrip final : public QWidget
    {
        Q_OBJECT
    public:
        explicit MidiStrip(MidiSession *session, QWidget *parent = nullptr);

        int countInBars() const;
        void cancelRecordArm();

    signals:
        void recordToggled(bool enabled);

    private:
        MidiSession *session_ = nullptr;
        QComboBox *inputCombo_ = nullptr;
        QComboBox *outputCombo_ = nullptr;
        QPushButton *inputButton_ = nullptr;
        QPushButton *outputButton_ = nullptr;
        QPushButton *recordButton_ = nullptr;
        QSpinBox *countInSpin_ = nullptr;
        QLabel *statusLabel_ = nullptr;
    };

}
