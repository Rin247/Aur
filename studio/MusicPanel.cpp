#include "MusicPanel.hpp"
#include "music/SongLibrary.hpp"
#include "music/VelocityLaneStrip.hpp"
#include "aur/audio/midi.hpp"
#include "aur/audio/synth.hpp"
#include "aur/audio/SongDocument.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <array>
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
        if (project && !currentSongId.isEmpty())
            project->music[currentSongId.toStdString()] = *song;
        updateTrackProperties();
        refreshTracks();
        return true;
    }

    bool MusicPanel::redoSongEdit()
    {
        Json *song = currentSong();
        if (!song || !pianoRoll || !pianoRoll->canRedo())
            return false;
        pianoRoll->redo();
        if (project && !currentSongId.isEmpty())
            project->music[currentSongId.toStdString()] = *song;
        updateTrackProperties();
        refreshTracks();
        return true;
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
                                    : "gm";
        previewSynth->setCurrentIndex(std::max(0, previewSynth->findData(synthId)));
        selectedTrack = 0;
        refreshTracks();
        trackList->setCurrentRow(selectedTrack);
        pianoRoll->setSong(*song, selectedTrack); velocityStrip->setSong(*song, selectedTrack);
        updateTrackProperties();
        updatingControls = false;
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

    void MusicPanel::refreshSessionGrid()
    {
        if (!sessionGrid)
            return;
        Json *song = currentSong();
        if (!song || !song->contains("tracks") || !(*song)["tracks"].is_array())
        {
            sessionGrid->clear();
            sessionGrid->setRowCount(0);
            sessionGrid->setColumnCount(0);
            selectedScene = -1;
            return;
        }

        music::initializeScenes(*song);
        JsonArray &scenes = (*song)["scenes"].as_array();
        const JsonArray &tracks = (*song)["tracks"].as_array();
        for (Json &scene : scenes)
        {
            if (!scene.contains("clips") || !scene["clips"].is_array())
                scene["clips"] = JsonArray{};
            scene["clips"].as_array().resize(tracks.size());
        }
        sessionGrid->clear();
        sessionGrid->setRowCount(static_cast<int>(tracks.size()));
        sessionGrid->setColumnCount(static_cast<int>(scenes.size()));
        QStringList trackNames;
        for (size_t row = 0; row < tracks.size(); ++row)
        {
            const Json &track = tracks[row];
            const std::string name = track.contains("name") && track["name"].is_string()
                                         ? track["name"].as_string()
                                         : "Track " + std::to_string(row + 1);
            trackNames.push_back(QString::fromStdString(name));
        }
        QStringList sceneNames;
        for (size_t column = 0; column < scenes.size(); ++column)
        {
            const Json &scene = scenes[column];
            const std::string name = scene.contains("name") && scene["name"].is_string()
                                         ? scene["name"].as_string()
                                         : "Scene " + std::to_string(column + 1);
            sceneNames.push_back(QString::fromStdString(name));
        }
        sessionGrid->setVerticalHeaderLabels(trackNames);
        sessionGrid->setHorizontalHeaderLabels(sceneNames);

        for (int row = 0; row < sessionGrid->rowCount(); ++row)
        {
            for (int column = 0; column < sessionGrid->columnCount(); ++column)
            {
                const Json &clip = scenes[static_cast<size_t>(column)]["clips"][static_cast<size_t>(row)];
                const bool hasClip = !clip.is_null();
                const int noteCount = hasClip && clip.contains("notes") && clip["notes"].is_array()
                                          ? static_cast<int>(clip["notes"].as_array().size())
                                          : 0;
                const audio::SongDocument::ClipInfo clipInfo =
                    hasClip ? audio::SongDocument::clip_info(clip, trackNames[row].toStdString())
                            : audio::SongDocument::ClipInfo{};
                auto *button = new QPushButton(
                    hasClip ? QString("●  %1\n%2 · %3")
                                  .arg(QString::fromStdString(clipInfo.name))
                                  .arg(clipInfo.length / 16)
                                  .arg(noteCount)
                            : QString("+"),
                    sessionGrid);
                button->setMinimumHeight(52);
                const bool active = activeScene == column && hasClip;
                button->setStyleSheet(active
                                          ? "QPushButton { background:#795c2d; border:2px solid #f0bd58; color:#fff1cf; font-weight:700; }"
                                      : hasClip
                                          ? "QPushButton { background:#285b52; border:1px solid #428d79; color:#e4fff2; }"
                                          : "QPushButton { background:#1b222a; border:1px dashed #49535d; color:#929da8; }");
                button->setToolTip(hasClip
                                       ? QString("%1 · %2 notes").arg(sceneNames[column]).arg(noteCount)
                                       : QString("Create a clip from %1 in %2").arg(trackNames[row], sceneNames[column]));
                connect(button, &QPushButton::clicked, this, [this, row, column]()
                        { QTimer::singleShot(0, this, [this, row, column]()
                                             {
                    if (row >= 0 && row < trackList->count()) trackList->setCurrentRow(row);
                    toggleClip(column, row); }); });
                sessionGrid->setCellWidget(row, column, button);
            }
        }
        selectedScene = scenes.empty() ? -1 : std::clamp(selectedScene, 0, static_cast<int>(scenes.size()) - 1);
        if (activeScene >= static_cast<int>(scenes.size()))
            activeScene = -1;
    }

    void MusicPanel::toggleClip(int sceneIndex, int trackIndex)
    {
        Json *song = currentSong();
        if (!song || sceneIndex < 0 || trackIndex < 0 ||
            !song->contains("tracks") || !(*song)["tracks"].is_array() ||
            trackIndex >= static_cast<int>((*song)["tracks"].as_array().size()))
            return;
        music::initializeScenes(*song);
        JsonArray &scenes = (*song)["scenes"].as_array();
        if (sceneIndex >= static_cast<int>(scenes.size()))
            return;
        Json &scene = scenes[static_cast<size_t>(sceneIndex)];
        JsonArray &clips = scene["clips"].as_array();
        clips.resize((*song)["tracks"].as_array().size());
        Json &clip = clips[static_cast<size_t>(trackIndex)];
        if (clip.is_null())
        {
            const Json &track = (*song)["tracks"][static_cast<size_t>(trackIndex)];
            JsonArray clipNotes = music::notesForBar(track, sceneIndex);
            clip = JsonObject{{"notes", clipNotes}};
            const std::string clipName =
                (track.contains("name") && track["name"].is_string()
                     ? track["name"].as_string()
                     : "Track " + std::to_string(trackIndex + 1)) +
                " · " + std::to_string(sceneIndex + 1);
            audio::SongDocument::set_clip_info(
                clip, audio::SongDocument::ClipInfo{clipName, 0, 16, false, -1});
            selectedScene = sceneIndex;
            activeScene = -1;
        }
        else
        {
            selectedScene = sceneIndex;
            activeScene = sceneIndex;
        }
        refreshSessionGrid();
    }

    Json MusicPanel::makeScenePreview(int sceneIndex)
    {
        const Json *source = currentSong();
        if (!source || sceneIndex < 0 || !source->contains("scenes") ||
            !(*source)["scenes"].is_array() ||
            sceneIndex >= static_cast<int>((*source)["scenes"].as_array().size()) ||
            !source->contains("tracks") || !(*source)["tracks"].is_array())
            return {};

        Json preview = *source;
        JsonArray &tracks = preview["tracks"].as_array();
        const Json &scene = (*source)["scenes"][static_cast<size_t>(sceneIndex)];
        for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
        {
            JsonArray notes;
            if (scene.contains("clips") && scene["clips"].is_array() &&
                trackIndex < scene["clips"].as_array().size())
            {
                const Json &clip = scene["clips"][trackIndex];
                if (!clip.is_null() && clip.contains("notes") && clip["notes"].is_array())
                {
                    notes = clip["notes"].as_array();
                }
            }
            tracks[trackIndex]["notes"] = notes;
        }
        preview["bars"] = 1;
        return preview;
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

    void MusicPanel::playPreview(const Json *previewSong)
    {
        const Json *selectedSong = previewSong ? previewSong : currentSong();
        if (!selectedSong || !selectedSong->contains("tracks") ||
            !(*selectedSong)["tracks"].is_array())
        {
            QMessageBox::information(this, "Music Preview", "Create a song and add notes before previewing.");
            return;
        }

        stopPreview();
        audioOutput->getMixer().resetPlayback();
        audioOutput->getMixer().loadSong(*selectedSong);
        audioOutput->start(audio::QtAudioOutput::kSampleRate);
        midiSession->startOutput(*selectedSong, currentSongId);
    }

    void MusicPanel::stopPreview()
    {
        audioOutput->stop();
        midiSession->stopOutput();
    }

    void MusicPanel::importMidi()
    {
        if (!project)
            return;
        const QString path = QFileDialog::getOpenFileName(
            this, "Import Standard MIDI File", {}, "MIDI files (*.mid *.midi)");
        if (path.isEmpty())
            return;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "MIDI Import", file.errorString());
            return;
        }
        const QByteArray bytes = file.readAll();
        std::vector<uint8_t> midiBytes;
        midiBytes.reserve(static_cast<size_t>(bytes.size()));
        for (char byte : bytes)
            midiBytes.push_back(static_cast<uint8_t>(byte));
        try
        {
            Json song = import_standard_midi(
                midiBytes, QFileInfo(path).completeBaseName().toStdString());
            const std::string base = music::songIdFromName(
                QString::fromStdString(song["name"].as_string()));
            std::string id = base;
            int suffix = 2;
            while (project->music.contains(id))
                id = base + "_" + std::to_string(suffix++);
            project->music[id] = std::move(song);
            refreshSongs();
            selectSong(QString::fromStdString(id));
        }
        catch (const std::exception &error)
        {
            QMessageBox::warning(this, "MIDI Import", QString::fromStdString(error.what()));
        }
    }

    void MusicPanel::exportMidi()
    {
        const Json *song = currentSong();
        if (!song)
        {
            QMessageBox::information(this, "MIDI Export", "Select a song to export.");
            return;
        }
        const QString title = (*song)["name"].is_string()
                                  ? QString::fromStdString((*song)["name"].as_string())
                                  : currentSongId;
        const QString path = QFileDialog::getSaveFileName(
            this, "Export Standard MIDI File", title + ".mid", "MIDI files (*.mid)");
        if (path.isEmpty())
            return;

        try
        {
            const std::vector<uint8_t> data = export_standard_midi(*song);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                QMessageBox::warning(this, "MIDI Export", file.errorString());
                return;
            }
            const qint64 written = file.write(
                reinterpret_cast<const char *>(data.data()), static_cast<qint64>(data.size()));
            if (written != static_cast<qint64>(data.size()))
            {
                QMessageBox::warning(this, "MIDI Export",
                                     file.errorString().isEmpty() ? "Could not write the complete MIDI file."
                                                                  : file.errorString());
            }
        }
        catch (const std::exception &error)
        {
            QMessageBox::warning(this, "MIDI Export", QString::fromStdString(error.what()));
        }
    }

