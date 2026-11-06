#include "MusicPanel.hpp"
#include "aur/audio/midi.hpp"
#include "aur/audio/synth.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QByteArray>
#include <QCompleter>
#include <QFrame>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace aur {
namespace {

constexpr int keyWidth = 40;
constexpr int stepWidth = 16;
constexpr int pitchHeight = 16;
constexpr int lowestPitch = 48;
constexpr int highestPitch = 83;
constexpr int minimumStepCount = 128;
constexpr int pitchCount = highestPitch - lowestPitch + 1;
constexpr int sampleRate = 44100;

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

int clampedInt(const Json& value, int fallback, int low, int high) {
    return value.is_number() ? std::clamp(value.as_int(), low, high) : fallback;
}

QString noteName(int midiPitch) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F",
                                  "F#", "G", "G#", "A", "A#", "B"};
    return QString("%1%2").arg(names[midiPitch % 12]).arg(midiPitch / 12 - 1);
}

std::string songIdFromName(const QString& name) {
    std::string id;
    for (const QChar ch : name.trimmed().toLower()) {
        if (ch.isLetterOrNumber()) id.push_back(ch.toLatin1());
        else if (!id.empty() && id.back() != '_') id.push_back('_');
    }
    while (!id.empty() && id.back() == '_') id.pop_back();
    return id.empty() ? "song" : id;
}

void appendU16(QByteArray& data, std::uint16_t value) {
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray& data, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        data.append(static_cast<char>((value >> shift) & 0xff));
}

Json makeTrack(const std::string& name, int channel, int program) {
    return JsonObject{
        {"name", name}, {"channel", channel}, {"program", program},
        {"bank", 0}, {"volume", 90}, {"pan", 64}, {"reverb", 0},
        {"chorus", 0}, {"drum", channel == 9}, {"notes", JsonArray{}}
    };
}

JsonArray notesForBar(const Json& track, int bar) {
    JsonArray clipNotes;
    if (!track.contains("notes") || !track["notes"].is_array()) return clipNotes;
    const int barStart = bar * 16;
    const int barEnd = barStart + 16;
    for (const Json& note : track["notes"].as_array()) {
        if (!note.is_array() || note.as_array().size() < 3) continue;
        const int start = note[0].as_int();
        const int end = start + std::max(1, note[2].as_int());
        const int overlapStart = std::max(start, barStart);
        const int overlapEnd = std::min(end, barEnd);
        if (overlapStart >= overlapEnd) continue;
        JsonArray clipNote{overlapStart - barStart, note[1], overlapEnd - overlapStart};
        if (note.as_array().size() >= 4) clipNote.emplace_back(note[3]);
        clipNotes.emplace_back(std::move(clipNote));
    }
    return clipNotes;
}

void initializeScenes(Json& song) {
    if (song.contains("scenes") && song["scenes"].is_array()) return;
    const JsonArray& tracks = song.contains("tracks") && song["tracks"].is_array()
        ? song["tracks"].as_array() : JsonArray{};
    const int bars = clampedInt(song["bars"], 8, 1, 128);
    JsonArray scenes;
    for (int bar = 0; bar < bars; ++bar) {
        JsonArray clips(tracks.size(), Json{});
        for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
            JsonArray clipNotes = notesForBar(tracks[trackIndex], bar);
            if (!clipNotes.empty()) clips[trackIndex] = JsonObject{{"notes", clipNotes}};
        }
        scenes.emplace_back(JsonObject{
            {"name", "Scene " + std::to_string(bar + 1)}, {"clips", clips}
        });
    }
    song["scenes"] = scenes;
}

} // namespace

PianoRollCanvasWidget::PianoRollCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(keyWidth + minimumStepCount * stepWidth, pitchCount * pitchHeight);
    setFocusPolicy(Qt::StrongFocus);
}

void PianoRollCanvasWidget::setSong(const Json& value, int selectedTrack) {
    song = value;
    trackIndex = selectedTrack;
    selectedNote = -1;
    const int newContentWidth = contentWidth();
    if (newContentWidth > width()) resize(newContentWidth, pitchCount * pitchHeight);
    update();
}

void PianoRollCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (event->size().width() > event->oldSize().width()) {
        const int newContentWidth = contentWidth();
        if (newContentWidth > width()) resize(newContentWidth, height());
    }
}

void PianoRollCanvasWidget::setSelectedTrack(int selectedTrack) {
    trackIndex = selectedTrack;
    selectedNote = -1;
    update();
}

void PianoRollCanvasWidget::setVisibleStep(int firstStep) {
    firstVisibleStep = std::max(0, firstStep);
    update();
}

int PianoRollCanvasWidget::contentWidth() const {
    int lastStep = minimumStepCount;
    if (song.contains("tracks") && song["tracks"].is_array()) {
        for (const Json& track : song["tracks"].as_array()) {
            if (!track.contains("notes") || !track["notes"].is_array()) continue;
            for (const Json& note : track["notes"].as_array()) {
                if (!note.is_array() || note.as_array().size() < 3) continue;
                const int noteEnd = std::max(0, note[0].as_int()) +
                                    std::max(1, note[2].as_int());
                lastStep = std::max(lastStep, noteEnd);
            }
        }
    }
    const int viewportSteps = std::max(1, (width() - keyWidth) / stepWidth);
    const int requiredSteps = ((lastStep + viewportSteps - 1) / viewportSteps) * viewportSteps;
    return keyWidth + requiredSteps * stepWidth;
}

void PianoRollCanvasWidget::setChangeCallback(std::function<void(const Json&)> callback) {
    onSongChanged = std::move(callback);
}

