#include "Sf2Core.hpp"

// Single definition of the vendored implementation; other TUs get declarations only.

#define TSF_IMPLEMENTATION

#include "tsf.h"

#include <cmath>

namespace aur::audio
{

    struct Sf2Core::Impl
    {
        tsf *font = nullptr;
        double sampleRate = 44100.0;
    };

    Sf2Core::Sf2Core() : impl(new Impl) {}

    Sf2Core::~Sf2Core()
    {
        if (impl && impl->font)
            tsf_close(impl->font);
        delete impl;
    }

    bool Sf2Core::loadBank(const std::uint8_t *data, std::size_t size)
    {
        if (!impl || !data || size == 0)
            return false;
        if (impl->font)
        {
            tsf_close(impl->font);
            impl->font = nullptr;
        }
        impl->font = tsf_load_memory(data, static_cast<int>(size));
        return impl->font != nullptr;
    }

    bool Sf2Core::setup(double sampleRate)
    {
        if (!impl || !impl->font)
            return false;
        impl->sampleRate = sampleRate;
        tsf_set_output(impl->font, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), 0.0f);
        // A voice cap keeps a dense MIDI file from ballooning the cost; the
        // steal order is tsf's own, which the golden hash then pins.
        tsf_set_max_voices(impl->font, 64);
        return true;
    }

    int Sf2Core::presetCount() const
    {
        return (impl && impl->font) ? tsf_get_presetcount(impl->font) : 0;
    }

    int Sf2Core::activeVoiceCount() const
    {
        return (impl && impl->font) ? tsf_active_voice_count(impl->font) : 0;
    }

    void Sf2Core::noteOn(int channel, int note, int velocity)
    {
        if (impl && impl->font)
            tsf_channel_note_on(impl->font, channel, note,
                                static_cast<float>(velocity) / 127.0f);
    }

    void Sf2Core::noteOff(int channel, int note)
    {
        if (impl && impl->font)
            tsf_channel_note_off(impl->font, channel, note);
    }

    void Sf2Core::pitchBend(int channel, int value14)
    {
        if (impl && impl->font)
            tsf_channel_set_pitchwheel(impl->font, channel, value14);
    }

    void Sf2Core::controlChange(int channel, int controller, int value)
    {
        if (impl && impl->font)
            tsf_channel_midi_control(impl->font, channel, controller, value);
    }

    void Sf2Core::programChange(int channel, int program)
    {
        if (impl && impl->font)
            tsf_channel_set_presetindex(impl->font, channel, program);
    }

    void Sf2Core::channelBank(int channel, int bank)
    {
        if (impl && impl->font)
            tsf_channel_set_bank(impl->font, channel, bank);
    }

    void Sf2Core::allNotesOff()
    {
        if (impl && impl->font)
            tsf_note_off_all(impl->font);
    }

    void Sf2Core::renderFrame(float &left, float &right)
    {
        if (!impl || !impl->font)
        {
            left = right = 0.f;
            return;
        }
        float buffer[2];
        tsf_render_float(impl->font, buffer, 1, 0);
        left = buffer[0];
        right = buffer[1];
    }

    void Sf2Core::reset()
    {
        if (impl && impl->font)
            tsf_reset(impl->font);
    }

} // namespace aur::audio
