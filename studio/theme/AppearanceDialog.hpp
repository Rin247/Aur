#pragma once

#include <QDialog>
#include <array>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace aur::theme
{

    class AppearanceDialog final : public QDialog
    {
        Q_OBJECT
    public:
        explicit AppearanceDialog(QWidget *parent = nullptr);

    private:
        void syncFromTheme();

        QButtonGroup *modeGroup_ = nullptr;
        QLineEdit *hexEdit_ = nullptr;
        QWidget *matrix_ = nullptr;
        QSlider *scaleSlider_ = nullptr;
        QLabel *scaleLabel_ = nullptr;
        QCheckBox *dyslexiaCheck_ = nullptr;
        QComboBox *colorBlindCombo_ = nullptr;
        QComboBox *graphicsBackendCombo_ = nullptr;
        std::array<QLabel *, 3> swatches_{};
    };

}
