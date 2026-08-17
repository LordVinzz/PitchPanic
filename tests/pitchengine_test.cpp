#include "parameters.h"
#include "pitchengine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;

[[noreturn]] void fail (const char* message)
{
    std::cerr << "Pitch Panic DSP smoke test failed: " << message << '\n';
    std::exit (1);
}

double estimateFrequency (const std::vector<double>& signal, double sampleRate,
                          std::size_t startSample)
{
    double bestFrequency = 0.0;
    double bestPower = -1.0;
    for (double frequency = 380.0; frequency <= 500.0; frequency += 0.5)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t index = startSample; index < signal.size (); ++index)
        {
            const double phase = 2.0 * kPi * frequency *
                                 static_cast<double> (index - startSample) / sampleRate;
            real += signal[index] * std::cos (phase);
            imaginary -= signal[index] * std::sin (phase);
        }
        const double power = real * real + imaginary * imaginary;
        if (power > bestPower)
        {
            bestPower = power;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
}
} // namespace

int main ()
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 48000;
    PitchPanic::PitchEngine engine;
    engine.prepare (sampleRate, 512, 2);

    auto parameters = PitchPanic::kDefaultValues;
    parameters[PitchPanic::kPedalId] = 1.0;
    parameters[PitchPanic::kToeId] = 2.0 / 3.0; // +12 semitones
    parameters[PitchPanic::kSmoothingId] = 0.0;
    parameters[PitchPanic::kWindowId] = 0.2;
    parameters[PitchPanic::kQualityId] = 0.5;
    parameters[PitchPanic::kLimiterId] = 1.0;

    std::vector<double> shifted (sampleCount, 0.0);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const double sine = 0.2 * std::sin (2.0 * kPi * 220.0 * sample / sampleRate);
        const std::array<double, 2> input {{sine, sine}};
        std::array<double, 2> output {{0.0, 0.0}};
        engine.processFrame (input.data (), output.data (), 2, parameters);
        if (!std::isfinite (output[0]) || !std::isfinite (output[1]))
            fail ("non-finite output");
        shifted[static_cast<std::size_t> (sample)] = output[0];
    }

    double energy = 0.0;
    for (std::size_t index = sampleCount / 2; index < shifted.size (); ++index)
        energy += shifted[index] * shifted[index];
    const double rms = std::sqrt (energy / (sampleCount / 2));
    if (rms < 0.02)
        fail ("pitch-shifted output is unexpectedly silent");

    const double frequency = estimateFrequency (shifted, sampleRate, sampleCount / 2);
    // A two-head time-domain shifter produces moving-window sidebands; verify
    // that the dominant component lands in the octave-up region.
    if (frequency < 420.0 || frequency > 460.0)
    {
        std::cerr << "Measured octave-up spectral peak: " << frequency
                  << " Hz, commanded pitch: " << engine.getLastPitchSemitones () << " st\n";
        fail ("octave-up frequency is outside tolerance");
    }

    parameters[PitchPanic::kModeId] =
        static_cast<double> (PitchPanic::kOctaveDownMode) / (PitchPanic::kNumModes - 1);
    for (int sample = 0; sample < 2048; ++sample)
    {
        const std::array<double, 2> input {{0.1, -0.1}};
        std::array<double, 2> output {{0.0, 0.0}};
        engine.processFrame (input.data (), output.data (), 2, parameters);
    }
    if (std::abs (engine.getLastPitchSemitones () + 12.0) > 0.01)
        fail ("octave-down mode did not command -12 semitones");

    parameters[PitchPanic::kFreezeId] = 1.0;
    parameters[PitchPanic::kFeedbackId] = 1.0;
    for (int sample = 0; sample < 8192; ++sample)
    {
        const std::array<double, 2> input {{0.0, 0.0}};
        std::array<double, 2> output {{0.0, 0.0}};
        engine.processFrame (input.data (), output.data (), 2, parameters);
        if (!std::isfinite (output[0]) || std::abs (output[0]) > 1.000001)
            fail ("freeze/feedback safety containment failed");
    }

    parameters[PitchPanic::kFreezeId] = 0.0;
    parameters[PitchPanic::kBypassId] = 1.0;
    double lastError = 1.0;
    for (int sample = 0; sample < 4096; ++sample)
    {
        const double source = 0.3 * std::sin (2.0 * kPi * 997.0 * sample / sampleRate);
        const std::array<double, 2> input {{source, source}};
        std::array<double, 2> output {{0.0, 0.0}};
        engine.processFrame (input.data (), output.data (), 2, parameters);
        lastError = std::abs (output[0] - source);
    }
    if (lastError > 1.0e-6)
        fail ("bypass crossfade did not settle to transparent audio");

    std::cout << "Pitch Panic DSP smoke test passed (measured octave-up "
              << frequency << " Hz, RMS " << rms << ")\n";
    return 0;
}
