#include "ScaleSelector.hpp"

#include <QHBoxLayout>
#include <QLabel>

#include <algorithm>

namespace aur
{
    namespace
    {
        const char *kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F",
                                      "F#", "G", "G#", "A", "A#", "B"};
    } // namespace

    ScaleSelector::ScaleSelector(QWidget *parent) : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        layout->addWidget(new QLabel("Key", this));
        rootBox = new QComboBox(this);
        for (int i = 0; i < 12; ++i)
            rootBox->addItem(kNoteNames[i], i);
        layout->addWidget(rootBox);
        layout->addWidget(new QLabel("Scale", this));
        scaleBox = new QComboBox(this);
        scaleBox->addItem("—", -1);
        int index = 0;
        for (const audio::SongDocument::ScaleDef &def : audio::SongDocument::scales())
        {
            scaleBox->addItem(def.name, index);
            ++index;
        }
        layout->addWidget(scaleBox);
        layout->addStretch(1);

        connect(rootBox, &QComboBox::currentIndexChanged, this, [this]()
                { emitChange(); });
        connect(scaleBox, &QComboBox::currentIndexChanged, this, [this]()
                { emitChange(); });
    }

    void ScaleSelector::setKey(int root, int scaleIndex)
    {
        const QSignalBlocker rootBlocker(rootBox);
        const QSignalBlocker scaleBlocker(scaleBox);
        const int rootIndex = rootBox->findData(root < 0 ? 0 : std::clamp(root, 0, 11));
        rootBox->setCurrentIndex(rootIndex >= 0 ? rootIndex : 0);
        const int scaleComboIndex = scaleBox->findData(scaleIndex);
        scaleBox->setCurrentIndex(scaleComboIndex >= 0 ? scaleComboIndex : 0);
    }

    int ScaleSelector::root() const
    {
        return rootBox->currentData().isValid() ? rootBox->currentData().toInt() : 0;
    }

    int ScaleSelector::scale() const
    {
        return scaleBox->currentData().isValid() ? scaleBox->currentData().toInt() : -1;
    }

    void ScaleSelector::setChangeCallback(std::function<void(int, int)> callback)
    {
        onChange = std::move(callback);
    }

    void ScaleSelector::emitChange()
    {
        if (onChange)
            onChange(root(), scale());
    }

} // namespace aur