int PianoRollCanvasWidget::noteAt(const QPoint& pos) const {
    if (!song.contains("tracks") || !song["tracks"].is_array() ||
        trackIndex < 0 || trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
        return -1;
    const Json& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"];
    if (!notes.is_array()) return -1;
    const int x = pos.x() - keyWidth;
    const int pitch = highestPitch - (pos.y() / pitchHeight);
    if (x < 0 || pitch < lowestPitch || pitch > highestPitch) return -1;
    for (int i = static_cast<int>(notes.as_array().size()) - 1; i >= 0; --i) {
        const Json& note = notes[static_cast<size_t>(i)];
        if (!note.is_array() || note.as_array().size() < 3) continue;
        const int start = note[0].as_int();
        const int length = std::max(1, note[2].as_int());
        const int left = keyWidth + start * stepWidth;
        const int right = left + length * stepWidth;
        const int top = (highestPitch - note[1].as_int()) * pitchHeight;
        if (pos.x() >= left && pos.x() < right &&
            pos.y() >= top && pos.y() < top + pitchHeight)
            return i;
    }
    return -1;
}

void PianoRollCanvasWidget::notifySongChanged() {
    if (onSongChanged) onSongChanged(song);
    update();
}

void PianoRollCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#171922"));

    for (int row = 0; row < pitchCount; ++row) {
        const int pitch = highestPitch - row;
        const QRect key(0, row * pitchHeight, keyWidth, pitchHeight);
        const bool blackKey = pitch % 12 == 1 || pitch % 12 == 3 ||
                              pitch % 12 == 6 || pitch % 12 == 8 || pitch % 12 == 10;
        painter.fillRect(key, blackKey ? QColor("#292c38") : QColor("#363945"));
        painter.setPen(QColor("#858a99"));
        painter.drawText(key.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignRight,
                         noteName(pitch));
        if (pitch % 12 == 0) {
            painter.fillRect(QRect(keyWidth, row * pitchHeight,
                                   width() - keyWidth, pitchHeight),
                             QColor(255, 255, 255, 8));
        }
    }

    const int displayedSteps = (width() - keyWidth) / stepWidth;
    const int firstGridStep = (firstVisibleStep / 16) * 16;
    const int lastGridStep = firstVisibleStep + displayedSteps;
    for (int step = firstGridStep; step <= lastGridStep; ++step) {
        const int x = keyWidth + (step - firstVisibleStep) * stepWidth;
        const int barNumber = step / 16;
        const int barBeat = step % 16;
        painter.setPen(barBeat == 0 ? QColor("#555b6d") :
                       barBeat % 4 == 0 ? QColor("#393d4b") : QColor("#272a35"));
        painter.drawLine(x, 0, x, height());
        if (barBeat == 0 && step >= firstVisibleStep) {
            painter.setPen(QColor("#aeb5c5"));
            painter.drawText(x + 3, 12, QString::number(barNumber + 1));
        }
    }
    for (int row = 0; row <= pitchCount; ++row) {
        painter.setPen(QColor("#30333f"));
        painter.drawLine(keyWidth, row * pitchHeight, width(), row * pitchHeight);
    }
    painter.setPen(QColor("#0c0d12"));
    painter.drawLine(keyWidth - 1, 0, keyWidth - 1, height());

    if (!song.contains("tracks") || !song["tracks"].is_array() ||
        trackIndex < 0 || trackIndex >= static_cast<int>(song["tracks"].as_array().size())) {
        painter.setPen(Qt::white);
        painter.drawText(QRect(keyWidth + 8, 8, width() - keyWidth - 16, 28),
                         Qt::AlignLeft, "Add or select a track, then click the grid to create notes.");
        return;
    }

    const Json& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"];
    if (!notes.is_array()) return;
    const QColor noteColor("#48d6a0");
    for (size_t i = 0; i < notes.as_array().size(); ++i) {
        const Json& note = notes[i];
        if (!note.is_array() || note.as_array().size() < 3) continue;
        const int step = std::max(0, note[0].as_int());
        const int pitch = std::clamp(note[1].as_int(), lowestPitch, highestPitch);
        const int length = std::max(1, note[2].as_int());
        const int visibleStart = std::max(step, firstVisibleStep);
        const int visibleEnd = std::min(step + length, firstVisibleStep + displayedSteps);
        if (visibleStart >= visibleEnd) continue;
        const QRect noteRect(keyWidth + (visibleStart - firstVisibleStep) * stepWidth + 1,
                             (highestPitch - pitch) * pitchHeight + 1,
                             std::max(2, (visibleEnd - visibleStart) * stepWidth - 2), pitchHeight - 2);
        painter.setPen(i == static_cast<size_t>(selectedNote)
                           ? QPen(QColor("#fff0a8"), 2)
                           : QPen(noteColor.darker(150), 1));
        painter.setBrush(i == static_cast<size_t>(selectedNote)
                             ? QColor("#f0bd58") : noteColor);
        painter.drawRoundedRect(noteRect, 2, 2);
    }
}

void PianoRollCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        selectedNote = noteAt(event->pos());
        if (selectedNote >= 0) {
            JsonArray& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
            notes.erase(notes.begin() + selectedNote);
            selectedNote = -1;
            notifySongChanged();
        }
        return;
    }
    if (event->button() != Qt::LeftButton || event->pos().x() < keyWidth) return;
    setFocus();
    const int step = firstVisibleStep + std::max(0, (event->pos().x() - keyWidth) / stepWidth);
    const int pitch = std::clamp(highestPitch - event->pos().y() / pitchHeight,
                                 lowestPitch, highestPitch);
    selectedNote = noteAt(event->pos());
    if (selectedNote < 0) {
        if (trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array() ||
            trackIndex >= static_cast<int>(song["tracks"].as_array().size()))
            return;
        JsonArray note{step, pitch, 4, 90};
        JsonArray& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        notes.push_back(Json(std::move(note)));
        selectedNote = static_cast<int>(notes.size()) - 1;
        resizingNote = false;
        notifySongChanged();
        const int newContentWidth = contentWidth();
        if (newContentWidth > width()) resize(newContentWidth, height());
        return;
    }

    Json& note = song["tracks"][static_cast<size_t>(trackIndex)]["notes"]
                     [static_cast<size_t>(selectedNote)];
    dragOriginStep = note[0].as_int();
    dragOriginPitch = note[1].as_int();
    dragOriginLength = std::max(1, note[2].as_int());
    dragStartPos = event->pos();
    resizingNote = event->pos().x() >= keyWidth +
        (dragOriginStep + dragOriginLength) * stepWidth - 5;
    update();
}

void PianoRollCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton) || selectedNote < 0 ||
        trackIndex < 0 || !song.contains("tracks") || !song["tracks"].is_array())
        return;
    JsonArray& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
    if (selectedNote >= static_cast<int>(notes.size())) return;
    Json& note = notes[static_cast<size_t>(selectedNote)];
    const int stepDelta = (event->pos().x() - dragStartPos.x()) / stepWidth;
    if (resizingNote) {
        note[2] = std::max(1, dragOriginLength + stepDelta);
    } else {
        note[0] = std::max(0, dragOriginStep + stepDelta);
        const int pitchDelta = (dragStartPos.y() - event->pos().y()) / pitchHeight;
        note[1] = std::clamp(dragOriginPitch + pitchDelta, lowestPitch, highestPitch);
    }
    notifySongChanged();
    const int newContentWidth = contentWidth();
    if (newContentWidth > width()) resize(newContentWidth, height());
}

void PianoRollCanvasWidget::mouseReleaseEvent(QMouseEvent*) {
    resizingNote = false;
}

void PianoRollCanvasWidget::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        selectedNote >= 0 && trackIndex >= 0 && song.contains("tracks") &&
        song["tracks"].is_array()) {
        JsonArray& notes = song["tracks"][static_cast<size_t>(trackIndex)]["notes"].as_array();
        if (selectedNote < static_cast<int>(notes.size())) {
            notes.erase(notes.begin() + selectedNote);
            selectedNote = -1;
            notifySongChanged();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

MusicPanel::MusicPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p), player(new QMediaPlayer(this)),
      audioOutput(new QAudioOutput(this)) {
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.7);

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
                if (pianoRoll) pianoRoll->setVisibleStep(std::max(0, value / stepWidth));
            });
    pianoLayout->addWidget(pianoScroll, 1);
    auto* pianoHint = new QLabel(
        "Piano Roll — click empty grid to add · drag to move or resize · right-click/Delete to remove · 16 steps per bar",
        pianoPage);
    pianoHint->setStyleSheet("color: #aeb5c5; padding: 3px;");
    pianoLayout->addWidget(pianoHint);
    editorTabs->addTab(pianoPage, "Piano Roll");
    splitter->addWidget(editorTabs);
    splitter->setSizes({270, 1300});
    layout->addWidget(splitter, 1);

    pianoRoll->setChangeCallback([this](const Json& song) { saveCurrentSong(song); });
    connect(songCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) selectSong(songCombo->itemData(index).toString());
    });
    connect(newSongButton, &QPushButton::clicked, this, [this]() {
        if (!project) return;
        bool accepted = false;
        const QString name = QInputDialog::getText(this, "New Song", "Song name:",
                                                    QLineEdit::Normal, "New Song", &accepted);
        if (!accepted || name.trimmed().isEmpty()) return;
        const std::string base = songIdFromName(name);
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
        if (Json* song = currentSong()) {
            pianoRoll->setSong(*song, selectedTrack);
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
        tracks.push_back(makeTrack("Track " + std::to_string(tracks.size() + 1),
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
        tracks.push_back(makeTrack("Drums", 9, 0));
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
        pianoRoll->setSong(*song, selectedTrack);
        refreshSessionGrid();
    });
    connect(addSceneButton, &QPushButton::clicked, this, [this]() {
        Json* song = currentSong();
        if (!song) return;
        initializeScenes(*song);
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
    connect(playButton, &QPushButton::clicked, this, [this]() { playPreview(); });
    connect(stopButton, &QPushButton::clicked, this, [this]() { stopPreview(); });
    connect(midiInputButton, &QPushButton::clicked, this,
            [this]() { toggleMidiInput(); });
    connect(midiOutputButton, &QPushButton::clicked, this,
            [this]() { toggleMidiOutput(); });
    connect(refreshMidiButton, &QPushButton::clicked, this,
            [this]() { refreshMidiDevices(); });
    connect(recordMidiButton, &QPushButton::toggled, this,
            [this](bool enabled) { setMidiRecording(enabled); });
    connect(&midiDevices, &MidiDeviceIo::messageReceived, this,
            [this](quint32 message) { receiveMidiMessage(message); });
    midiOutputTimer = new QTimer(this);
    midiOutputTimer->setInterval(2);
    connect(midiOutputTimer, &QTimer::timeout, this,
            [this]() { pumpMidiOutput(); });
    connect(importMidiButton, &QPushButton::clicked, this, [this]() { importMidi(); });
    connect(exportMidiButton, &QPushButton::clicked, this, [this]() { exportMidi(); });
    connect(player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
        playButton->setText(state == QMediaPlayer::PlayingState ? "Playing…" : "Play Preview");
    });
    connect(player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString& message) {
        if (error != QMediaPlayer::NoError)
            QMessageBox::warning(this, "Music Preview Error", message);
    });
    refreshMidiDevices();
    refreshSongs();
}

MusicPanel::~MusicPanel() {
    stopPreview();
    midiDevices.closeInput();
    midiDevices.closeOutput();
}

void MusicPanel::refreshMidiDevices() {
    const int inputId = midiInputCombo->currentData().toInt();
    const int outputId = midiOutputCombo->currentData().toInt();
    const QSignalBlocker inputBlocker(midiInputCombo);
    const QSignalBlocker outputBlocker(midiOutputCombo);
    midiInputCombo->clear();
    midiOutputCombo->clear();
    midiInputCombo->addItem("Select MIDI input…", -1);
    midiOutputCombo->addItem("Select MIDI output…", -1);
    for (const MidiDeviceInfo& device : MidiDeviceIo::inputDevices()) {
        midiInputCombo->addItem(device.name, static_cast<int>(device.id));
    }
    for (const MidiDeviceInfo& device : MidiDeviceIo::outputDevices()) {
        midiOutputCombo->addItem(device.name, static_cast<int>(device.id));
    }
    const int inputIndex = midiInputCombo->findData(inputId);
    const int outputIndex = midiOutputCombo->findData(outputId);
    midiInputCombo->setCurrentIndex(inputIndex >= 0 ? inputIndex : 0);
    midiOutputCombo->setCurrentIndex(outputIndex >= 0 ? outputIndex : 0);
    updateMidiStatus();
}

void MusicPanel::toggleMidiInput() {
    if (midiDevices.inputConnected()) {
        setMidiRecording(false);
        const QString error = midiDevices.closeInput();
        midiInputButton->setText("Connect");
        updateMidiStatus();
        if (!error.isEmpty()) QMessageBox::warning(this, "MIDI Input", error);
        return;
    }
    bool validId = false;
    const uint deviceId = midiInputCombo->currentData().toUInt(&validId);
    if (!validId) {
        QMessageBox::information(this, "MIDI Input", "Connect a MIDI input device first.");
        return;
    }
    const QString error = midiDevices.openInput(deviceId);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "MIDI Input", error);
        return;
    }
    midiInputButton->setText("Disconnect");
    updateMidiStatus();
}

