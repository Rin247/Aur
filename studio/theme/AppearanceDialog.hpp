#pragma once

#include <QDialog>

class QButtonGroup;
class QLineEdit;

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
    };

}
