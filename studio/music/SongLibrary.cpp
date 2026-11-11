#include "SongLibrary.hpp"

#include <algorithm>
#include <array>

#include <cstdint>

namespace aur
{
    namespace music
    {

        Json makeTrack(const std::string &name, int channel, int program)
        {
            return JsonObject{
                {"name", name}, {"channel", channel}, {"program", program}, {"bank", 0}, {"volume", 90}, {"pan", 64}, {"reverb", 0}, {"chorus", 0}, {"drum", channel == 9}, {"notes", JsonArray{}}};
        }

        JsonArray notesForBar(const Json &track, int bar)
        {
            JsonArray clipNotes;
            if (!track.contains("notes") || !track["notes"].is_array())
                return clipNotes;
            const int barStart = bar * 16;
            const int barEnd = barStart + 16;
            for (const Json &note : track["notes"].as_array())
            {
                if (!note.is_array() || note.as_array().size() < 3)
                    continue;
                const int start = note[0].as_int();
                const int end = start + std::max(1, note[2].as_int());
                const int overlapStart = std::max(start, barStart);
                const int overlapEnd = std::min(end, barEnd);
                if (overlapStart >= overlapEnd)
                    continue;
                JsonArray clipNote{overlapStart - barStart, note[1], overlapEnd - overlapStart};
                if (note.as_array().size() >= 4)
                    clipNote.emplace_back(note[3]);
                clipNotes.emplace_back(std::move(clipNote));
            }
            return clipNotes;
        }

        void initializeScenes(Json &song)
        {
            if (song.contains("scenes") && song["scenes"].is_array())
                return;
            const JsonArray &tracks = song.contains("tracks") && song["tracks"].is_array()
                                          ? song["tracks"].as_array()
                                          : JsonArray{};
            const int bars = song.contains("bars") && song["bars"].is_number()
                                 ? std::clamp(song["bars"].as_int(), 1, 128)
                                 : 8;
            JsonArray scenes;
            for (int bar = 0; bar < bars; ++bar)
            {
                JsonArray clips(tracks.size(), Json{});
                for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
                {
                    JsonArray clipNotes = notesForBar(tracks[trackIndex], bar);
                    if (!clipNotes.empty())
                        clips[trackIndex] = JsonObject{{"notes", clipNotes}};
                }
                scenes.emplace_back(JsonObject{
                    {"name", "Scene " + std::to_string(bar + 1)}, {"clips", clips}});
            }
            song["scenes"] = scenes;
        }

        std::string songIdFromName(const QString &name)
        {
            std::string id;
            for (const QChar ch : name.trimmed().toLower())
            {
                if (ch.isLetterOrNumber())
                    id.push_back(ch.toLatin1());
                else if (!id.empty() && id.back() != '_')
                    id.push_back('_');
            }
            while (!id.empty() && id.back() == '_')
                id.pop_back();
            return id.empty() ? "song" : id;
        }

        Json quickSong(const std::string &style)
        {
            Json song = JsonObject{
                {"name", style + " Theme"},
                {"tempo", style == "Battle" ? 140 : style == "Village" ? 96
                                                                       : 110},
                {"bars", 8},
                {"beats", 4},
                {"loop_bar", 0},
                {"preview_synth", "md_fm"}};

            const int root = style == "Battle" ? 50 : style == "Village" ? 60
                                                                         : 62;
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
            for (int bar = 0; bar < 8; ++bar)
            {
                for (int beat = 0; beat < 4; ++beat)
                    bassNotes.push_back(JsonArray{bar * 16 + beat * 4, roots[bar % 4], 3, 78});
            }
            bass["notes"] = bassNotes;

            Json chords = makeTrack("Harmony", 2, 48);
            JsonArray chordNotes;
            for (int bar = 0; bar < 8; ++bar)
            {
                const int chordRoot = roots[bar % 4] + 24;
                chordNotes.push_back(JsonArray{bar * 16, chordRoot, 16, 58});
                chordNotes.push_back(JsonArray{bar * 16, chordRoot + 4, 16, 58});
                chordNotes.push_back(JsonArray{bar * 16, chordRoot + 7, 16, 58});
            }
            chords["notes"] = chordNotes;

            Json drums = makeTrack("Drums", 9, 0);
            JsonArray drumNotes;
            for (int bar = 0; bar < 8; ++bar)
            {
                for (int beat = 0; beat < 4; ++beat)
                {
                    const int step = bar * 16 + beat * 4;
                    if (beat == 0 || beat == 2)
                        drumNotes.push_back(JsonArray{step, 36, 1, 92});
                    if (beat == 1 || beat == 3)
                        drumNotes.push_back(JsonArray{step, 38, 1, 82});
                    drumNotes.push_back(JsonArray{step, 42, 1, 52});
                    drumNotes.push_back(JsonArray{step + 2, 42, 1, 38});
                }
            }
            drums["notes"] = drumNotes;
            song["tracks"] = JsonArray{lead, bass, chords, drums};
            return song;
        }

        namespace
        {

            void appendU16(QByteArray &data, std::uint16_t value)
            {
                data.append(static_cast<char>(value & 0xff));
                data.append(static_cast<char>((value >> 8) & 0xff));
            }

            void appendU32(QByteArray &data, std::uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                    data.append(static_cast<char>((value >> shift) & 0xff));
            }

        } // namespace

        QByteArray stereoPcmToWav(const StereoPcm &pcm)
        {
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
            for (std::int16_t sample : pcm.samples)
                appendU16(wav, static_cast<std::uint16_t>(sample));
            return wav;
        }

    } // namespace music
} // namespace aur
