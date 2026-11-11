#pragma once

// Sf2Core.hpp - a sampled GM/GS instrument over TinySoundFont, on the
// SynthCoreVoice surface. Not thread-safe: every call from the audio thread.

#include <cstdint>
#include <vector>

namespace aur::audio
{

    class Sf2Core
    {
    public:
        Sf2Core();
        ~Sf2Core();

        Sf2Core(const Sf2Core &) = delete;
        Sf2Core &operator=(const Sf2Core &) = delete;

        // Parses an SF2 image. Call before setup; may allocate.
        bool loadBank(const std::uint8_t *data, std::size_t size);

        // Initializes the render state at a sample rate.
        bool setup(double sampleRate);

        // Number of presets the loaded bank carries (0 = none).
        int presetCount() const;

        // Voices currently sounding (0 = the note did not start).
        int activeVoiceCount() const;

        void noteOn(int channel, int note, int velocity);
        void noteOff(int channel, int note);
        void pitchBend(int channel, int value14); // 0..16383, centre 8192
        void controlChange(int channel, int controller, int value);
        void programChange(int channel, int program);
        // Channel 10 (index 9) carries the drum kit preset.
        void channelBank(int channel, int bank);

        void allNotesOff();
        void renderFrame(float &left, float &right);
        void reset();

    private:
        struct Impl;
        Impl *impl = nullptr;
    };

} // namespace aur::audio
