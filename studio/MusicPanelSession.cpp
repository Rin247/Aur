#include "MusicPanel.hpp"

#include "music/ClipButton.hpp"
#include "music/SongLibrary.hpp"
#include "music/SongTransfer.hpp"
#include "music/TransportIcon.hpp"
#include "theme/LabelRoles.hpp"
#include <QHeaderView>
#include <QScrollBar>
#include "theme/LabelRoles.hpp"
#include <QHeaderView>
#include <QScrollBar>

#include <QHideEvent>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>

namespace aur
{

    void MusicPanel::buildEditorTabs(QSplitter *splitter)
    {
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
    theme::hintLabel(sessionHint);
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
                    std::max(0, value / pianoRoll->stepWidth()));
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
    scaleSelector = new ScaleSelector(pianoPage);
    scaleSelector->setChangeCallback([this](int root, int scale) {
        Json *song = currentSong();
        if (!song)
            return;
        if (scale < 0)
        {
            song->as_object().erase("key_root");
            song->as_object().erase("key_scale");
        }
        else
        {
            (*song)["key_root"] = root;
            (*song)["key_scale"] = scale;
        }
        saveCurrentSong(*song);
    });
    pianoLayout->addWidget(scaleSelector);
    clipLauncher.attachClock(&audioOutput->getMixer().playheadFrames,
                             &audioOutput->getMixer().playheadBpm,
                             &audioOutput->getMixer().playheadActive);
    auto *launcherTimer = new QTimer(this);
    connect(launcherTimer, &QTimer::timeout, this, [this]() {
        const int previousPlaying = launcherPlayingScene;
        if (clipLauncher.tick())
        {
            // A queued slot promoted: swap the mixer to the scene whose
            // slots are now playing, without resetting the transport.
            int promoted = -1;
            for (int scene = 0; scene < sessionGrid->columnCount(); ++scene)
            {
                bool anyPlaying = false;
                for (int track = 0; track < sessionGrid->rowCount(); ++track)
                {
                    if (clipLauncher.state(scene, track) == ClipLauncherStates::State::Playing)
                    {
                        anyPlaying = true;
                        break;
                    }
                }
                if (anyPlaying)
                {
                    promoted = scene;
                    break;
                }
            }
            if (promoted >= 0)
            {
                Json sceneSong = makeScenePreview(promoted);
                if (!sceneSong.is_null())
                {
                    activeScene = promoted;
                    audioOutput->getMixer().loadSong(sceneSong);
                    midiSession->startOutput(sceneSong, currentSongId);
                }
            }
            refreshSessionGrid();
        }
        launcherPlayingScene = clipLauncher.state(
                                        selectedScene >= 0 ? selectedScene : 0, 0) ==
                                ClipLauncherStates::State::Playing
                                   ? selectedScene
                                   : -1;
        Q_UNUSED(previousPlaying);
    });
    launcherTimer->start(100);
    auto* pianoHint = new QLabel(
        "Piano Roll — drag empty grid to marquee-select · Shift+click adds a note · drag to move/resize (Alt = velocity) · Ctrl+click toggles · Ctrl+C/V/D copies/pastes/duplicates · right-click or Delete removes",
        pianoPage);
    theme::hintLabel(pianoHint);
    pianoLayout->addWidget(pianoHint);
    editorTabs->addTab(pianoPage, "Piano Roll");

