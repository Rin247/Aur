#include "MusicPanel.hpp"
#include "music/SharedAudio.hpp"
#include "music/GmPatchList.hpp"
#include "music/MasterVolumeStrip.hpp"
#include "music/SongLibrary.hpp"
#include "music/TransportIcon.hpp"
#include "theme/LabelRoles.hpp"
#include "theme/ResponsiveSplitter.hpp"
#include "theme/ThemeIcons.hpp"

#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>

namespace aur
{

    int MusicPanel::clampedInt(const Json &value, int fallback, int low, int high) const
    {
        return value.is_number() ? std::clamp(value.as_int(), low, high) : fallback;
    }

    Json *MusicPanel::currentSong()
    {
        if (!project || currentSongId.isEmpty())
            return nullptr;
        auto it = project->music.find(currentSongId.toStdString());
        return it == project->music.end() ? nullptr : &it->second;
    }

    const Json *MusicPanel::currentSong() const
    {
        return const_cast<MusicPanel *>(this)->currentSong();
    }

    void MusicPanel::syncScaleSelector(const Json &song)
    {
        const int root = song.contains("key_root") && song["key_root"].is_number()
                             ? song["key_root"].as_int() : 0;
        const int scale = song.contains("key_scale") && song["key_scale"].is_number()
                              ? song["key_scale"].as_int() : -1;
        scaleSelector->setKey(root, scale);
    }

    void MusicPanel::saveCurrentSong(const Json &song)
    {
        if (!project || currentSongId.isEmpty())
            return;
        const bool structural = song.contains("tracks") && song["tracks"].is_array() &&
                                selectedTrack >= 0 &&
                                selectedTrack < static_cast<int>(song["tracks"].as_array().size());
        project->music[currentSongId.toStdString()] = song;
        if (structural)
        {
            const Json &track = song["tracks"][static_cast<size_t>(selectedTrack)];
            const QString name = track.contains("name")
                                     ? QString::fromStdString(track["name"].as_string())
                                     : QString("Track %1").arg(selectedTrack + 1);
            const int notes = track.contains("notes") && track["notes"].is_array()
                                  ? static_cast<int>(track["notes"].as_array().size())
                                  : 0;
            if (QListWidgetItem *item = trackList->item(selectedTrack))
                item->setText(QString("%1  ·  %2 notes").arg(name).arg(notes));
        }
    }

    bool MusicPanel::undoSongEdit()
    {
        Json *song = currentSong();
        if (!song || !pianoRoll || !pianoRoll->canUndo())
            return false;
        pianoRoll->undo();
        commitSongEdit(*song);
        return true;
    }

    bool MusicPanel::redoSongEdit()
    {
        Json *song = currentSong();
        if (!song || !pianoRoll || !pianoRoll->canRedo())
            return false;
        pianoRoll->redo();
        commitSongEdit(*song);
        return true;
    }

    void MusicPanel::commitSongEdit(const Json &song)
    {
        if (project && !currentSongId.isEmpty())
            project->music[currentSongId.toStdString()] = song;
        updateTrackProperties();
        refreshTracks();
    }

    void MusicPanel::setProject(Project *p)
    {
        project = p;
        currentSongId.clear();
        refreshSongs();
    }

    void MusicPanel::refreshSongs()
    {
        const QSignalBlocker blocker(songCombo);
        songCombo->clear();
        if (project)
        {
            for (const auto &[id, song] : project->music)
            {
                const QString qid = QString::fromStdString(id);
                const QString name = song.contains("name")
                                         ? QString::fromStdString(song["name"].as_string())
                                         : qid;
                songCombo->addItem(name + " (" + qid + ")", qid);
            }
        }
        if (!currentSongId.isEmpty() && songCombo->findData(currentSongId) >= 0)
            songCombo->setCurrentIndex(songCombo->findData(currentSongId));
        else if (songCombo->count() > 0)
        {
            songCombo->setCurrentIndex(0);
            currentSongId = songCombo->itemData(0).toString();
            selectSong(currentSongId);
        }
        else
        {
            currentSongId.clear();
            selectedTrack = -1;
            selectedScene = -1;
            activeScene = -1;
            trackList->clear();
            pianoRoll->setSong(Json{}, -1); velocityStrip->setSong(Json{}, -1);
            arrangementCanvas->setSong(Json{});
            refreshSessionGrid();
            songName->clear();
            tempo->setValue(120);
            previewSynth->setCurrentIndex(0);
        }
    }

