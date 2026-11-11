#include "GrooveBoxPanel.hpp"

#include <QFormLayout>
#include <QHBoxLayout>

#include <QVBoxLayout>

#include <algorithm>

namespace aur
{
    namespace
    {
        const char *kArpModes[] = {"off", "up", "down", "updown",
                                   "random", "chord", "asplayed"};
        const char *kArpRates[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
    } // namespace

    GrooveBoxPanel::GrooveBoxPanel(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);

        auto *grooveRow = new QHBoxLayout();
        swing = new QSlider(Qt::Horizontal, this);
        swing->setRange(0, 100);
        swing->setToolTip("Swing: delays every second 16th note of the bar.");
        humanize = new QSlider(Qt::Horizontal, this);
        humanize->setRange(0, 100);
        humanize->setToolTip("Humanise: deterministic timing jitter on every step.");
        grooveSlider = new QSlider(Qt::Horizontal, this);
        grooveSlider->setRange(-50, 50);
        grooveSlider->setToolTip("Groove: master velocity offset for every step.");
        grooveRow->addWidget(new QLabel("Swing", this));
        grooveRow->addWidget(swing, 1);
        grooveRow->addWidget(new QLabel("Humanise", this));
        grooveRow->addWidget(humanize, 1);
        grooveRow->addWidget(new QLabel("Groove", this));
        grooveRow->addWidget(grooveSlider, 1);
        root->addLayout(grooveRow);

        auto *arpRow = new QHBoxLayout();
        arpMode = new QComboBox(this);
        for (const char *mode : kArpModes)
            arpMode->addItem(QString::fromLatin1(mode), QString::fromLatin1(mode));
        arpMode->setToolTip("Arpeggiator mode applied to the pattern's pitches.");
        arpRate = new QComboBox(this);
        for (const char *rate : kArpRates)
            arpRate->addItem(QString::fromLatin1(rate), QString::fromLatin1(rate));
        arpRate->setToolTip("Arpeggiator step rate.");
        arpGate = new QSpinBox(this);
        arpGate->setRange(1, 100);
        arpGate->setSuffix(" %");
        arpGate->setToolTip("Note length as a percent of the arpeggiator step.");
        arpOctave = new QSpinBox(this);
        arpOctave->setRange(0, 3);
        arpOctave->setToolTip("Extra octaves the arpeggiator sweeps.");
        arpRow->addWidget(new QLabel("Arp", this));
        arpRow->addWidget(arpMode);
        arpRow->addWidget(arpRate);
        arpRow->addWidget(new QLabel("Gate", this));
        arpRow->addWidget(arpGate);
        arpRow->addWidget(new QLabel("Octave", this));
        arpRow->addWidget(arpOctave);
        arpRow->addStretch();
        root->addLayout(arpRow);

        auto *patternRow = new QHBoxLayout();
        patternList = new QListWidget(this);
        patternList->setToolTip("Patterns in the song. Select one to chain it.");
        patternList->setMaximumHeight(120);
        chainList = new QListWidget(this);
        chainList->setToolTip("Chain order: the patterns play one after the other.");
        chainList->setMaximumHeight(120);
        auto *addButton = new QPushButton("Add Pattern", this);
        auto *removeButton = new QPushButton("Remove", this);
        auto *chainUpButton = new QPushButton("Chain ↑", this);
        auto *chainDownButton = new QPushButton("Chain ↓", this);
        playButton = new QPushButton("Play", this);
        playButton->setCheckable(true);
        statusLabel = new QLabel("No patterns", this);
        statusLabel->setStyleSheet("color: #aeb8c2;");
        patternRow->addWidget(patternList, 1);
        patternRow->addWidget(addButton);
        patternRow->addWidget(removeButton);
        patternRow->addWidget(chainList, 1);
        patternRow->addWidget(chainUpButton);
        patternRow->addWidget(chainDownButton);
        patternRow->addWidget(playButton);
        patternRow->addWidget(statusLabel);
        root->addLayout(patternRow);

        connect(swing, &QSlider::valueChanged, this, [this](int) { writeGroove(); });
        connect(humanize, &QSlider::valueChanged, this, [this](int) { writeGroove(); });
        connect(grooveSlider, &QSlider::valueChanged, this, [this](int) { writeGroove(); });
        connect(arpMode, &QComboBox::currentIndexChanged, this, [this](int) { writeArp(); });
        connect(arpRate, &QComboBox::currentIndexChanged, this, [this](int) { writeArp(); });
        connect(arpGate, &QSpinBox::valueChanged, this, [this](int) { writeArp(); });
        connect(arpOctave, &QSpinBox::valueChanged, this, [this](int) { writeArp(); });
        connect(addButton, &QPushButton::clicked, this, [this]() { addPattern(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removePattern(); });
        connect(chainUpButton, &QPushButton::clicked, this, [this]() { onChainChanged(); });
        connect(chainDownButton, &QPushButton::clicked, this, [this]() { onChainChanged(); });
        connect(playButton, &QPushButton::clicked, this, [this](bool on) { onTransport(on); });
        connect(patternList, &QListWidget::itemSelectionChanged, this,
                [this]() { addSelectedToChain(); });
    }