void MusicPanel::toggleMidiOutput() {
    if (midiDevices.outputConnected()) {
        stopMidiOutput();
        const QString error = midiDevices.closeOutput();
        midiOutputButton->setText("Connect");
        updateMidiStatus();
        if (!error.isEmpty()) QMessageBox::warning(this, "MIDI Output", error);
        return;
    }
    bool validId = false;
    const uint deviceId = midiOutputCombo->currentData().toUInt(&validId);
    if (!validId) {
        QMessageBox::information(this, "MIDI Output", "Connect a MIDI output device first.");
        return;
    }
    const QString error = midiDevices.openOutput(deviceId);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "MIDI Output", error);
        return;
    }
    midiOutputButton->setText("Disconnect");
    updateMidiStatus();
}

void MusicPanel::setMidiRecording(bool enabled) {
    if (enabled && (!midiDevices.inputConnected() || !currentSong() ||
                    selectedTrack < 0)) {
        const QSignalBlocker blocker(recordMidiButton);
        recordMidiButton->setChecked(false);
        QMessageBox::information(this, "MIDI Recording",
            "Connect a MIDI input, select a song, and select a track before recording.");
        return;
    }
    midiRecording = enabled;
    if (enabled) {
        heldMidiNotes.clear();
        midiRecordClock.start();
        recordMidiButton->setText("Stop Recording");
    } else {
        flushHeldMidiNotes();
        recordMidiButton->setText("Record MIDI");
    }
    updateMidiStatus();
}

int MusicPanel::midiRecordStep() {
    const Json* song = currentSong();
    const int bpm = song ? clampedInt((*song)["tempo"], 120, 30, 240) : 120;
    const qint64 elapsed = midiRecordClock.isValid() ? midiRecordClock.elapsed() : 0;
    return static_cast<int>(std::clamp<qint64>(
        qRound64(elapsed * bpm * 4.0 / 60000.0), 0, 1000000));
}

void MusicPanel::commitRecordedMidiNote(const HeldMidiNote& note, int endStep) {
    if (!project || !project->music.contains(note.songId.toStdString())) return;
    Json& song = project->music[note.songId.toStdString()];
    if (!song.contains("tracks") || !song["tracks"].is_array() ||
        note.trackIndex < 0 ||
        note.trackIndex >= static_cast<int>(song["tracks"].as_array().size())) return;
    Json& track = song["tracks"][static_cast<size_t>(note.trackIndex)];
    if (!track.contains("notes") || !track["notes"].is_array()) {
        track["notes"] = JsonArray{};
    }
    const int length = std::max(1, endStep - note.startStep);
    track["notes"].as_array().emplace_back(
        JsonArray{note.startStep, note.pitch, length, note.velocity});
    std::stable_sort(track["notes"].as_array().begin(), track["notes"].as_array().end(),
        [](const Json& left, const Json& right) {
            return left.is_array() && right.is_array() &&
                left[0].as_int() < right[0].as_int();
        });
    if (note.songId == currentSongId) {
        refreshTracks();
        pianoRoll->setSong(song, selectedTrack);
        refreshSessionGrid();
    }
}

void MusicPanel::flushHeldMidiNotes() {
    const int endStep = midiRecordStep();
    for (const auto& [key, notes] : heldMidiNotes) {
        Q_UNUSED(key);
        for (const HeldMidiNote& note : notes) commitRecordedMidiNote(note, endStep);
    }
    heldMidiNotes.clear();
}

void MusicPanel::receiveMidiMessage(quint32 message) {
    if (!midiRecording) return;
    const int status = static_cast<int>(message & 0xff);
    const int kind = status & 0xf0;
    if (kind != 0x80 && kind != 0x90) return;
    const int channelNumber = status & 0x0f;
    const int pitch = static_cast<int>((message >> 8) & 0x7f);
    const int velocity = static_cast<int>((message >> 16) & 0x7f);
    const auto key = std::make_pair(channelNumber, pitch);
    const bool noteOn = kind == 0x90 && velocity > 0;
    if (noteOn) {
        if (selectedTrack < 0 || !currentSong()) return;
        heldMidiNotes[key].push_back({
            currentSongId, selectedTrack, midiRecordStep(), pitch, velocity
        });
        return;
    }
    auto held = heldMidiNotes.find(key);
    if (held == heldMidiNotes.end() || held->second.empty()) return;
    const HeldMidiNote note = held->second.back();
    held->second.pop_back();
    if (held->second.empty()) heldMidiNotes.erase(held);
    commitRecordedMidiNote(note, midiRecordStep());
}

void MusicPanel::updateMidiStatus() {
    QStringList connections;
    if (midiDevices.inputConnected()) {
        connections.push_back("In: " + midiInputCombo->currentText());
    }
    if (midiDevices.outputConnected()) {
        connections.push_back("Out: " + midiOutputCombo->currentText());
    }
    if (midiRecording) connections.push_back("Recording");
    midiStatusLabel->setText(connections.isEmpty()
        ? "No MIDI devices connected" : connections.join(" · "));
}