    auto *arrangePage = new QWidget(editorTabs);
    auto *arrangeLayout = new QVBoxLayout(arrangePage);
    auto *arrangeToolbar = new QHBoxLayout();
    auto *playArrangeButton = new QPushButton("Play Arrangement", arrangePage);
    auto *exportArrangeButton = new QPushButton("Export Arrangement MIDI…", arrangePage);
    arrangeToolbar->addWidget(playArrangeButton);
    arrangeToolbar->addWidget(exportArrangeButton);
    arrangeToolbar->addStretch();
    arrangeLayout->addLayout(arrangeToolbar);
    auto *arrangeScroll = new QScrollArea(arrangePage);
    arrangeScroll->setWidgetResizable(false);
    arrangeScroll->setFrameShape(QFrame::NoFrame);
    arrangementCanvas = new ArrangementCanvas(arrangeScroll);
    arrangeScroll->setWidget(arrangementCanvas);
    arrangementCanvas->setHistory(&songHistory);
    arrangementCanvas->setChangeCallback([this](const Json &song) { saveCurrentSong(song); });
    connect(arrangementCanvas, &ArrangementCanvas::historyChanged, this, [this]() {
        emit undoStateChanged(pianoRoll && pianoRoll->canUndo(),
                              pianoRoll && pianoRoll->canRedo());
    });
    arrangeLayout->addWidget(arrangeScroll, 1);
    connect(playArrangeButton, &QPushButton::clicked, this,
            [this]() { playArrangement(); });
    connect(exportArrangeButton, &QPushButton::clicked, this,
            [this]() { exportArrangementMidi(); });
    auto *arrangeHint = new QLabel(
        "Arrangement — click an empty cell to place the selected scene's clip · drag to move · Delete removes · Play Arrangement flattens the timeline",
        arrangePage);
    theme::hintLabel(arrangeHint);
    arrangeLayout->addWidget(arrangeHint);
    editorTabs->addTab(arrangePage, "Arrangement");

    // Step Sequencer tab: the 128-step grid over the groovebox strip.
    auto *stepPage = new QWidget(editorTabs);
    auto *stepLayout = new QVBoxLayout(stepPage);
    groovePanel = new GrooveBoxPanel(stepPage);
    groovePanel->setChangeCallback([this](const Json &song) { saveCurrentSong(song); });
    connect(groovePanel, &GrooveBoxPanel::historyChanged, this, [this]() {
        emit undoStateChanged(pianoRoll && pianoRoll->canUndo(),
                              pianoRoll && pianoRoll->canRedo());
    });
    groovePanel->setTransportHook([this](bool play) {
        if (play)
            playPreview();
        else
            stopPreview();
    });
    stepLayout->addWidget(groovePanel);
    stepCanvas = new StepSequencerCanvas(stepPage);
    stepCanvas->setHistory(&songHistory);
    stepCanvas->setChangeCallback([this](const Json &song) { saveCurrentSong(song); });
    stepCanvas->setAuditionHook([this](int pitch, int velocity) {
        audioOutput->getMixer().audition(pitch, velocity, 2);
    });
    connect(stepCanvas, &StepSequencerCanvas::historyChanged, this, [this]() {
        emit undoStateChanged(pianoRoll && pianoRoll->canUndo(),
                              pianoRoll && pianoRoll->canRedo());
    });
    stepLayout->addWidget(stepCanvas, 1);
    auto *stepHint = new QLabel(
        "Step Sequencer — click a cell to toggle it · drag to paint · Ctrl+click accents · "
        "right-click clears · arrow keys move · Space toggles · Page Up/Down transposes",
        stepPage);
    theme::hintLabel(stepHint);
    stepHint->setWordWrap(true);
    stepLayout->addWidget(stepHint);
    editorTabs->addTab(stepPage, "Step Sequencer");

