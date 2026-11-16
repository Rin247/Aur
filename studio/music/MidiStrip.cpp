#include "music/MidiStrip.hpp"

#include "music/MidiSession.hpp"
#include "theme/LabelRoles.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

namespace aur
{

    MidiStrip::MidiStrip(MidiSession *session, QWidget *parent)
        : QWidget(parent), session_(session)
    {
        auto *bar = new QHBoxLayout(this);
        bar->setContentsMargins(0, 0, 0, 0);
        bar->setSpacing(6);

        bar->addWidget(new QLabel(QStringLiteral("MIDI In:"), this));
        inputCombo_ = new QComboBox(this);
        inputCombo_->setMinimumWidth(120);
        bar->addWidget(inputCombo_);
        inputButton_ = new QPushButton(QStringLiteral("Connect"), this);
        bar->addWidget(inputButton_);

        bar->addSpacing(6);
        bar->addWidget(new QLabel(QStringLiteral("MIDI Out:"), this));
        outputCombo_ = new QComboBox(this);
        outputCombo_->setMinimumWidth(120);
        bar->addWidget(outputCombo_);
        outputButton_ = new QPushButton(QStringLiteral("Connect"), this);
        bar->addWidget(outputButton_);

        auto *refreshButton = new QPushButton(QStringLiteral("Refresh devices"), this);
        bar->addWidget(refreshButton);

        recordButton_ = new QPushButton(QStringLiteral("Record MIDI"), this);
        recordButton_->setCheckable(true);
        recordButton_->setToolTip(
            QStringLiteral("Arm MIDI input and record notes into the selected track."));
        bar->addWidget(recordButton_);

        countInSpin_ = new QSpinBox(this);
        countInSpin_->setRange(0, 8);
        countInSpin_->setValue(1);
        countInSpin_->setSuffix(QStringLiteral(" bars"));
        countInSpin_->setToolTip(
            QStringLiteral("Count-in before recorded notes are captured (0 disables)."));
        bar->addWidget(new QLabel(QStringLiteral("Count-in"), this));
        bar->addWidget(countInSpin_);

        statusLabel_ = new QLabel(QStringLiteral("No MIDI devices connected"), this);
        theme::dimLabel(statusLabel_);
        bar->addWidget(statusLabel_, 1);

        connect(session_, &MidiSession::statusChanged, statusLabel_, &QLabel::setText);
        connect(session_, &MidiSession::outputError, this, [this](const QString &message) {
            QMessageBox::warning(this, QStringLiteral("MIDI Output"), message);
        });
        connect(inputButton_, &QPushButton::clicked, this, [this]() {
            session_->toggleInput(inputCombo_, inputButton_);
        });
        connect(outputButton_, &QPushButton::clicked, this, [this]() {
            session_->toggleOutput(outputCombo_, outputButton_);
        });
        connect(refreshButton, &QPushButton::clicked, this, [this]() {
            session_->refreshDevices(inputCombo_, outputCombo_);
        });
        connect(recordButton_, &QPushButton::toggled, this, [this](bool enabled) {
            recordButton_->setText(enabled ? QStringLiteral("Stop Recording")
                                           : QStringLiteral("Record MIDI"));
            emit recordToggled(enabled);
        });

        session_->refreshDevices(inputCombo_, outputCombo_);
    }

    int MidiStrip::countInBars() const
    {
        return countInSpin_->value();
    }

    void MidiStrip::cancelRecordArm()
    {
        const QSignalBlocker blocker(recordButton_);
        recordButton_->setChecked(false);
        recordButton_->setText(QStringLiteral("Record MIDI"));
    }

}