QString MusicPanel::sendMidiMessage(quint32 message) {
    return midiDevices.sendShortMessage(message);
}

void MusicPanel::startMidiOutput(const Json& song) {
    stopMidiOutput();
    if (!midiDevices.outputConnected() || !song.contains("tracks") ||
        !song["tracks"].is_array()) return;
    midiOutputSongId = currentSongId;
    midiPlaybackEvents.clear();
    midiPlaybackIteration = 0;
    nextMidiPlaybackEvent = 0;
    midiOutputFailureShown = false;
    const int bpm = clampedInt(song["tempo"], 120, 30, 240);
    const double millisecondsPerStep = 60000.0 / (bpm * 4.0);
    int totalSteps = std::clamp(clampedInt(song["bars"], 8, 1, 128) * 16, 16, 4096);
    const auto message = [](int status, int data1, int data2 = 0) {
        return static_cast<quint32>((status & 0xff) | ((data1 & 0x7f) << 8) |
                                    ((data2 & 0x7f) << 16));
    };
    for (size_t trackIndex = 0; trackIndex < song["tracks"].as_array().size(); ++trackIndex) {
        const Json& track = song["tracks"][trackIndex];
        const int channelNumber = clampedInt(
            track["channel"], static_cast<int>(trackIndex % 16), 0, 15);
        const int bankValue = clampedInt(track["bank"], 0, 0, 16383);
        const int programValue = clampedInt(track["program"], 0, 0, 127);
        const int volumeValue = clampedInt(track["volume"], 90, 0, 127);
        const int panValue = clampedInt(track["pan"], 64, 0, 127);
        const int channelStatus = channelNumber;
        midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 0,
                                                    bankValue / 128)});
        midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 32,
                                                    bankValue % 128)});
        midiPlaybackEvents.push_back({0, 0, message(0xc0 | channelStatus, programValue)});
        midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 7, volumeValue)});
        midiPlaybackEvents.push_back({0, 0, message(0xb0 | channelStatus, 10, panValue)});
        if (!track.contains("notes") || !track["notes"].is_array()) continue;
        for (const Json& note : track["notes"].as_array()) {
            if (!note.is_array() || note.as_array().size() < 3) continue;
            const int startStep = std::clamp(note[0].as_int(), 0, 4095);
            const int pitch = std::clamp(note[1].as_int(), 0, 127);
            const int velocity = note.as_array().size() >= 4
                ? std::clamp(note[3].as_int(), 0, 127) : 90;
            const int length = std::clamp(note[2].as_int(), 1, 4096);
            const int endStep = std::min(4096, startStep + length);
            totalSteps = std::max(totalSteps, endStep);
            midiPlaybackEvents.push_back({
                qRound64(startStep * millisecondsPerStep), 2,
                message(0x90 | channelStatus, pitch, velocity)
            });
            midiPlaybackEvents.push_back({
                qRound64(endStep * millisecondsPerStep), 1,
                message(0x80 | channelStatus, pitch)
            });
        }
    }
    midiLoopDurationMs = std::clamp<qint64>(
        qRound64(totalSteps * millisecondsPerStep), 1, 180000);
    for (MidiPlaybackEvent& event : midiPlaybackEvents) {
        event.timeMs = std::clamp<qint64>(event.timeMs, 0, midiLoopDurationMs - 1);
    }
    std::stable_sort(midiPlaybackEvents.begin(), midiPlaybackEvents.end(),
        [](const MidiPlaybackEvent& left, const MidiPlaybackEvent& right) {
            if (left.timeMs != right.timeMs) return left.timeMs < right.timeMs;
            return left.priority < right.priority;
        });
    midiPlaybackClock.start();
    midiOutputTimer->start();
}

void MusicPanel::pumpMidiOutput() {
    if (!midiDevices.outputConnected() || midiPlaybackEvents.empty() ||
        midiLoopDurationMs <= 0) {
        midiOutputTimer->stop();
        return;
    }
    const qint64 now = midiPlaybackClock.elapsed();
    while (true) {
        if (nextMidiPlaybackEvent >= midiPlaybackEvents.size()) {
            ++midiPlaybackIteration;
            nextMidiPlaybackEvent = 0;
        }
        const MidiPlaybackEvent& event = midiPlaybackEvents[nextMidiPlaybackEvent];
        const qint64 due = midiPlaybackIteration * midiLoopDurationMs + event.timeMs;
        if (now < due) return;
        const QString error = sendMidiMessage(event.message);
        if (!error.isEmpty()) {
            midiOutputTimer->stop();
            midiPlaybackEvents.clear();
            if (!midiOutputFailureShown) {
                midiOutputFailureShown = true;
                QMessageBox::warning(this, "MIDI Output", error);
            }
            return;
        }
        ++nextMidiPlaybackEvent;
    }
}

void MusicPanel::stopMidiOutput() {
    midiOutputTimer->stop();
    midiPlaybackEvents.clear();
    nextMidiPlaybackEvent = 0;
    midiLoopDurationMs = 0;
    if (!midiDevices.outputConnected()) return;
    for (int channelNumber = 0; channelNumber < 16; ++channelNumber) {
        const quint32 allNotesOff = static_cast<quint32>(
            0xb0 | channelNumber | (123 << 8));
        const QString error = sendMidiMessage(allNotesOff);
        if (!error.isEmpty()) {
            if (!midiOutputFailureShown) {
                midiOutputFailureShown = true;
                QMessageBox::warning(this, "MIDI Output", error);
            }
            break;
        }
    }
}

Json* MusicPanel::currentSong() {
    if (!project || currentSongId.isEmpty()) return nullptr;
    auto it = project->music.find(currentSongId.toStdString());
    return it == project->music.end() ? nullptr : &it->second;
}