    splitter->addWidget(editorTabs);
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
        clipLauncher.launch(selectedScene, 0);
        activeScene = selectedScene;
        refreshSessionGrid();
        if (clipLauncher.state(selectedScene, 0) == ClipLauncherStates::State::Playing)
            playPreview(&scene);
    });
    connect(sessionGrid->horizontalHeader(), &QHeaderView::sectionClicked, this,
            [this](int column) { selectedScene = column; if (arrangementCanvas) arrangementCanvas->setPlaceScene(std::max(0, selectedScene)); });
    connect(sessionGrid, &QTableWidget::cellClicked, this, [this](int row, int column) {
        selectedScene = column;
        if (arrangementCanvas) arrangementCanvas->setPlaceScene(std::max(0, selectedScene));
        if (row >= 0 && row < trackList->count()) trackList->setCurrentRow(row);
    });
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
                const Json &clip =
                    scenes[static_cast<size_t>(column)]["clips"][static_cast<size_t>(row)];
                const bool hasClip = !clip.is_null();
                const int noteCount = hasClip && clip.contains("notes") && clip["notes"].is_array()
                                          ? static_cast<int>(clip["notes"].as_array().size())
                                          : 0;
                const audio::SongDocument::ClipInfo clipInfo =
                    hasClip ? audio::SongDocument::clip_info(clip, trackNames[row].toStdString())
                            : audio::SongDocument::ClipInfo{};
                auto *button = new ClipButton(
                    hasClip ? QString("●  %1\n%2 · %3%4")
                                  .arg(QString::fromStdString(clipInfo.name))
                                  .arg(clipInfo.length / 16)
                                  .arg(noteCount)
                                  .arg(clipLauncher.state(column, row) ==
                                               ClipLauncherStates::State::Playing
                                           ? "\n▶ playing"
                                           : clipLauncher.state(column, row) ==
                                                     ClipLauncherStates::State::Queued
                                           ? QString("\nqueued %1 steps")
                                                 .arg(clipLauncher.queuedInSteps(column, row))
                                           : QString())
                            : QString("+"),
                    sessionGrid);
                button->setMinimumHeight(52);
                const bool active =
                    clipLauncher.state(column, row) != ClipLauncherStates::State::Idle && hasClip;
                button->setState(!hasClip  ? ClipButton::State::Empty
                                 : !active ? ClipButton::State::Clip
                                 : clipLauncher.state(column, row) ==
                                           ClipLauncherStates::State::Playing
                                     ? ClipButton::State::Playing
                                     : ClipButton::State::Queued);
                button->setToolTip(
                    hasClip ? QString("%1 · %2 notes").arg(sceneNames[column]).arg(noteCount)
                            : QString("Create a clip from %1 in %2")
                                  .arg(trackNames[row], sceneNames[column]));
                connect(button, &QPushButton::clicked, this, [this, row, column]() {
                    QTimer::singleShot(0, this, [this, row, column]() {
                        if (row >= 0 && row < trackList->count()) trackList->setCurrentRow(row);
                        toggleClip(column, row);
                    });
                });
                sessionGrid->setCellWidget(row, column, button);
            }
        }
        selectedScene = scenes.empty()
                            ? -1
                            : std::clamp(selectedScene, 0, static_cast<int>(scenes.size()) - 1);
        if (activeScene >= static_cast<int>(scenes.size()))
            activeScene = -1;
    }

    void MusicPanel::toggleClip(int sceneIndex, int trackIndex)
    {
        Json *song = currentSong();
        if (!song || sceneIndex < 0 || trackIndex < 0 || !song->contains("tracks") ||
            !(*song)["tracks"].is_array() ||
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
                    notes = clip["notes"].as_array();
            }
            tracks[trackIndex]["notes"] = notes;
        }
        preview["bars"] = 1;
        return preview;
    }

    void MusicPanel::playPreview(const Json *previewSong)
    {
        const Json *selectedSong = previewSong ? previewSong : currentSong();
        if (!selectedSong || !selectedSong->contains("tracks") ||
            !(*selectedSong)["tracks"].is_array())
        {
            QMessageBox::information(this, "Music Preview",
                                     "Create a song and add notes before previewing.");
            return;
        }

        stopPreview();
        lastPlayedSong = *selectedSong;
        audioOutput->getMixer().resetPlayback();
        audioOutput->getMixer().loadSong(*selectedSong);
        audioOutput->start(audio::QtAudioOutput::kSampleRate);
        midiSession->startOutput(*selectedSong, currentSongId);
        setTransport(Transport::Playing);
    }

    void MusicPanel::pausePreview()
    {
        midiOutputWasOn = midiSession->outputActive();
        audioOutput->stop();
        midiSession->stopOutput();
        setTransport(Transport::Paused);
    }

    void MusicPanel::resumePreview()
    {
        audioOutput->start(audio::QtAudioOutput::kSampleRate);
        if (midiOutputWasOn)
            midiSession->startOutput(lastPlayedSong, currentSongId);
        setTransport(Transport::Playing);
    }

    void MusicPanel::stopPreview()
    {
        audioOutput->stop();
        midiSession->stopOutput();
        audioOutput->getMixer().resetPlayback();
        setTransport(Transport::Stopped);
    }

    void MusicPanel::setTransport(Transport state)
    {
        transport = state;
        if (!playButton)
            return;
        const bool playing = state == Transport::Playing;
        playButton->setIcon(music::transportIcon(playing ? music::TransportGlyph::Pause
                                                         : music::TransportGlyph::Play));
        playButton->setToolTip(playing ? "Pause the playback" : "Play the song");
    }

    void MusicPanel::hideEvent(QHideEvent *event)
    {
        stopPreview();
        QWidget::hideEvent(event);
    }

    bool MusicPanel::flattenedArrangement(Json &flat) const
    {
        const Json *song = currentSong();
        if (!song)
            return false;
        flat = audio::SongDocument::flatten_arrangement(*song);
        if (!flat.contains("tracks") || !flat["tracks"].is_array())
            return false;
        for (const Json &track : flat["tracks"].as_array())
            if (track.contains("notes") && track["notes"].is_array() &&
                !track["notes"].as_array().empty())
                return true;
        return false;
    }

    void MusicPanel::playArrangement()
    {
        Json flat;
        if (!flattenedArrangement(flat))
        {
            QMessageBox::information(this, "Arrangement",
                                     "The arrangement is empty — place clips first.");
            return;
        }
        stopPreview();
        audioOutput->getMixer().resetPlayback();
        audioOutput->getMixer().loadSong(flat);
        audioOutput->start(audio::QtAudioOutput::kSampleRate);
        midiSession->startOutput(flat, currentSongId);
        setTransport(Transport::Playing);
    }

    void MusicPanel::exportArrangementMidi()
    {
        const Json *song = currentSong();
        Json flat;
        if (!song || !flattenedArrangement(flat))
        {
            QMessageBox::information(this, "Arrangement MIDI",
                                     "The arrangement is empty — nothing to export.");
            return;
        }
        const QString name = (*song)["name"].is_string()
                                 ? QString::fromStdString((*song)["name"].as_string())
                                 : QStringLiteral("arrangement");
        music::exportArrangementMidiFile(this, flat, name);
    }

    void MusicPanel::importMidi()
    {
        music::importMidiFile(this, project, [this](const std::string &id) {
            refreshSongs();
            selectSong(QString::fromStdString(id));
        });
    }

    void MusicPanel::exportMidi()
    {
        const Json *song = currentSong();
        if (!song)
        {
            QMessageBox::information(this, "MIDI Export", "Select a song to export.");
            return;
        }
        music::exportMidiFile(this, *song, currentSongId);
    }

    std::vector<audio::SongDocument::EffectSlot> MusicPanel::currentChain() const
    {
        const Json *song = currentSong();
        return song ? audio::SongDocument::effect_chain(*song)
                    : std::vector<audio::SongDocument::EffectSlot>{};
    }

    void MusicPanel::writeChain(const std::vector<audio::SongDocument::EffectSlot> &chain)
    {
        Json *song = currentSong();
        if (!song)
            return;
        audio::SongDocument::set_effect_chain(*song, chain);
        saveCurrentSong(*song);
    }

    void MusicPanel::refreshFxChain()
    {
        if (fxStrip)
            fxStrip->setSong(currentSong());
    }

}
