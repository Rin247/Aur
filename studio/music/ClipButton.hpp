#pragma once

#include <QPushButton>

namespace aur
{

    class ClipButton : public QPushButton
    {
    public:
        enum class State
        {
            Empty,
            Clip,
            Queued,
            Playing
        };

        explicit ClipButton(const QString &text, QWidget *parent = nullptr);
        void setState(State state);

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        State state_ = State::Empty;
    };

}
