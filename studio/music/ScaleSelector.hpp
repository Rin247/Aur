#pragma once

#include <QWidget>
#include <QComboBox>
#include <functional>
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

namespace aur
{

    // Key/scale selector for the piano roll's scale highlight. Writes
    // song-level "key_root"/"key_scale" keys through a change callback;
    // -1 scale = no highlight. Lives outside PianoRollCanvas so the
    // canvas stays under the size ratchet.
    class ScaleSelector : public QWidget
    {
        Q_OBJECT
    public:
        explicit ScaleSelector(QWidget *parent = nullptr);

        void setKey(int root, int scaleIndex); // root 0..11, scale -1..N
        int root() const;
        int scale() const;
        void setChangeCallback(std::function<void(int root, int scale)> callback);

    private:
        void emitChange();

        QComboBox *rootBox = nullptr;
        QComboBox *scaleBox = nullptr;
        std::function<void(int, int)> onChange;
    };

} // namespace aur