void MusicPanel::saveCurrentSong(const Json& song) {
    if (!project || currentSongId.isEmpty()) return;
    project->music[currentSongId.toStdString()] = song;
    if (selectedTrack >= 0 && selectedTrack < static_cast<int>(song["tracks"].as_array().size())) {
        const Json& track = song["tracks"][static_cast<size_t>(selectedTrack)];
        const QString name = track.contains("name")
            ? QString::fromStdString(track["name"].as_string())
            : QString("Track %1").arg(selectedTrack + 1);
        const int notes = track.contains("notes") && track["notes"].is_array()
            ? static_cast<int>(track["notes"].as_array().size()) : 0;
        if (QListWidgetItem* item = trackList->item(selectedTrack))
            item->setText(QString("%1  ·  %2 notes").arg(name).arg(notes));
    }
}

void MusicPanel::setProject(Project* p) {
    project = p;
    currentSongId.clear();
    refreshSongs();
}

void MusicPanel::refreshSongs() {
    const QSignalBlocker blocker(songCombo);
    songCombo->clear();
    if (project) {
        for (const auto& [id, song] : project->music) {
            const QString qid = QString::fromStdString(id);
            const QString name = song.contains("name")
                ? QString::fromStdString(song["name"].as_string()) : qid;
            songCombo->addItem(name + " (" + qid + ")", qid);
        }
    }
    if (!currentSongId.isEmpty() && songCombo->findData(currentSongId) >= 0)
        songCombo->setCurrentIndex(songCombo->findData(currentSongId));
    else if (songCombo->count() > 0) {
        songCombo->setCurrentIndex(0);
        currentSongId = songCombo->itemData(0).toString();
        selectSong(currentSongId);
    } else {
        currentSongId.clear();
        selectedTrack = -1;
        selectedScene = -1;
        activeScene = -1;
        trackList->clear();
        pianoRoll->setSong(Json{}, -1);
        refreshSessionGrid();
        songName->clear();
        tempo->setValue(120);
        previewSynth->setCurrentIndex(0);
    }
}

void MusicPanel::selectSong(const QString& id) {
    currentSongId = id;
    Json* song = currentSong();
    if (!song) return;
    selectedScene = 0;
    activeScene = -1;
    updatingControls = true;
    songName->setText((*song).contains("name")
        ? QString::fromStdString((*song)["name"].as_string()) : id);
    tempo->setValue(clampedInt((*song)["tempo"], 120, 30, 240));
    const QString synthId = (*song)["preview_synth"].is_string()
        ? QString::fromStdString((*song)["preview_synth"].as_string()) : "gm";
    previewSynth->setCurrentIndex(std::max(0, previewSynth->findData(synthId)));
    selectedTrack = 0;
    refreshTracks();
    trackList->setCurrentRow(selectedTrack);
    pianoRoll->setSong(*song, selectedTrack);
    updateTrackProperties();
    updatingControls = false;
}

void MusicPanel::refreshTracks() {
    const QSignalBlocker blocker(trackList);
    trackList->clear();
    Json* song = currentSong();
    if (!song || !(*song).contains("tracks") || !(*song)["tracks"].is_array()) {
        selectedTrack = -1;
        pianoRoll->setSong(song ? *song : Json{}, -1);
        refreshSessionGrid();
        return;
    }
    const JsonArray& tracks = (*song)["tracks"].as_array();
    for (size_t i = 0; i < tracks.size(); ++i) {
        const Json& track = tracks[i];
        const QString name = track.contains("name")
            ? QString::fromStdString(track["name"].as_string())
            : QString("Track %1").arg(i + 1);
        const int noteCount = track.contains("notes") && track["notes"].is_array()
            ? static_cast<int>(track["notes"].as_array().size()) : 0;
        trackList->addItem(QString("%1  ·  %2 notes").arg(name).arg(noteCount));
    }
    selectedTrack = tracks.empty() ? -1 :
        std::clamp(selectedTrack, 0, static_cast<int>(tracks.size()) - 1);
    refreshSessionGrid();
}

