// SynthCores.cpp - note-event translation + per-frame rendering for the MD/SPC cores.
#include "aur/audio/SynthCores.hpp"

#include <algorithm>

namespace aur
{
    namespace audio
    {

        namespace
        {

            // Track -> core channel mapping. The YM has 6 channels; tracks beyond 6 fold
            // onto channel (track % 6). The SNES core accepts all 16 MIDI channels.
            int fmChannelForTrack(int track) { return ((track % 6) + 6) % 6; }

            void applyFmTrackTone(YmFmCore &fm, int ch, int program)
            {
                // Programs pick a built-in patch: patch i for program i mod 13.
                fm.loadBuiltinPatch(ch, ((program % 13) + 13) % 13);
            }

            void applySpcTrackTone(SnesPcmCore &spc, int ch, int program)
            {
                // Three procedural demo samples: saw / pulse / snare on slots 0..2.
                spc.setChannelParam(ch, SnesParam::Slot, ((program % 3) + 3) % 3);
            }

        } // namespace

        void SynthCoreVoice::setup(Kind kind, double sampleRate)
        {
            kind_ = kind;
            sampleRate_ = sampleRate;
            events_.clear();
            pendingOffs_.clear();
            offIndex_.clear();
            eventCursor_ = 0;
            offCursor_ = 0;
            nextEventIndex_ = 0;
            fm_ = YmFmCore{};
            spc_ = SnesPcmCore{};
            fm_.init(sampleRate);
            spc_.init(sampleRate);
            spc_.loadDemoSamples();
            if (kind_ == Kind::MdFm)
            {
                fm_.setGlobalParam(YmParam::MasterGain, 60);
            }
        }

        void SynthCoreVoice::setEvents(std::vector<CoreNoteEvent> events)
        {
            std::sort(events.begin(), events.end(),
                      [](const CoreNoteEvent &a, const CoreNoteEvent &b)
                      {
                          return a.startFrame < b.startFrame;
                      });
            events_ = std::move(events);
            eventCursor_ = 0;
            pendingOffs_.clear();
            offIndex_.clear();
            offCursor_ = 0;
            nextEventIndex_ = 0;
        }

        void SynthCoreVoice::clearEvents()
        {
            events_.clear();
            pendingOffs_.clear();
            offIndex_.clear();
            eventCursor_ = 0;
            offCursor_ = 0;
            nextEventIndex_ = 0;
        }

        void SynthCoreVoice::rebaseEvents(std::int64_t shift)
        {
            for (CoreNoteEvent &e : events_)
            {
                e.startFrame += shift;
                e.endFrame += shift;
            }
            eventCursor_ = 0;
            pendingOffs_.clear();
            offIndex_.clear();
            offCursor_ = 0;
            nextEventIndex_ = 0;
        }

        void SynthCoreVoice::issueNoteOn(const CoreNoteEvent &e)
        {
            if (kind_ == Kind::MdFm)
            {
                const int ch = fmChannelForTrack(e.track);
                applyFmTrackTone(fm_, ch, e.program);
                fm_.setChannelParam(ch, YmParam::Pan, e.pan >= 85 ? 1 : (e.pan <= 42 ? 2 : 3));
                fm_.noteOn(ch, e.pitch, e.velocity);
            }
            else
            {
                const int ch = ((e.track % 16) + 16) % 16;
                applySpcTrackTone(spc_, ch, e.program);
                spc_.setChannelParam(ch, SnesParam::Pan, e.pan);
                spc_.noteOn(ch, e.pitch, e.velocity);
            }
            pendingOffs_.push_back(e.endFrame);
            offIndex_.push_back(nextEventIndex_++);
        }

        void SynthCoreVoice::issueNoteOff(const CoreNoteEvent &e)
        {
            if (kind_ == Kind::MdFm)
            {
                fm_.noteOff(fmChannelForTrack(e.track), e.pitch);
            }
            else
            {
                spc_.noteOff(((e.track % 16) + 16) % 16, e.pitch);
            }
        }

        void SynthCoreVoice::consumeUpTo(std::int64_t frame)
        {
            while (eventCursor_ < events_.size() &&
                   events_[eventCursor_].startFrame <= frame)
            {
                const CoreNoteEvent &e = events_[eventCursor_];
                // A zero/negative-length note is a no-op.
                if (e.endFrame > e.startFrame)
                    issueNoteOn(e);
                ++eventCursor_;
            }
            while (offCursor_ < pendingOffs_.size() && pendingOffs_[offCursor_] <= frame)
            {
                // Offs mirror the issued note-on order; offIndex_ maps each off
                // back to its events_ index for the matching note-off call.
                issueNoteOff(events_[offIndex_[offCursor_]]);
                ++offCursor_;
            }
        }

        void SynthCoreVoice::renderFrame(float &left, float &right)
        {
            left = 0.f;
            right = 0.f;
            if (kind_ == Kind::MdFm)
            {
                float l = 0.f, r = 0.f;
                fm_.render(&l, &r, 1);
                left = l;
                right = r;
            }
            else
            {
                float l = 0.f, r = 0.f;
                spc_.render(&l, &r, 1);
                left = l;
                right = r;
            }
        }

        void SynthCoreVoice::reset()
        {
            fm_.reset();
            spc_.reset();
            eventCursor_ = 0;
            offCursor_ = 0;
            nextEventIndex_ = 0;
        }

    } // namespace audio
} // namespace aur
