#include "mini_test.hpp"
#include "Sf2Fixture.hpp"

#include "aur/audio/Sf2Core.hpp"

#include <cmath>
#include <vector>

namespace
{
    using aur::audio::Sf2Core;

    void test_bank_loads_and_renders()
    {
        const auto bank = aur::testsupport::make_test_bank();
        Sf2Core core;
        ASSERT(core.loadBank(bank.data(), bank.size()));
        ASSERT(core.setup(44100.0));

        core.programChange(0, aur::testsupport::kTestPresetProgram);
        core.noteOn(0, 60, 100);
        float left = 0.f, right = 0.f;
        core.renderFrame(left, right);
        ASSERT(!std::isnan(left) && !std::isnan(right));
    }

    void test_render_is_nonzero()
    {
        const auto bank = aur::testsupport::make_test_bank();
        Sf2Core core;
        ASSERT(core.loadBank(bank.data(), bank.size()));
        ASSERT(core.setup(44100.0));

        core.programChange(0, aur::testsupport::kTestPresetProgram);
        core.noteOn(0, 60, 100);
        float peak = 0.f;
        for (int i = 0; i < 2205; ++i) // 50 ms at 44.1 kHz
        {
            float left = 0.f, right = 0.f;
            core.renderFrame(left, right);
            peak = std::max(peak, std::fabs(left));
        }
        ASSERT(peak > 0.01f);
    }

    void test_render_is_deterministic()
    {
        const auto bank = aur::testsupport::make_test_bank();

        auto render = [&bank]() {
            Sf2Core core;
            (void)core.loadBank(bank.data(), bank.size());
            (void)core.setup(44100.0);
            core.programChange(0, aur::testsupport::kTestPresetProgram);
            core.noteOn(0, 60, 100);
            std::vector<float> frames;
            frames.reserve(4410);
            for (int i = 0; i < 4410; ++i)
            {
                float left = 0.f, right = 0.f;
                core.renderFrame(left, right);
                frames.push_back(left);
                frames.push_back(right);
            }
            return frames;
        };

        const auto first = render();
        const auto second = render();
        ASSERT(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i)
            ASSERT(std::fabs(first[i] - second[i]) < 1e-9f);
    }

    void test_note_off_silences()
    {
        const auto bank = aur::testsupport::make_test_bank();
        Sf2Core core;
        (void)core.loadBank(bank.data(), bank.size());
        (void)core.setup(44100.0);
        core.programChange(0, aur::testsupport::kTestPresetProgram);
        core.noteOn(0, 60, 100);
        for (int i = 0; i < 4410; ++i) // 100 ms: past attack
        {
            float l = 0.f, r = 0.f;
            core.renderFrame(l, r);
        }
        core.noteOff(0, 60);
        float peak = 0.f;
        for (int i = 0; i < 22050; ++i) // 500 ms release tail
        {
            float l = 0.f, r = 0.f;
            core.renderFrame(l, r);
            peak = std::max(peak, std::fabs(l));
        }
        ASSERT(peak < 0.001f);
    }
} // namespace

void test_sf2()
{
    test_bank_loads_and_renders();
    test_render_is_nonzero();
    test_render_is_deterministic();
    test_note_off_silences();
}