void MusicPanel::refreshSessionGrid() {
    if (!sessionGrid) return;
    Json* song = currentSong();
    if (!song || !song->contains("tracks") || !(*song)["tracks"].is_array()) {
        sessionGrid->clear();
        sessionGrid->setRowCount(0);
        sessionGrid->setColumnCount(0);
        selectedScene = -1;
        return;
    }

    initializeScenes(*song);
    JsonArray& scenes = (*song)["scenes"].as_array();
    const JsonArray& tracks = (*song)["tracks"].as_array();
    for (Json& scene : scenes) {
        if (!scene.contains("clips") || !scene["clips"].is_array()) scene["clips"] = JsonArray{};
        scene["clips"].as_array().resize(tracks.size());
    }
    sessionGrid->clear();
    sessionGrid->setRowCount(static_cast<int>(tracks.size()));
    sessionGrid->setColumnCount(static_cast<int>(scenes.size()));
    QStringList trackNames;
    for (size_t row = 0; row < tracks.size(); ++row) {
        const Json& track = tracks[row];
        const std::string name = track.contains("name") && track["name"].is_string()
            ? track["name"].as_string() : "Track " + std::to_string(row + 1);
        trackNames.push_back(QString::fromStdString(name));
    }
    QStringList sceneNames;
    for (size_t column = 0; column < scenes.size(); ++column) {
        const Json& scene = scenes[column];
        const std::string name = scene.contains("name") && scene["name"].is_string()
            ? scene["name"].as_string() : "Scene " + std::to_string(column + 1);
        sceneNames.push_back(QString::fromStdString(name));
    }
    sessionGrid->setVerticalHeaderLabels(trackNames);
    sessionGrid->setHorizontalHeaderLabels(sceneNames);

    for (int row = 0; row < sessionGrid->rowCount(); ++row) {
        for (int column = 0; column < sessionGrid->columnCount(); ++column) {
            const Json& clip = scenes[static_cast<size_t>(column)]["clips"][static_cast<size_t>(row)];
            const bool hasClip = !clip.is_null();
            const int noteCount = hasClip && clip.contains("notes") && clip["notes"].is_array()
                ? static_cast<int>(clip["notes"].as_array().size()) : 0;
            auto* button = new QPushButton(
                hasClip ? QString("●  %1 notes").arg(noteCount) : QString("+"),
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
            connect(button, &QPushButton::clicked, this, [this, row, column]() {
                QTimer::singleShot(0, this, [this, row, column]() {
                    if (row >= 0 && row < trackList->count()) trackList->setCurrentRow(row);
                    toggleClip(column, row);
                });
            });
            sessionGrid->setCellWidget(row, column, button);
        }
    }
    selectedScene = scenes.empty() ? -1 : std::clamp(selectedScene, 0,
                                                      static_cast<int>(scenes.size()) - 1);
    if (activeScene >= static_cast<int>(scenes.size())) activeScene = -1;
}

void MusicPanel::toggleClip(int sceneIndex, int trackIndex) {
    Json* song = currentSong();
    if (!song || sceneIndex < 0 || trackIndex < 0 ||
        !song->contains("tracks") || !(*song)["tracks"].is_array() ||
        trackIndex >= static_cast<int>((*song)["tracks"].as_array().size())) return;
    initializeScenes(*song);
    JsonArray& scenes = (*song)["scenes"].as_array();
    if (sceneIndex >= static_cast<int>(scenes.size())) return;
    Json& scene = scenes[static_cast<size_t>(sceneIndex)];
    JsonArray& clips = scene["clips"].as_array();
    clips.resize((*song)["tracks"].as_array().size());
    Json& clip = clips[static_cast<size_t>(trackIndex)];
    if (clip.is_null()) {
        const Json& track = (*song)["tracks"][static_cast<size_t>(trackIndex)];
        JsonArray clipNotes = notesForBar(track, sceneIndex);
        clip = JsonObject{{"notes", clipNotes}};
        selectedScene = sceneIndex;
        activeScene = -1;
    } else {
        selectedScene = sceneIndex;
        activeScene = sceneIndex;
    }
    refreshSessionGrid();
}

Json MusicPanel::makeScenePreview(int sceneIndex) {
    const Json* source = currentSong();
    if (!source || sceneIndex < 0 || !source->contains("scenes") ||
        !(*source)["scenes"].is_array() ||
        sceneIndex >= static_cast<int>((*source)["scenes"].as_array().size()) ||
        !source->contains("tracks") || !(*source)["tracks"].is_array()) return {};

    Json preview = *source;
    JsonArray& tracks = preview["tracks"].as_array();
    const Json& scene = (*source)["scenes"][static_cast<size_t>(sceneIndex)];
    for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        JsonArray notes;
        if (scene.contains("clips") && scene["clips"].is_array() &&
            trackIndex < scene["clips"].as_array().size()) {
            const Json& clip = scene["clips"][trackIndex];
            if (!clip.is_null() && clip.contains("notes") && clip["notes"].is_array()) {
                notes = clip["notes"].as_array();
            }
        }
        tracks[trackIndex]["notes"] = notes;
    }
    preview["bars"] = 1;
    return preview;
}

void MusicPanel::updateTrackProperties() {
    Json* song = currentSong();
    const bool valid = song && selectedTrack >= 0 && (*song).contains("tracks") &&
                       selectedTrack < static_cast<int>((*song)["tracks"].as_array().size());
    updatingControls = true;
    instrument->setEnabled(valid);
    bank->setEnabled(valid);
    channel->setEnabled(valid);
    volume->setEnabled(valid);
    pan->setEnabled(valid);
    if (valid) {
        const Json& track = (*song)["tracks"][static_cast<size_t>(selectedTrack)];
        instrument->setCurrentIndex(instrument->findData(
            clampedInt(track["program"], 0, 0, 127)));
        bank->setValue(clampedInt(track["bank"], 0, 0, 16383));
        channel->setValue(clampedInt(track["channel"], selectedTrack % 16, 0, 15) + 1);
        volume->setValue(clampedInt(track["volume"], 90, 0, 127));
        pan->setValue(clampedInt(track["pan"], 64, 0, 127));
    }
    updatingControls = false;
}

void MusicPanel::updateTrackProperty(const char* key, int value) {
    if (updatingControls) return;
    Json* song = currentSong();
    if (!song || selectedTrack < 0 ||
        selectedTrack >= static_cast<int>((*song)["tracks"].as_array().size())) return;
    (*song)["tracks"][static_cast<size_t>(selectedTrack)][key] = value;
    refreshTracks();
}

void MusicPanel::generateQuickSong(const std::string& style) {
    if (!project) return;
    Json song = JsonObject{
        {"name", style + " Theme"},
        {"tempo", style == "Battle" ? 140 : style == "Village" ? 96 : 110},
        {"bars", 8}, {"beats", 4}, {"loop_bar", 0}, {"preview_synth", "gm"}
    };

    const int root = style == "Battle" ? 50 : style == "Village" ? 60 : 62;
    Json lead = makeTrack("Melody", 0, 80);
    JsonArray melody;
    const int melodySteps[] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60};
    const int melodyIntervals[] = {0, 2, 4, 7, 4, 2, 5, 7, 9, 7, 5, 4, 2, 4, 7, 2};
    for (int i = 0; i < 16; ++i)
        melody.push_back(JsonArray{melodySteps[i], root + melodyIntervals[i], 3, 92});
    lead["notes"] = melody;

    Json bass = makeTrack("Bass", 1, 38);
    JsonArray bassNotes;
    const int roots[] = {root - 24, root - 19, root - 17, root - 22};
    for (int bar = 0; bar < 8; ++bar) {
        for (int beat = 0; beat < 4; ++beat)
            bassNotes.push_back(JsonArray{bar * 16 + beat * 4, roots[bar % 4], 3, 78});
    }
    bass["notes"] = bassNotes;

    Json chords = makeTrack("Harmony", 2, 48);
    JsonArray chordNotes;
    for (int bar = 0; bar < 8; ++bar) {
        const int chordRoot = roots[bar % 4] + 24;
        chordNotes.push_back(JsonArray{bar * 16, chordRoot, 16, 58});
        chordNotes.push_back(JsonArray{bar * 16, chordRoot + 4, 16, 58});
        chordNotes.push_back(JsonArray{bar * 16, chordRoot + 7, 16, 58});
    }
    chords["notes"] = chordNotes;

    Json drums = makeTrack("Drums", 9, 0);
    JsonArray drumNotes;
    for (int bar = 0; bar < 8; ++bar) {
        for (int beat = 0; beat < 4; ++beat) {
            const int step = bar * 16 + beat * 4;
            if (beat == 0 || beat == 2) drumNotes.push_back(JsonArray{step, 36, 1, 92});
            if (beat == 1 || beat == 3) drumNotes.push_back(JsonArray{step, 38, 1, 82});
            drumNotes.push_back(JsonArray{step, 42, 1, 52});
            drumNotes.push_back(JsonArray{step + 2, 42, 1, 38});
        }
    }
    drums["notes"] = drumNotes;
    song["tracks"] = JsonArray{lead, bass, chords, drums};

    const std::string base = songIdFromName(QString::fromStdString(style + " Theme"));
    std::string id = base;
    int suffix = 2;
    while (project->music.contains(id)) id = base + "_" + std::to_string(suffix++);
    project->music[id] = std::move(song);
    refreshSongs();
    selectSong(QString::fromStdString(id));
}