    void GrooveBoxPanel::setSong(const Json &value)
    {
        song = value;
        applySong(song);
        refreshPatterns();
    }

    void GrooveBoxPanel::setChangeCallback(std::function<void(const Json &)> callback)
    {
        onSongChanged = std::move(callback);
    }

    void GrooveBoxPanel::setTransportHook(std::function<void(bool)> hook)
    {
        onTransportRequested = std::move(hook);
    }

    void GrooveBoxPanel::applySong(const Json &value)
    {
        updating = true;
        const audio::GrooveSettings groove = audio::read_groove_settings(value);
        swing->setValue(groove.swing);
        humanize->setValue(groove.humanize);
        grooveSlider->setValue(groove.groove);
        const audio::ArpSettings arp = audio::read_arp_settings(value);
        const int modeIndex = arpMode->findData(QString::fromLatin1(audio::arp_mode_name(arp.mode)));
        arpMode->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        const int rateIndex = arpRate->findData(QString::fromLatin1(audio::arp_rate_name(arp.rate)));
        arpRate->setCurrentIndex(rateIndex >= 0 ? rateIndex : 0);
        arpGate->setValue(arp.gate);
        arpOctave->setValue(arp.octave);
        updating = false;
    }

    audio::ArpSettings GrooveBoxPanel::arpSettings() const
    {
        audio::ArpSettings settings;
        audio::arp_mode_from_name(arpMode->currentData().toString().toStdString(), settings.mode);
        audio::arp_rate_from_name(arpRate->currentData().toString().toStdString(), settings.rate);
        settings.gate = arpGate->value();
        settings.octave = arpOctave->value();
        return settings;
    }

    audio::GrooveSettings GrooveBoxPanel::grooveSettings() const
    {
        audio::GrooveSettings groove;
        groove.swing = swing->value();
        groove.humanize = humanize->value();
        groove.groove = grooveSlider->value();
        return groove;
    }

    void GrooveBoxPanel::writeGroove()
    {
        if (updating)
            return;
        audio::SongDocument::set_groove_settings(song, grooveSettings());
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::writeArp()
    {
        if (updating)
            return;
        audio::write_arp_settings(song, arpSettings());
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::writeChain()
    {
        std::vector<int> chain;
        for (int i = 0; i < chainList->count(); ++i)
        {
            QListWidgetItem *item = chainList->item(i);
            if (item)
                chain.push_back(item->data(Qt::UserRole).toInt());
        }
        audio::SongDocument::set_groove_chain(song, chain);
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::refreshPatterns()
    {
        patterns = audio::read_step_patterns(song);
        const int selected = patternList->currentRow();
        updating = true;
        patternList->clear();
        chainList->clear();
        for (size_t i = 0; i < patterns.size(); ++i)
        {
            QListWidgetItem *item = new QListWidgetItem(
                QString::fromStdString(patterns[i].name), patternList);
            item->setData(Qt::UserRole, static_cast<int>(i));
        }
        if (selected >= 0 && selected < patternList->count())
            patternList->setCurrentRow(selected);
        for (int index : audio::SongDocument::groove_chain(song))
        {
            if (index < 0 || index >= static_cast<int>(patterns.size()))
                continue;
            QListWidgetItem *item = new QListWidgetItem(
                QString::fromStdString(patterns[static_cast<size_t>(index)].name), chainList);
            item->setData(Qt::UserRole, index);
        }
        updating = false;
        statusLabel->setText(patterns.empty()
                                 ? QStringLiteral("No patterns")
                                 : QString("%1 pattern(s)").arg(patterns.size()));
    }

    void GrooveBoxPanel::addSelectedToChain()
    {
        if (updating)
            return;
        const int row = patternList->currentRow();
        if (row < 0)
            return;
        const auto *item = patternList->item(row);
        if (!item)
            return;
        const int index = item->data(Qt::UserRole).toInt();
        auto chain = audio::SongDocument::groove_chain(song);
        if (std::find(chain.begin(), chain.end(), index) != chain.end())
            return;
        chain.push_back(index);
        audio::SongDocument::set_groove_chain(song, chain);
        refreshPatterns();
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::addPattern()
    {
        const int index = audio::SongDocument::add_step_pattern(song, "");
        if (index < 0)
            return;
        refreshPatterns();
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::removePattern()
    {
        const int row = patternList->currentRow();
        if (row < 0)
            return;
        audio::SongDocument::remove_step_pattern(song, row);
        refreshPatterns();
        if (onSongChanged)
            onSongChanged(song);
        emit historyChanged();
    }

    void GrooveBoxPanel::onChainChanged()
    {
        writeChain();
    }

    void GrooveBoxPanel::moveChain(int delta)
    {
        const int row = chainList->currentRow();
        if (row < 0 || row + delta < 0 || row + delta >= chainList->count())
            return;
        QListWidgetItem *item = chainList->takeItem(row);
        chainList->insertItem(row + delta, item);
        chainList->setCurrentRow(row + delta);
        writeChain();
    }

    void GrooveBoxPanel::onTransport(bool play)
    {
        if (onTransportRequested)
            onTransportRequested(play);
    }

} // namespace aur
