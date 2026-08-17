#pragma once

#include "parameters.h"

#include <array>
#include <cstdint>
#include <vector>

namespace PitchPanic
{
class PitchEngine
{
public:
    static constexpr int kMaxChannels = 2;
    using NormalizedParameters = std::array<double, kNumParameters>;

    void prepare (double newSampleRate, int maximumBlockSize, int channels);
    void reset ();

    void processFrame (const double* input, double* output, int channels,
                       const NormalizedParameters& parameters) noexcept;

    double getLastPedalPosition () const noexcept { return lastPedalPosition; }
    double getLastPitchSemitones () const noexcept { return lastPitchSemitones; }

private:
    struct ChannelState
    {
        std::vector<double> delay;
        std::array<double, 2> phase {{0.0, 0.25}};
        std::array<double, 2> smoothedPitch {{0.0, 0.0}};
        double feedbackSample {0.0};
        double highPassX {0.0};
        double highPassY {0.0};
        double lowPassY {0.0};
    };

    double readDelay (const ChannelState& channel, double delaySamples, int quality) const noexcept;
    double readLinear (const ChannelState& channel, double position) const noexcept;
    double readCubic (const ChannelState& channel, double position) const noexcept;
    double readSinc8 (const ChannelState& channel, double position) const noexcept;
    double readWrapped (const ChannelState& channel, int index) const noexcept;
    double renderVoice (ChannelState& channel, int voiceIndex, double semitones,
                        double windowSamples, int windowShape, int quality,
                        double unitySample) noexcept;
    double windowWeight (double phase, int windowShape) const noexcept;
    double nextLfo (double rateHz, int shape) noexcept;
    static double wrapPhase (double phase) noexcept;

    std::array<ChannelState, kMaxChannels> channelStates;
    int writeIndex {0};
    int preparedChannels {2};
    int delaySize {0};
    double sampleRate {44100.0};
    double envelope {0.0};
    double gateGain {1.0};
    double bypassMix {0.0};
    double lfoPhase {0.0};
    double heldRandom {0.0};
    std::uint32_t randomState {0xA341316Cu};
    double lastPedalPosition {0.0};
    double lastPitchSemitones {0.0};
};
} // namespace PitchPanic