void MusicPanel::playPreview(const Json* previewSong) {
    const Json* selectedSong = previewSong ? previewSong : currentSong();
    if (!selectedSong || !selectedSong->contains("tracks") ||
        !(*selectedSong)["tracks"].is_array()) {
        QMessageBox::information(this, "Music Preview", "Create a song and add notes before previewing.");
        return;
    }
    if (!previewDirectory.isValid()) {
        QMessageBox::warning(this, "Music Preview", "Could not create temporary preview storage.");
        return;
    }

    const std::string synthId = (*selectedSong)["preview_synth"].is_string()
        ? (*selectedSong)["preview_synth"].as_string() : "gm";
    const PreviewSynth synth = synthId == "retro_psg" ? PreviewSynth::RetroPsg
        : synthId == "fm" ? PreviewSynth::Fm : PreviewSynth::Gm;
    StereoPcm pcm;
    try {
        pcm = synthesize_preview(*selectedSong, synth, sampleRate, 180);
    } catch (const std::exception& error) {
        QMessageBox::information(this, "Music Preview", QString::fromStdString(error.what()));
        return;
    }

    QByteArray wav;
    const int totalFrames = static_cast<int>(pcm.samples.size() / 2);
    wav.reserve(44 + totalFrames * 4);
    wav.append("RIFF", 4);
    appendU32(wav, static_cast<std::uint32_t>(36 + totalFrames * 4));
    wav.append("WAVEfmt ", 8);
    appendU32(wav, 16);
    appendU16(wav, 1);
    appendU16(wav, 2);
    appendU32(wav, static_cast<std::uint32_t>(pcm.sample_rate));
    appendU32(wav, static_cast<std::uint32_t>(pcm.sample_rate * 4));
    appendU16(wav, 4);
    appendU16(wav, 16);
    wav.append("data", 4);
    appendU32(wav, static_cast<std::uint32_t>(totalFrames * 4));
    for (int16_t sample : pcm.samples) appendU16(wav, static_cast<uint16_t>(sample));

    stopPreview();
    const QString filePath = previewDirectory.filePath(
        QString("music-preview-%1.wav").arg(++previewCount));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        file.write(wav) != wav.size() || !file.flush()) {
        QMessageBox::warning(this, "Music Preview", "Could not write the temporary audio preview.");
        return;
    }
    file.close();
    player->setSource(QUrl::fromLocalFile(filePath));
    player->setLoops(QMediaPlayer::Infinite);
    player->play();
    startMidiOutput(*selectedSong);
}

void MusicPanel::stopPreview() {
    if (player) player->stop();
    stopMidiOutput();
}

void MusicPanel::importMidi() {
    if (!project) return;
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Standard MIDI File", {}, "MIDI files (*.mid *.midi)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "MIDI Import", file.errorString());
        return;
    }
    const QByteArray bytes = file.readAll();
    std::vector<uint8_t> midiBytes;
    midiBytes.reserve(static_cast<size_t>(bytes.size()));
    for (char byte : bytes) midiBytes.push_back(static_cast<uint8_t>(byte));
    try {
        Json song = import_standard_midi(
            midiBytes, QFileInfo(path).completeBaseName().toStdString());
        const std::string base = songIdFromName(
            QString::fromStdString(song["name"].as_string()));
        std::string id = base;
        int suffix = 2;
        while (project->music.contains(id)) id = base + "_" + std::to_string(suffix++);
        project->music[id] = std::move(song);
        refreshSongs();
        selectSong(QString::fromStdString(id));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "MIDI Import", QString::fromStdString(error.what()));
    }
}

void MusicPanel::exportMidi() {
    const Json* song = currentSong();
    if (!song) {
        QMessageBox::information(this, "MIDI Export", "Select a song to export.");
        return;
    }
    const QString title = (*song)["name"].is_string()
        ? QString::fromStdString((*song)["name"].as_string()) : currentSongId;
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Standard MIDI File", title + ".mid", "MIDI files (*.mid)");
    if (path.isEmpty()) return;

    try {
        const std::vector<uint8_t> data = export_standard_midi(*song);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, "MIDI Export", file.errorString());
            return;
        }
        const qint64 written = file.write(
            reinterpret_cast<const char*>(data.data()), static_cast<qint64>(data.size()));
        if (written != static_cast<qint64>(data.size())) {
            QMessageBox::warning(this, "MIDI Export",
                file.errorString().isEmpty() ? "Could not write the complete MIDI file."
                                             : file.errorString());
        }
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "MIDI Export", QString::fromStdString(error.what()));
    }
}

} // namespace aur
