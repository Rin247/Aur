#include "FxChainStrip.hpp"

#include "aur/audio/Effect.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace aur
{

    FxChainStrip::FxChainStrip(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        auto *bar = new QHBoxLayout();
        typeCombo = new QComboBox(this);
        for (int i = 0; i < int(audio::EffectId::Count); ++i)
            typeCombo->addItem(audio::effectName(audio::EffectId(i)));
        typeCombo->setToolTip("Effect to append to the master chain (applies to preview and export).");
        addButton = new QPushButton("Add", this);
        bypassButton = new QPushButton("Bypass", this);
        bypassButton->setEnabled(false);
        bypassButton->setToolTip("Toggle bypass on the selected chain slot (no DSP, bit-exact).");
        upButton = new QPushButton("Up", this);
        upButton->setEnabled(false);
        downButton = new QPushButton("Down", this);
        downButton->setEnabled(false);
        removeButton = new QPushButton("Remove", this);
        removeButton->setEnabled(false);
        statusLabel = new QLabel("No master effects", this);
        statusLabel->setStyleSheet("color: #aeb8c2;");
        bar->addWidget(new QLabel("Master FX:", this));
        bar->addWidget(typeCombo);
        bar->addWidget(addButton);
        bar->addWidget(bypassButton);
        bar->addWidget(upButton);
        bar->addWidget(downButton);
        bar->addWidget(removeButton);
        bar->addWidget(statusLabel, 1);
        root->addLayout(bar);

        chainList = new QListWidget(this);
        chainList->setMaximumHeight(88);
        chainList->setToolTip("Master effect chain, top processes first. Select a slot to modify it.");
        root->addWidget(chainList);

        connect(addButton, &QPushButton::clicked, this, [this]() {
            auto chain = currentChain();
            audio::SongDocument::EffectSlot slot;
            slot.type = typeCombo->currentText().toStdString();
            chain.push_back(slot);
            writeChain(chain);
            refresh();
        });
        connect(removeButton, &QPushButton::clicked, this, [this]() {
            const int row = chainList->currentRow();
            if (row < 0) return;
            auto chain = currentChain();
            if (row >= int(chain.size())) return;
            chain.erase(chain.begin() + row);
            writeChain(chain);
            refresh();
        });
        connect(bypassButton, &QPushButton::clicked, this, [this]() {
            const int row = chainList->currentRow();
            if (row < 0) return;
            auto chain = currentChain();
            if (row >= int(chain.size())) return;
            chain[size_t(row)].bypass = !chain[size_t(row)].bypass;
            writeChain(chain);
            refresh();
        });
        auto moveSlot = [this](int delta) {
            const int row = chainList->currentRow();
            if (row < 0) return;
            const int target = row + delta;
            auto chain = currentChain();
            if (target < 0 || target >= int(chain.size())) return;
            std::swap(chain[size_t(row)], chain[size_t(target)]);
            writeChain(chain);
            refresh();
            chainList->setCurrentRow(target);
        };
        connect(upButton, &QPushButton::clicked, this, [moveSlot]() { moveSlot(-1); });
        connect(downButton, &QPushButton::clicked, this, [moveSlot]() { moveSlot(1); });
        connect(chainList, &QListWidget::itemSelectionChanged, this, [this]() {
            const int row = chainList->currentRow();
            const bool has = row >= 0;
            bypassButton->setEnabled(has);
            removeButton->setEnabled(has);
            upButton->setEnabled(has && row > 0);
            downButton->setEnabled(has && row < chainList->count() - 1);
        });
    }

    void FxChainStrip::setSong(const Json *value)
    {
        hasSong = value != nullptr;
        song = value ? *value : Json{};
        refresh();
    }

    std::vector<audio::SongDocument::EffectSlot> FxChainStrip::currentChain() const
    {
        if (!hasSong)
            return {};
        return audio::SongDocument::effect_chain(song);
    }

    void FxChainStrip::writeChain(const std::vector<audio::SongDocument::EffectSlot> &chain)
    {
        if (!hasSong)
            return;
        audio::SongDocument::set_effect_chain(song, chain);
        emit chainChanged();
    }

    void FxChainStrip::refresh()
    {
        const int selected = chainList->currentRow();
        chainList->clear();
        const auto chain = currentChain();
        for (const auto &slot : chain)
        {
            QString text = QString::fromStdString(slot.type);
            if (slot.bypass) text += " (bypassed)";
            chainList->addItem(text);
        }
        if (chain.empty())
            statusLabel->setText("No master effects");
        else
            statusLabel->setText(QString("%1 effect(s) in the master chain").arg(int(chain.size())));
        chainList->setCurrentRow(std::min<int>(selected, int(chain.size()) - 1));
    }

    std::vector<audio::SongDocument::EffectSlot> FxChainStrip::chain() const
    {
        return currentChain();
    }

    void FxChainStrip::updateMeters(double chainLeft, double chainRight, long long budget)
    {
        statusLabel->setText(QString("post-FX L %1% R %2% | chain cost %3")
                                 .arg(int(chainLeft * 100))
                                 .arg(int(chainRight * 100))
                                 .arg(budget));
    }

} // namespace aur
