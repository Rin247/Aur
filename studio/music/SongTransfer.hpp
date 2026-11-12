#pragma once

// SongTransfer.hpp - the DAW's MIDI files in and out.
//
// A move out of the panel: the transfer is a file concern, not a panel
// concern, and the panel was over its size limit.

#include <QString>
#include <functional>
#include <string>

#include "aur/json.hpp"

class QWidget;

namespace aur
{

    class Project;

    namespace music
    {

        // Reads a standard MIDI file into the project and reports the song
        // id it made.
        void importMidiFile(QWidget *parent, Project *project,
                            const std::function<void(const std::string &)> &onImported);

        // Writes a song as a standard MIDI file.
        void exportMidiFile(QWidget *parent, const Json &song, const QString &songId);

        // Writes an arrangement (the flattened song) as a standard MIDI file.
        void exportArrangementMidiFile(QWidget *parent, const Json &flat, const QString &name);

    } // namespace music

} // namespace aur
