#include "Sf2Fixture.hpp"

#include <cmath>

#include <cstring>

#include <functional>

#include <string>

namespace aur::testsupport
{
    namespace
    {
        // Little-endian writers. SF2 is LE throughout.
        void put16(std::vector<std::uint8_t> &out, int value)
        {
            out.push_back(static_cast<std::uint8_t>(value & 0xff));
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        }

        void put32(std::vector<std::uint8_t> &out, std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xff));
        }

        void put_tag(std::vector<std::uint8_t> &out, const char (&tag)[5])
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(static_cast<std::uint8_t>(tag[i]));
        }

        // Emits a RIFF chunk with a placeholder size, calls body, then patches
        // the size in place.
        void chunk(std::vector<std::uint8_t> &out, const char (&id)[5],
                   const std::function<void()> &body)
        {
            put_tag(out, id);
            const size_t sizePos = out.size();
            put32(out, 0);
            const size_t startPos = out.size();
            body();
            const std::uint32_t size = static_cast<std::uint32_t>(out.size() - startPos);
            out[sizePos] = static_cast<std::uint8_t>(size & 0xff);
            out[sizePos + 1] = static_cast<std::uint8_t>((size >> 8) & 0xff);
            out[sizePos + 2] = static_cast<std::uint8_t>((size >> 16) & 0xff);
            out[sizePos + 3] = static_cast<std::uint8_t>((size >> 24) & 0xff);
        }

        void bag_record(std::vector<std::uint8_t> &out, std::uint16_t genIndex)
        {
            put16(out, genIndex);
            put16(out, 0); // modIndex
        }

        // SF2 name fields are 20 bytes, NUL-padded (20 for phdr/inst, 20 for shdr too).
        void put_name(std::vector<std::uint8_t> &out, const std::string &name)
        {
            for (int i = 0; i < 20; ++i)
                out.push_back(i < static_cast<int>(name.size())
                                  ? static_cast<std::uint8_t>(name[i])
                                  : 0);
        }

        // A modulator record is 10 bytes; the terminal record is all zeros.
        void mod_record(std::vector<std::uint8_t> &out)
        {
            put16(out, 0);
            put16(out, 0);
            put16(out, 0);
            put16(out, 0);
            put16(out, 0);
        }

        // One zone generator record.
        void gen_record(std::vector<std::uint8_t> &out, std::uint16_t oper, int amount)
        {
            put16(out, oper);
            put16(out, static_cast<std::uint16_t>(amount));
        }

        // Range generators pack lo|hi in ONE u16 (lo = low byte, hi = high).
        void gen_range(std::vector<std::uint8_t> &out, std::uint16_t oper, int lo, int hi)
        {
            put16(out, oper);
            put16(out, (lo & 0x7f) | ((hi & 0x7f) << 8));
        }

    } // namespace

    std::vector<std::uint8_t> make_test_bank()
    {
        std::vector<std::uint8_t> out;
        chunk(out, "RIFF", [&] {
            put_tag(out, "sfbk");

            chunk(out, "LIST", [&] {
                put_tag(out, "INFO");
                chunk(out, "ifil", [&] { put16(out, 2); put16(out, 1); }); // SF2.01
                chunk(out, "INAM", [&] {
                    for (const char c : std::string("Aur Test Bank"))
                        out.push_back(static_cast<std::uint8_t>(c));
                    out.push_back(0);
                });
            });

            chunk(out, "LIST", [&] {
                put_tag(out, "sdta");
                chunk(out, "smpl", [&] {
                    // A 1 kHz-ish sine tuned to root key 60; a loop covers the
                    // whole sample so sustain is steady.
                    const std::uint32_t frames = kTestSampleFrames;
                    for (std::uint32_t i = 0; i < frames; ++i)
                    {
                        const double t = static_cast<double>(i) / kTestSampleRate;
                        const double v = 0.8 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t);
                        put16(out, static_cast<int>(v * 32000.0));
                    }
                    // SF2 sample data carries 46 trailing zero frames.
                    for (int i = 0; 46 > i; ++i)
                        put16(out, 0);
                    put16(out, 0);
                });
            });

            chunk(out, "LIST", [&] {
                put_tag(out, "pdta");
                chunk(out, "phdr", [&] {
                    // Preset 0 "Sine" (bank 0, program 0) + terminal EOP record.
                    put_name(out, "Sine");
                    put16(out, 0);  // preset
                    put16(out, 0);  // bank
                    put16(out, 0);  // pbagIndex
                    put32(out, 0);  // library
                    put32(out, 0);  // genre
                    put32(out, 0);  // morphology
                    put_name(out, "EOD");
                    put16(out, 0);  // preset
                    put16(out, 0);  // bank
                    put16(out, 1);  // pbagIndex (terminal, no zones)
                    put32(out, 0);
                    put32(out, 0);
                    put32(out, 0);
                });
                chunk(out, "pbag", [&] {
                    // Zone 0 spans pgen[0..1): the instrument gen. Bag 1 is
                    // the terminal zone.
                    bag_record(out, 0);
                    bag_record(out, 1);
                });
                chunk(out, "pmod", [&] { mod_record(out); });
                chunk(out, "pgen", [&] {
                    gen_record(out, 41, 0); // instrument = 0
                });
                chunk(out, "inst", [&] {
                    put_name(out, "SineInst");
                    put16(out, 0); // ibagIndex
                    put_name(out, "EOI");
                    put16(out, 1); // terminal
                });
                chunk(out, "ibag", [&] {
                    // Zone 0 spans igen[0..3): the three gens below. Bag 1 is
                    // the terminal zone and starts where they end.
                    bag_record(out, 0);
                    bag_record(out, 3);
                });
                chunk(out, "imod", [&] { mod_record(out); });
                chunk(out, "igen", [&] {
                    gen_range(out, 43, 0, 127);  // keyRange: full keyboard
                    gen_range(out, 44, 0, 127);  // velRange: full velocity
                    gen_record(out, 53, 0);      // sampleID = 0
                });
                chunk(out, "shdr", [&] {
                    // The real sample record (46 bytes) ...
                    put_name(out, "Sine");
                    put32(out, 0);                          // start
                    put32(out, kTestSampleFrames + 46);     // end
                    put32(out, 0);                          // loopStart
                    put32(out, kTestSampleFrames);          // loopEnd
                    put32(out, kTestSampleRate);
                    out.push_back(static_cast<std::uint8_t>(kTestPresetRootKey)); // originalPitch (1 byte)
                    out.push_back(0);                                             // pitchCorrection (1 byte)
                    put16(out, 0); // sampleLink
                    put16(out, 1); // type: looped
                    // ... and the terminal EOS record (all zero fields).
                    put_name(out, "EOS");
                    for (int i = 0; i < 5; ++i)
                        put32(out, 0);
                    out.push_back(0);
                    out.push_back(0);
                    put16(out, 0);
                    put16(out, 0);
                });
            });
        });
        return out;
    }

} // namespace aur::testsupport
