#pragma once

#include <QWidget>

namespace aur::theme
{

    class CardFrame : public QWidget
    {
    public:
        explicit CardFrame(QWidget *parent = nullptr);

    protected:
        void paintEvent(QPaintEvent *event) override;
    };

}