namespace {

const std::array<const char*, 128>& gmInstrumentNames() {
    static constexpr std::array<const char*, 128> names = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
        "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord",
        "Clavinet", "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba",
        "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ", "Percussive Organ",
        "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica",
        "Tango Accordion", "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)",
        "Electric Guitar (jazz)", "Electric Guitar (clean)", "Electric Guitar (muted)",
        "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics", "Acoustic Bass",
        "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass", "Slap Bass 1",
        "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", "Violin", "Viola", "Cello",
        "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit", "Trumpet", "Trombone",
        "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1",
        "Synth Brass 2", "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe",
        "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina", "Lead 1 (square)",
        "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)", "Lead 5 (charang)",
        "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)", "Pad 1 (new age)",
        "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)", "Pad 5 (bowed)",
        "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)", "FX 1 (rain)",
        "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)", "FX 5 (brightness)",
        "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", "Sitar", "Banjo", "Shamisen",
        "Koto", "Kalimba", "Bag Pipe", "Fiddle", "Shanai", "Tinkle Bell", "Agogo",
        "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum",
        "Reverse Cymbal", "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"
    };
    return names;
}

} // namespace

MusicPanel::MusicPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    audioOutput = new audio::QtAudioOutput(this);
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
        if (songId == currentSongId) {
            refreshTracks();
            pianoRoll->setSong(song, selectedTrack); velocityStrip->setSong(song, selectedTrack);
            refreshSessionGrid();
        }
    });
    connect(midiSession, &MidiSession::statusChanged, this, [this](const QString& status) {
        midiStatusLabel->setText(status);
    });
    connect(midiSession, &MidiSession::outputError, this, [this](const QString& message) {
        QMessageBox::warning(this, "MIDI Output", message);
    });

    auto* layout = new QVBoxLayout(this);
    auto* songBar = new QHBoxLayout();
    songBar->addWidget(new QLabel("Song:", this));
    songCombo = new QComboBox(this);
    songCombo->setMinimumWidth(220);
    songBar->addWidget(songCombo);
    auto* newSongButton = new QPushButton("New", this);
    auto* deleteSongButton = new QPushButton("Delete", this);
    songBar->addWidget(newSongButton);
    songBar->addWidget(deleteSongButton);
    songBar->addSpacing(12);
    songBar->addWidget(new QLabel("Title:", this));
    songName = new QLineEdit(this);
    songName->setMaximumWidth(200);
    songBar->addWidget(songName);
    songBar->addWidget(new QLabel("Tempo:", this));
    tempo = new QSpinBox(this);
    tempo->setRange(30, 240);
    tempo->setSuffix(" BPM");
    songBar->addWidget(tempo);
    previewSynth = new QComboBox(this);
    previewSynth->addItem("GM-style instruments", "gm");
    previewSynth->addItem("FM (6-op style)", "fm");
    previewSynth->addItem("Retro PSG", "retro_psg");
    previewSynth->setToolTip(
        "Built-in GM patch-family synthesis. MIDI output sends the selected bank and program to a connected GM/GS device.");
    songBar->addWidget(new QLabel("Preview:", this));
    songBar->addWidget(previewSynth);
    playButton = new QPushButton("Play Preview", this);
    auto* stopButton = new QPushButton("Stop", this);
    auto* importMidiButton = new QPushButton("Import MIDI…", this);
    auto* exportMidiButton = new QPushButton("Export MIDI…", this);
    songBar->addWidget(playButton);
    songBar->addWidget(stopButton);
    songBar->addWidget(importMidiButton);
    songBar->addWidget(exportMidiButton);
    songBar->addStretch();
    layout->addLayout(songBar);

    auto* midiBar = new QHBoxLayout();
    midiBar->addWidget(new QLabel("MIDI In:", this));
    midiInputCombo = new QComboBox(this);
    midiInputCombo->setMinimumWidth(180);
    midiBar->addWidget(midiInputCombo);
    midiInputButton = new QPushButton("Connect", this);
    midiBar->addWidget(midiInputButton);
    midiBar->addSpacing(8);
    midiBar->addWidget(new QLabel("MIDI Out:", this));
    midiOutputCombo = new QComboBox(this);
    midiOutputCombo->setMinimumWidth(180);
    midiBar->addWidget(midiOutputCombo);
    midiOutputButton = new QPushButton("Connect", this);
    midiBar->addWidget(midiOutputButton);
    auto* refreshMidiButton = new QPushButton("Refresh devices", this);
    midiBar->addWidget(refreshMidiButton);
    recordMidiButton = new QPushButton("Record MIDI", this);
    recordMidiButton->setCheckable(true);
    recordMidiButton->setToolTip("Arm MIDI input and record notes into the selected track.");
    midiBar->addWidget(recordMidiButton);
    countInBarsSpin = new QSpinBox(this);
    countInBarsSpin->setRange(0, 8);
    countInBarsSpin->setValue(1);
    countInBarsSpin->setSuffix(" bars");
    countInBarsSpin->setToolTip("Count-in before recorded notes are captured (0 disables).");
    midiBar->addWidget(new QLabel("Count-in", this));
    midiBar->addWidget(countInBarsSpin);
    midiStatusLabel = new QLabel("No MIDI devices connected", this);
    midiStatusLabel->setStyleSheet("color: #aeb8c2;");
    midiBar->addWidget(midiStatusLabel, 1);
    layout->addLayout(midiBar);

    auto* presetBar = new QHBoxLayout();
    presetBar->addWidget(new QLabel("Quick Compose:", this));
    auto* overworldButton = new QPushButton("Overworld", this);
    auto* battleButton = new QPushButton("Battle", this);
    auto* villageButton = new QPushButton("Village", this);
    presetBar->addWidget(overworldButton);
    presetBar->addWidget(battleButton);
    presetBar->addWidget(villageButton);
    presetBar->addWidget(new QLabel("Presets create editable project songs.", this));
    presetBar->addStretch();
    layout->addLayout(presetBar);

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
    instrument = new QComboBox(trackPane);
    instrument->setEditable(true);
    instrument->setInsertPolicy(QComboBox::NoInsert);
    instrument->setMinimumContentsLength(25);
    instrument->setMaxVisibleItems(24);
    instrument->setToolTip("GM program number and instrument name. Type to filter the 128 General MIDI patches.");
    for (int program = 0; program < static_cast<int>(gmInstrumentNames().size()); ++program) {
        instrument->addItem(
            QString("%1  %2").arg(program + 1, 3, 10, QChar('0'))
                .arg(QString::fromLatin1(gmInstrumentNames()[static_cast<size_t>(program)])),
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

    editorTabs = new QTabWidget(splitter);

    auto* sessionPage = new QWidget(editorTabs);
    auto* sessionLayout = new QVBoxLayout(sessionPage);
    auto* sessionToolbar = new QHBoxLayout();
    auto* addSceneButton = new QPushButton("Add Scene", sessionPage);
    auto* removeSceneButton = new QPushButton("Remove Scene", sessionPage);
    auto* removeClipButton = new QPushButton("Remove Clip", sessionPage);
    playSceneButton = new QPushButton("Play Scene", sessionPage);
    sessionToolbar->addWidget(addSceneButton);
    sessionToolbar->addWidget(removeSceneButton);
    sessionToolbar->addWidget(removeClipButton);
    sessionToolbar->addStretch();
    sessionToolbar->addWidget(playSceneButton);
    sessionLayout->addLayout(sessionToolbar);
    sessionGrid = new QTableWidget(sessionPage);
    sessionGrid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sessionGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    sessionGrid->setSelectionBehavior(QAbstractItemView::SelectItems);
    sessionGrid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    sessionGrid->horizontalHeader()->setDefaultSectionSize(120);
    sessionGrid->verticalHeader()->setDefaultSectionSize(66);
    sessionGrid->horizontalHeader()->setStretchLastSection(false);
    sessionLayout->addWidget(sessionGrid, 1);
    auto* sessionHint = new QLabel(
        "Each column is a launchable scene; each row is a track. Click an empty slot to create a clip "
        "from that bar of the piano roll, then select a scene and Play Scene.",
        sessionPage);
    sessionHint->setWordWrap(true);
    sessionHint->setStyleSheet("color: #aeb5c5; padding: 3px;");
    sessionLayout->addWidget(sessionHint);
    editorTabs->addTab(sessionPage, "Session Grid");

    auto* pianoPage = new QWidget(editorTabs);
    auto* pianoLayout = new QVBoxLayout(pianoPage);
    auto* pianoScroll = new QScrollArea(pianoPage);
    pianoScroll->setWidgetResizable(false);
    pianoScroll->setFrameShape(QFrame::StyledPanel);
    pianoRoll = new PianoRollCanvasWidget(pianoScroll);
    pianoScroll->setWidget(pianoRoll);
    connect(pianoRoll, &QObject::destroyed, this, [this]() { pianoRoll = nullptr; });
    connect(pianoScroll->horizontalScrollBar(), &QScrollBar::valueChanged,
            pianoRoll, [this](int value) {
                if (pianoRoll) pianoRoll->setVisibleStep(
                    std::max(0, value / PianoRollCanvasWidget::stepWidth));
            });
    pianoLayout->addWidget(pianoScroll, 1);
    velocityStrip = new VelocityLaneStrip(pianoPage);
    velocityStrip->setHistory(&songHistory);
    velocityStrip->setChangeCallback([this](const Json& song) { saveCurrentSong(song); });
    connect(velocityStrip, &VelocityLaneStrip::historyChanged, this, [this]() {
        emit undoStateChanged(pianoRoll && pianoRoll->canUndo(),
                              pianoRoll && pianoRoll->canRedo());
    });
    pianoLayout->addWidget(velocityStrip);
    auto* pianoHint = new QLabel(
        "Piano Roll — drag empty grid to marquee-select · Shift+click adds a note · drag to move/resize (Alt = velocity) · Ctrl+click toggles · Ctrl+C/V/D copies/pastes/duplicates · right-click or Delete removes",
        pianoPage);
    pianoHint->setStyleSheet("color: #aeb5c5; padding: 3px;");
    pianoLayout->addWidget(pianoHint);
    editorTabs->addTab(pianoPage, "Piano Roll");
    splitter->addWidget(editorTabs);
    splitter->setSizes({270, 1300});
    layout->addWidget(splitter, 1);

    pianoRoll->setChangeCallback([this](const Json& song) { saveCurrentSong(song); });
    pianoRoll->setHistory(&songHistory);
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
            {"beats", 4}, {"loop_bar", 0}, {"preview_synth", "gm"},
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
        refreshSessionGrid();
    });
    connect(addSceneButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song) return;
        music::initializeScenes(*song);
        JsonArray clips((*song)["tracks"].as_array().size(), Json{});
        JsonArray& scenes = (*song)["scenes"].as_array();
        scenes.emplace_back(JsonObject{
            {"name", "Scene " + std::to_string(scenes.size() + 1)}, {"clips", clips}
        });
        selectedScene = static_cast<int>(scenes.size()) - 1;
        refreshSessionGrid();
    });
    connect(removeSceneButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song || !song->contains("scenes") || !(*song)["scenes"].is_array() ||
            selectedScene < 0 ||
            selectedScene >= static_cast<int>((*song)["scenes"].as_array().size())) return;
        JsonArray& scenes = (*song)["scenes"].as_array();
        scenes.erase(scenes.begin() + selectedScene);
        selectedScene = scenes.empty() ? -1 :
            std::min(selectedScene, static_cast<int>(scenes.size()) - 1);
        if (activeScene >= static_cast<int>(scenes.size())) activeScene = -1;
        refreshSessionGrid();
    });
    connect(removeClipButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song || selectedScene < 0 || selectedTrack < 0 ||
            !song->contains("scenes") || selectedScene >=
                static_cast<int>((*song)["scenes"].as_array().size())) return;
        JsonArray& clips = (*song)["scenes"][static_cast<size_t>(selectedScene)]["clips"].as_array();
        if (selectedTrack >= static_cast<int>(clips.size()) ||
            clips[static_cast<size_t>(selectedTrack)].is_null()) return;
        clips[static_cast<size_t>(selectedTrack)] = Json{};
        if (activeScene == selectedScene) activeScene = -1;
        refreshSessionGrid();
    });
    connect(playSceneButton, &QPushButton::clicked, this, [this]() {
        if (selectedScene < 0) return;
        const Json scene = makeScenePreview(selectedScene);
        if (scene.is_null()) return;
        activeScene = selectedScene;
        refreshSessionGrid();
        playPreview(&scene);
    });
    connect(sessionGrid->horizontalHeader(), &QHeaderView::sectionClicked, this,
            [this](int column) { selectedScene = column; });
    connect(sessionGrid, &QTableWidget::cellClicked, this, [this](int row, int column) {
        selectedScene = column;
        if (row >= 0 && row < trackList->count()) trackList->setCurrentRow(row);
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
        playPreview();
        playButton->setText("Playing…");
    });
    connect(stopButton, &QPushButton::clicked, this, [this]() {
        stopPreview();
        playButton->setText("Play Preview");
    });
    connect(midiInputButton, &QPushButton::clicked, this,
            [this]() { midiSession->toggleInput(midiInputCombo, midiInputButton); });
    connect(midiOutputButton, &QPushButton::clicked, this,
            [this]() { midiSession->toggleOutput(midiOutputCombo, midiOutputButton); });
    connect(refreshMidiButton, &QPushButton::clicked, this, [this]() {
        midiSession->refreshDevices(midiInputCombo, midiOutputCombo);
    });
    connect(recordMidiButton, &QPushButton::toggled, this, [this](bool enabled) {
        if (enabled && (!midiSession->devices().inputConnected() || !currentSong() ||
                        selectedTrack < 0)) {
            const QSignalBlocker blocker(recordMidiButton);
            recordMidiButton->setChecked(false);
            QMessageBox::information(this, "MIDI Recording",
                "Connect a MIDI input, select a song, and select a track before recording.");
            return;
        }
        midiSession->setSelectedTrack(selectedTrack);
        midiSession->setRecordTarget(currentSongId);
        midiSession->setCountInBars(countInBarsSpin->value());
        midiSession->setRecording(enabled);
        recordMidiButton->setText(enabled ? "Stop Recording" : "Record MIDI");
    });
    connect(importMidiButton, &QPushButton::clicked, this, [this]() { importMidi(); });
    connect(exportMidiButton, &QPushButton::clicked, this, [this]() { exportMidi(); });
    midiSession->refreshDevices(midiInputCombo, midiOutputCombo);
    refreshSongs();
}

MusicPanel::~MusicPanel() {
    stopPreview();
}

} // namespace aur