    void MusicPanel::selectSong(const QString &id)
    {
        currentSongId = id;
        midiSession->setRecordTarget(id);
        Json *song = currentSong();
        if (!song)
            return;
        selectedScene = 0;
        activeScene = -1;
        updatingControls = true;
        songName->setText((*song).contains("name")
                              ? QString::fromStdString((*song)["name"].as_string())
                              : id);
        tempo->setValue(clampedInt((*song)["tempo"], 120, 30, 240));
        const QString synthId = (*song)["preview_synth"].is_string()
                                    ? QString::fromStdString((*song)["preview_synth"].as_string())
                                    : "md_fm";
        // A retired oscillator id ("gm", "fm", "retro_psg") is not in the list
        // any more, so the song falls back to the first core.
        const int synthIndex = previewSynth->findData(synthId);
        previewSynth->setCurrentIndex(synthIndex >= 0 ? synthIndex : 0);
        selectedTrack = 0;
        refreshTracks();
        refreshFxChain();
        trackList->setCurrentRow(selectedTrack);
        pianoRoll->setSong(*song, selectedTrack); velocityStrip->setSong(*song, selectedTrack);
        arrangementCanvas->setSong(*song);
        stepCanvas->setSong(*song);
        stepCanvas->setSourceTrack(selectedTrack);
        groovePanel->setSong(*song);
    syncScaleSelector(*song);
        updateTrackProperties();
        updatingControls = false;
    }

    void MusicPanel::selectPageTab(const QString &name)
    {
        if (!editorTabs)
            return;
        for (int index = 0; index < editorTabs->count(); ++index)
        {
            if (editorTabs->tabText(index) == name)
            {
                editorTabs->setCurrentIndex(index);
                return;
            }
        }
    }

