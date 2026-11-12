#include "SongTransfer.hpp"

#include "SongLibrary.hpp"

#include "aur/audio/midi.hpp"

#include "aur/project.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QWidget>

namespace aur
{
    namespace music
    {

        namespace
        {
            void writeMidi(QWidget *parent, const QString &dialogTitle,
                           const QString &defaultName, const QString &messageTitle,
                           const Json &song)
            {
                const QString path = QFileDialog::getSaveFileName(
                    parent, dialogTitle, defaultName, "MIDI files (*.mid)");
                if (path.isEmpty())
                    return;
                try
                {
                    const std::vector<uint8_t> data = export_standard_midi(song);
                    QFile file(path);
                    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    {
                        QMessageBox::warning(parent, messageTitle, file.errorString());
                        return;
                    }
                    const qint64 written = file.write(
                        reinterpret_cast<const char *>(data.data()),
                        static_cast<qint64>(data.size()));
                    if (written != static_cast<qint64>(data.size()))
                    {
                        QMessageBox::warning(parent, messageTitle,
                                             file.errorString().isEmpty()
                                                 ? "Could not write the complete MIDI file."
                                                 : file.errorString());
                    }
                }
                catch (const std::exception &error)
                {
                    QMessageBox::warning(parent, messageTitle,
                                         QString::fromStdString(error.what()));
                }
            }
        } // namespace

        void importMidiFile(QWidget *parent, Project *project,
                            const std::function<void(const std::string &)> &onImported)
        {
            if (!project)
                return;
            const QString path = QFileDialog::getOpenFileName(
                parent, "Import Standard MIDI File", {}, "MIDI files (*.mid *.midi)");
            if (path.isEmpty())
                return;

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                QMessageBox::warning(parent, "MIDI Import", file.errorString());
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
                const std::string base = songIdFromName(
                    QString::fromStdString(song["name"].as_string()));
                std::string id = base;
                int suffix = 2;
                while (project->music.contains(id))
                    id = base + "_" + std::to_string(suffix++);
                project->music[id] = std::move(song);
                if (onImported)
                    onImported(id);
            }
            catch (const std::exception &error)
            {
                QMessageBox::warning(parent, "MIDI Import", QString::fromStdString(error.what()));
            }
        }

        void exportMidiFile(QWidget *parent, const Json &song, const QString &songId)
        {
            const QString title = song["name"].is_string()
                                      ? QString::fromStdString(song["name"].as_string())
                                      : songId;
            writeMidi(parent, "Export Standard MIDI File", title + ".mid", "MIDI Export", song);
        }

        void exportArrangementMidiFile(QWidget *parent, const Json &flat, const QString &name)
        {
            writeMidi(parent, "Export Arrangement MIDI", name + " arrangement.mid",
                      "Arrangement MIDI", flat);
        }

    } // namespace music
} // namespace aur