    void MusicPanel::refreshTracks()
    {
        const QSignalBlocker blocker(trackList);
        trackList->clear();
        Json *song = currentSong();
        if (!song || !(*song).contains("tracks") || !(*song)["tracks"].is_array())
        {
            selectedTrack = -1;
            pianoRoll->setSong(song ? *song : Json{}, -1); velocityStrip->setSong(song ? *song : Json{}, -1);
            arrangementCanvas->setSong(song ? *song : Json{});
            refreshSessionGrid();
            return;
        }
        const JsonArray &tracks = (*song)["tracks"].as_array();
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            const Json &track = tracks[i];
            const QString name = track.contains("name")
                                     ? QString::fromStdString(track["name"].as_string())
                                     : QString("Track %1").arg(i + 1);
            const int noteCount = track.contains("notes") && track["notes"].is_array()
                                      ? static_cast<int>(track["notes"].as_array().size())
                                      : 0;
            trackList->addItem(QString("%1  ·  %2 notes").arg(name).arg(noteCount));
        }
        selectedTrack = tracks.empty() ? -1 : std::clamp(selectedTrack, 0, static_cast<int>(tracks.size()) - 1);
        midiSession->setSelectedTrack(selectedTrack);
        refreshSessionGrid();
    }

    void MusicPanel::updateTrackProperties()
    {
        Json *song = currentSong();
        const bool valid = song && selectedTrack >= 0 && (*song).contains("tracks") &&
                           selectedTrack < static_cast<int>((*song)["tracks"].as_array().size());
        updatingControls = true;
        instrument->setEnabled(valid);
        bank->setEnabled(valid);
        channel->setEnabled(valid);
        volume->setEnabled(valid);
        pan->setEnabled(valid);
        if (valid)
        {
            const Json &track = (*song)["tracks"][static_cast<size_t>(selectedTrack)];
            instrument->setCurrentIndex(instrument->findData(
                clampedInt(track["program"], 0, 0, 127)));
            bank->setValue(clampedInt(track["bank"], 0, 0, 16383));
            channel->setValue(clampedInt(track["channel"], selectedTrack % 16, 0, 15) + 1);
            volume->setValue(clampedInt(track["volume"], 90, 0, 127));
            pan->setValue(clampedInt(track["pan"], 64, 0, 127));
        }
        updatingControls = false;
    }

    void MusicPanel::beginTrackEdit()
    {
        Json *song = currentSong();
        if (!song || selectedTrack < 0 ||
            selectedTrack >= static_cast<int>((*song)["tracks"].as_array().size()))
            return;
        songHistory.begin_track_edit(*song, selectedTrack);
    }

    void MusicPanel::updateTrackProperty(const char *key, int value)
    {
        if (updatingControls)
            return;
        Json *song = currentSong();
        if (!song || selectedTrack < 0 ||
            selectedTrack >= static_cast<int>((*song)["tracks"].as_array().size()))
            return;
        songHistory.begin_track_edit(*song, selectedTrack);
        (*song)["tracks"][static_cast<size_t>(selectedTrack)][key] = value;
        songHistory.commit_track_change(*song, selectedTrack,
                                        editor::SongAction::Kind::TrackProps,
                                        std::string("set ") + key);
        project->music[currentSongId.toStdString()] = *song;
        refreshTracks();
    }

    void MusicPanel::generateQuickSong(const std::string &style)
    {
        if (!project)
            return;
        Json song = music::quickSong(style);
        const std::string base = music::songIdFromName(
            QString::fromStdString(style + " Theme"));
        std::string id = base;
        int suffix = 2;
        while (project->music.contains(id))
            id = base + "_" + std::to_string(suffix++);
        project->music[id] = std::move(song);
        refreshSongs();
        selectSong(QString::fromStdString(id));
    }

MusicPanel::MusicPanel(Project* p, SharedAudio* sharedAudio, QWidget* parent)
    : QWidget(parent), project(p) {
    // The DAW and the playtest share one device, so the preview cannot be
    // played over the game.
    shared = sharedAudio ? sharedAudio : new SharedAudio(this);
    audioOutput = &shared->output();
    midiSession = new MidiSession(this);
    midiSession->attachClock(&audioOutput->getMixer().playheadFrames,
                             &audioOutput->getMixer().playheadBpm,
                             &audioOutput->getMixer().playheadActive);
    connect(midiSession, &MidiSession::noteRecorded, this,
            [this](const QString& songId, int trackIndex, int startStep,
                   int pitch, int length, int velocity) {
        if (!project || !project->music.contains(songId.toStdString())) return;
        Json& song = project->music[songId.toStdString()];
        if (!song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex < 0 ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size())) return;
        Json& track = song["tracks"][static_cast<size_t>(trackIndex)];
        if (!track.contains("notes") || !track["notes"].is_array()) {
            track["notes"] = JsonArray{};
        }
        track["notes"].as_array().emplace_back(
            JsonArray{startStep, pitch, length, velocity});
        std::stable_sort(track["notes"].as_array().begin(), track["notes"].as_array().end(),
            [](const Json& left, const Json& right) {
                return left.is_array() && right.is_array() &&
                       left[0].as_int() < right[0].as_int();
            });
        // Scene record: mirror the note into the armed scene column's clip
        // for this track, clipped to the clip's 16-step window.
        if (recordingScene >= 0)
        {
            music::initializeScenes(song);
            JsonArray& scenes = song["scenes"].as_array();
            if (recordingScene < static_cast<int>(scenes.size()))
            {
                JsonArray& clips = scenes[static_cast<size_t>(recordingScene)]["clips"].as_array();
                clips.resize(song["tracks"].as_array().size());
                Json& clip = clips[static_cast<size_t>(trackIndex)];
                if (clip.is_null())
                    clip = JsonObject{{"notes", JsonArray{}}};
                const int clipStep = std::clamp(startStep % 16, 0, 15);
                if (!clip.contains("notes") || !clip["notes"].is_array())
                    clip["notes"] = JsonArray{};
                clip["notes"].as_array().emplace_back(
                    JsonArray{clipStep, pitch, length, velocity});
            }
        }
        if (songId == currentSongId) {
            refreshTracks();
            pianoRoll->setSong(song, selectedTrack); velocityStrip->setSong(song, selectedTrack);
            arrangementCanvas->setSong(song);
            refreshSessionGrid();
        }
    });


    auto* layout = new QVBoxLayout(this);
    auto* songBar = new QHBoxLayout();
    songBar->addWidget(new QLabel("Song:", this));
    songCombo = new QComboBox(this);
    songCombo->setMinimumWidth(150);
    songBar->addWidget(songCombo);
    auto* newSongButton = new QPushButton("New", this);
    auto* deleteSongButton = new QPushButton("Delete", this);
    songBar->addWidget(newSongButton);
    songBar->addWidget(deleteSongButton);
    songBar->addSpacing(12);
    songBar->addWidget(new QLabel("Title:", this));
    songName = new QLineEdit(this);
    songName->setMinimumWidth(80);
    songName->setMaximumWidth(200);
    songBar->addWidget(songName);
    songBar->addWidget(new QLabel("Tempo:", this));
    tempo = new QSpinBox(this);
    tempo->setRange(30, 240);
    tempo->setSuffix(" BPM");
    songBar->addWidget(tempo);
    previewSynth = new QComboBox(this);
    previewSynth->addItem("Mega Drive FM", "md_fm");
    previewSynth->addItem("SPC sampler", "spc");
    previewSynth->setToolTip(
        "The preview instrument. MIDI output sends the selected bank and program to a connected GM/GS device.");
    songBar->addWidget(new QLabel("Preview:", this));
    songBar->addWidget(previewSynth);
    // The transport is icons: the style's own, so there is nothing to draw.
    playButton = new QPushButton(this);
    playButton->setIcon(music::transportIcon(music::TransportGlyph::Play));
    playButton->setToolTip("Play the song");
    auto* stopButton = new QPushButton(this);
    stopButton->setIcon(music::transportIcon(music::TransportGlyph::Stop));
    stopButton->setToolTip("Stop and rewind");
    auto* importMidiButton = new QPushButton(this);
    importMidiButton->setIcon(theme::icon(theme::Glyph::Open));
    importMidiButton->setToolTip("Import MIDI…");
    auto* exportMidiButton = new QPushButton(this);
    exportMidiButton->setIcon(theme::icon(theme::Glyph::Save));
    exportMidiButton->setToolTip("Export MIDI…");
    songBar->addWidget(playButton);
    songBar->addWidget(stopButton);
    songBar->addWidget(importMidiButton);
    songBar->addWidget(exportMidiButton);
    songBar->addSpacing(8);
    songBar->addWidget(new MasterVolumeStrip(shared, QStringLiteral("Master:"), this));
    songBar->addStretch();
    layout->addLayout(songBar);

    midiStrip = new MidiStrip(midiSession, this);
    layout->addWidget(midiStrip);

    auto* fxAndPresetRow = new QHBoxLayout();
    fxStrip = new FxChainStrip(this);
    connect(fxStrip, &FxChainStrip::chainChanged, this, [this]() {
        Json *song = currentSong();
        if (!song) return;
        audio::SongDocument::set_effect_chain(*song, fxStrip->chain());
        saveCurrentSong(*song);
    });
    fxAndPresetRow->addWidget(fxStrip, 1);
    fxAndPresetRow->addSpacing(8);
    fxAndPresetRow->addWidget(new QLabel("Quick Compose:", this));
    auto* overworldButton = new QPushButton("Overworld", this);
    auto* battleButton = new QPushButton("Battle", this);
    auto* villageButton = new QPushButton("Village", this);
    fxAndPresetRow->addWidget(overworldButton);
    fxAndPresetRow->addWidget(battleButton);
    fxAndPresetRow->addWidget(villageButton);
    layout->addLayout(fxAndPresetRow);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* trackPane = new QWidget(splitter);
    auto* trackLayout = new QVBoxLayout(trackPane);
    trackLayout->setContentsMargins(0, 0, 8, 0);
    trackLayout->addWidget(new QLabel("Tracks — select to edit notes", trackPane));
    trackList = new QListWidget(trackPane);
    trackList->setMinimumWidth(220);
    trackLayout->addWidget(trackList, 1);
    auto* trackButtons = new QHBoxLayout();
    auto* addTrackButton = new QPushButton("Add Track", trackPane);
    auto* addDrumTrackButton = new QPushButton("Add Drum Track", trackPane);
    auto* removeTrackButton = new QPushButton("Remove", trackPane);
    trackButtons->addWidget(addTrackButton);
    trackButtons->addWidget(removeTrackButton);
    trackLayout->addLayout(trackButtons);
    trackLayout->addWidget(addDrumTrackButton);

    auto* propertyForm = new QFormLayout();
    propertyForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    instrument = new QComboBox(trackPane);
    instrument->setEditable(true);
    instrument->setInsertPolicy(QComboBox::NoInsert);
    instrument->setMinimumContentsLength(25);
    instrument->setMaxVisibleItems(24);
    instrument->setToolTip("GM program number and instrument name. Type to filter the 128 General MIDI patches.");
    for (int program = 0; program < static_cast<int>(music::gmInstrumentNames().size()); ++program) {
        instrument->addItem(
            QString("%1  %2").arg(program + 1, 3, 10, QChar('0'))
                .arg(QString::fromLatin1(music::gmInstrumentNames()[static_cast<size_t>(program)])),
            program);
    }
    if (QCompleter* completer = instrument->completer()) {
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
    }
    bank = new QSpinBox(trackPane);
    bank->setRange(0, 16383);
    bank->setToolTip("MIDI bank number (MSB × 128 + LSB), used by GM/GS sound modules.");
    channel = new QSpinBox(trackPane);
    channel->setRange(1, 16);
    volume = new QSpinBox(trackPane);
    volume->setRange(0, 127);
    pan = new QSpinBox(trackPane);
    pan->setRange(0, 127);
    propertyForm->addRow("GM instrument:", instrument);
    propertyForm->addRow("GS bank:", bank);
    propertyForm->addRow("MIDI channel:", channel);
    propertyForm->addRow("Volume:", volume);
    propertyForm->addRow("Pan:", pan);
    trackLayout->addLayout(propertyForm);
    splitter->addWidget(trackPane);

    buildEditorTabs(splitter);
    theme::bindResponsiveSplitter(splitter, {22, 78});
    layout->addWidget(splitter, 1);

    pianoRoll->setChangeCallback([this](const Json& song) { saveCurrentSong(song); });
    pianoRoll->setHistory(&songHistory);
    pianoRoll->attachPlayhead(&audioOutput->getMixer().playheadFrames,
                              &audioOutput->getMixer().playheadBpm,
                              &audioOutput->getMixer().playheadActive);
    pianoRoll->setAuditionHook([this](int pitch, int velocity) {
        audioOutput->getMixer().audition(pitch, velocity, 2);
    });
    connect(pianoRoll, &PianoRollCanvasWidget::historyChanged, this, [this]() {
        emit undoStateChanged(pianoRoll && pianoRoll->canUndo(),
                              pianoRoll && pianoRoll->canRedo());
    });
    connect(songCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) selectSong(songCombo->itemData(index).toString());
    });
    connect(newSongButton, &QPushButton::clicked, this, [this]() {
        if (!project) return;
        bool accepted = false;
        const QString name = QInputDialog::getText(this, "New Song", "Song name:",
                                                    QLineEdit::Normal, "New Song", &accepted);
        if (!accepted || name.trimmed().isEmpty()) return;
        const std::string base = music::songIdFromName(name);
        std::string id = base;
        int suffix = 2;
        while (project->music.contains(id)) id = base + "_" + std::to_string(suffix++);
        project->music[id] = JsonObject{
            {"name", name.trimmed().toStdString()}, {"tempo", 120}, {"bars", 8},
            {"beats", 4}, {"loop_bar", 0}, {"preview_synth", "md_fm"},
            {"tracks", JsonArray{}}
        };
        refreshSongs();
        selectSong(QString::fromStdString(id));
    });
    connect(deleteSongButton, &QPushButton::clicked, this, [this]() {
        if (!project || currentSongId.isEmpty()) return;
        const auto answer = QMessageBox::question(this, "Delete Song",
            "Delete this song from the project? Save Project to persist the change.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        project->music.erase(currentSongId.toStdString());
        currentSongId.clear();
        refreshSongs();
    });
    connect(songName, &QLineEdit::textEdited, this, [this](const QString& name) {
        Json* song = currentSong();
        if (song) {
            (*song)["name"] = name.toStdString();
            const QSignalBlocker blocker(songCombo);
            const int index = songCombo->findData(currentSongId);
            if (index >= 0) songCombo->setItemText(index, name.trimmed().isEmpty()
                ? currentSongId : name.trimmed() + " (" + currentSongId + ")");
        }
    });
    connect(tempo, &QSpinBox::valueChanged, this, [this](int value) {
        Json* song = currentSong();
        if (song) (*song)["tempo"] = value;
    });
    connect(previewSynth, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updatingControls || index < 0) return;
        if (Json* song = currentSong()) {
            (*song)["preview_synth"] = previewSynth->itemData(index).toString().toStdString();
        }
    });
    connect(trackList, &QListWidget::currentRowChanged, this, [this](int row) {
        selectedTrack = row;
        midiSession->setSelectedTrack(row);
        if (Json* song = currentSong()) {
            pianoRoll->setSong(*song, selectedTrack); velocityStrip->setSong(*song, selectedTrack);
        arrangementCanvas->setSong(*song);
        stepCanvas->setSourceTrack(selectedTrack);
    syncScaleSelector(*song);
            updateTrackProperties();
        }
        refreshSessionGrid();
    });
    connect(trackList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        Json* song = currentSong();
        if (!song || selectedTrack < 0 || !song->contains("tracks") ||
            selectedTrack >= static_cast<int>((*song)["tracks"].as_array().size())) return;
        bool accepted = false;
        const QString name = QInputDialog::getText(this, "Rename Track", "Track name:",
            QLineEdit::Normal, item->text(), &accepted);
        if (!accepted || name.trimmed().isEmpty()) return;
        (*song)["tracks"][static_cast<size_t>(selectedTrack)]["name"] = name.trimmed().toStdString();
        item->setText(name.trimmed());
    });
    connect(addTrackButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song) return;
        JsonArray& tracks = (*song)["tracks"].as_array();
        tracks.push_back(music::makeTrack("Track " + std::to_string(tracks.size() + 1),
                                          static_cast<int>(tracks.size() % 9),
                                          80 + static_cast<int>(tracks.size() % 8)));
        selectedTrack = static_cast<int>(tracks.size()) - 1;
        refreshTracks();
        trackList->setCurrentRow(selectedTrack);
    });
    connect(addDrumTrackButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song) return;
        JsonArray& tracks = (*song)["tracks"].as_array();
        tracks.push_back(music::makeTrack("Drums", 9, 0));
        selectedTrack = static_cast<int>(tracks.size()) - 1;
        refreshTracks();
        trackList->setCurrentRow(selectedTrack);
    });
    connect(removeTrackButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song || selectedTrack < 0 ||
            selectedTrack >= static_cast<int>((*song)["tracks"].as_array().size())) return;
        JsonArray& tracks = (*song)["tracks"].as_array();
        tracks.erase(tracks.begin() + selectedTrack);
        selectedTrack = std::min(selectedTrack, static_cast<int>(tracks.size()) - 1);
        refreshTracks();
        trackList->setCurrentRow(selectedTrack);
        pianoRoll->setSong(*song, selectedTrack); velocityStrip->setSong(*song, selectedTrack);
        arrangementCanvas->setSong(*song);
    syncScaleSelector(*song);
        refreshSessionGrid();
    });
    connect(instrument, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                if (index >= 0) updateTrackProperty("program", instrument->itemData(index).toInt());
            });
    connect(bank, &QSpinBox::valueChanged, this,
            [this](int value) { updateTrackProperty("bank", value); });
    connect(channel, &QSpinBox::valueChanged, this,
            [this](int value) { updateTrackProperty("channel", value - 1); });
    connect(volume, &QSpinBox::valueChanged, this,
            [this](int value) { updateTrackProperty("volume", value); });
    connect(pan, &QSpinBox::valueChanged, this,
            [this](int value) { updateTrackProperty("pan", value); });
    connect(overworldButton, &QPushButton::clicked, this,
            [this]() { generateQuickSong("Overworld"); });
    connect(battleButton, &QPushButton::clicked, this,
            [this]() { generateQuickSong("Battle"); });
    connect(villageButton, &QPushButton::clicked, this,
            [this]() { generateQuickSong("Village"); });
    connect(playButton, &QPushButton::clicked, this, [this]() {
        if (transport == Transport::Playing)
            pausePreview();
        else if (transport == Transport::Paused)
            resumePreview();
        else
            playPreview();
    });
    connect(stopButton, &QPushButton::clicked, this, [this]() { stopPreview(); });
    connect(midiStrip, &MidiStrip::recordToggled, this, [this](bool enabled) {
        if (enabled && (!midiSession->devices().inputConnected() || !currentSong() ||
                        selectedTrack < 0)) {
            midiStrip->cancelRecordArm();
            QMessageBox::information(this, "MIDI Recording",
                "Connect a MIDI input, select a song, and select a track before recording.");
            return;
        }
        recordingScene = enabled ? selectedScene : -1;
        midiSession->setSelectedTrack(selectedTrack);
        midiSession->setRecordTarget(currentSongId);
        midiSession->setCountInBars(midiStrip->countInBars());
        midiSession->setRecording(enabled);
    });
    connect(importMidiButton, &QPushButton::clicked, this, [this]() { importMidi(); });
    connect(exportMidiButton, &QPushButton::clicked, this, [this]() { exportMidi(); });
    refreshSongs();

    // Master FX chain strip behaviour lives in FxChainStrip; the panel feeds
    // it the song and shows the level readout while playing.
    auto* fxTimer = new QTimer(this);
    connect(fxTimer, &QTimer::timeout, this, [this]() {
        if (!audioOutput->isPlaying()) return;
        auto& mixer = audioOutput->getMixer();
        const double l = mixer.chainLevelLeft.load(std::memory_order_relaxed);
        const double r = mixer.chainLevelRight.load(std::memory_order_relaxed);
        const long long budget = mixer.chainBudget.load(std::memory_order_relaxed);
        fxStrip->updateMeters(l, r, budget);
    });
    fxTimer->start(100);
}

MusicPanel::~MusicPanel() {
    stopPreview();
}

} // namespace aur
