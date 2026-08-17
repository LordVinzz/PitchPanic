#include "pitchengine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace PitchPanic
{
namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;

double clampValue (double value, double minimum, double maximum)
{
    return std::max (minimum, std::min (maximum, value));
}

double onePoleCoefficient (double milliseconds, double sampleRate)
{
    if (milliseconds <= 0.0)
        return 0.0;
    return std::exp (-1.0 / (milliseconds * 0.001 * sampleRate));
}
} // namespace

void PitchEngine::prepare (double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate = std::max (8000.0, newSampleRate);
    preparedChannels = std::max (1, std::min (kMaxChannels, channels));
    delaySize = static_cast<int> (std::ceil (sampleRate * 0.25)) +
                std::max (maximumBlockSize, 1) + 64;

    for (auto& channel : channelStates)
        channel.delay.assign (static_cast<std::size_t> (delaySize), 0.0);

    reset ();
}

void PitchEngine::reset ()
{
    writeIndex = 0;
    envelope = 0.0;
    gateGain = 1.0;
    bypassMix = 0.0;
    lfoPhase = 0.0;
    heldRandom = 0.0;
    randomState = 0xA341316Cu;
    lastPedalPosition = 0.0;
    lastPitchSemitones = 0.0;

    for (int channelIndex = 0; channelIndex < kMaxChannels; ++channelIndex)
    {
        auto& channel = channelStates[static_cast<std::size_t> (channelIndex)];
        std::fill (channel.delay.begin (), channel.delay.end (), 0.0);
        channel.phase = {{0.0, channelIndex == 0 ? 0.25 : 0.75}};
        channel.smoothedPitch = {{0.0, 0.0}};
        channel.feedbackSample = 0.0;
        channel.highPassX = 0.0;
        channel.highPassY = 0.0;
        channel.lowPassY = 0.0;
    }
}

double PitchEngine::wrapPhase (double phase) noexcept
{
    phase -= std::floor (phase);
    return phase;
}

double PitchEngine::readWrapped (const ChannelState& channel, int index) const noexcept
{
    if (delaySize <= 0 || channel.delay.empty ())
        return 0.0;
    index %= delaySize;
    if (index < 0)
        index += delaySize;
    return channel.delay[static_cast<std::size_t> (index)];
}

double PitchEngine::readLinear (const ChannelState& channel, double position) const noexcept
{
    const auto base = static_cast<int> (std::floor (position));
    const double fraction = position - static_cast<double> (base);
    const double first = readWrapped (channel, base);
    return first + (readWrapped (channel, base + 1) - first) * fraction;
}

double PitchEngine::readCubic (const ChannelState& channel, double position) const noexcept
{
    const auto base = static_cast<int> (std::floor (position));
    const double t = position - static_cast<double> (base);
    const double y0 = readWrapped (channel, base - 1);
    const double y1 = readWrapped (channel, base);
    const double y2 = readWrapped (channel, base + 1);
    const double y3 = readWrapped (channel, base + 2);
    const double t2 = t * t;
    const double t3 = t2 * t;
    return 0.5 * ((2.0 * y1) + (-y0 + y2) * t +
                  (2.0 * y0 - 5.0 * y1 + 4.0 * y2 - y3) * t2 +
                  (-y0 + 3.0 * y1 - 3.0 * y2 + y3) * t3);
}

double PitchEngine::readSinc8 (const ChannelState& channel, double position) const noexcept
{
    const auto center = static_cast<int> (std::floor (position));
    double sum = 0.0;
    double weightSum = 0.0;

    for (int tap = -3; tap <= 4; ++tap)
    {
        const int sampleIndex = center + tap;
        const double distance = position - static_cast<double> (sampleIndex);
        const double absoluteDistance = std::abs (distance);
        if (absoluteDistance >= 4.0)
            continue;

        const double sinc = absoluteDistance < 1.0e-12 ? 1.0 :
            std::sin (kPi * distance) / (kPi * distance);
        const double window = 0.42 + 0.5 * std::cos (kPi * distance / 4.0) +
                              0.08 * std::cos (kPi * distance / 2.0);
        const double weight = sinc * window;
        sum += readWrapped (channel, sampleIndex) * weight;
        weightSum += weight;
    }

    return std::abs (weightSum) > 1.0e-12 ? sum / weightSum : 0.0;
}

double PitchEngine::readDelay (const ChannelState& channel, double delaySamples,
                               int quality) const noexcept
{
    const double position = static_cast<double> (writeIndex) - delaySamples;
    if (quality <= 0)
        return readLinear (channel, position);
    if (quality == 1)
        return readCubic (channel, position);
    return readSinc8 (channel, position);
}

double PitchEngine::windowWeight (double phase, int windowShape) const noexcept
{
    phase = wrapPhase (phase);
    switch (windowShape)
    {
        case 1: // triangle
            return 1.0 - std::abs (2.0 * phase - 1.0);
        case 2: // equal-power sine
            return std::sin (kPi * phase);
        default: // Hann
            return 0.5 - 0.5 * std::cos (2.0 * kPi * phase);
    }
}

double PitchEngine::renderVoice (ChannelState& channel, int voiceIndex, double semitones,
                                 double windowSamples, int windowShape, int quality,
                                 double unitySample) noexcept
{
    semitones = clampValue (semitones, -48.0, 48.0);
    const double ratio = std::pow (2.0, semitones / 12.0);
    const double phaseA = channel.phase[static_cast<std::size_t> (voiceIndex)];
    const double phaseB = wrapPhase (phaseA + 0.5);
    constexpr double minimumDelay = 8.0;

    const double weightA = windowWeight (phaseA, windowShape);
    const double weightB = windowWeight (phaseB, windowShape);
    const double weightTotal = std::max (1.0e-12, weightA + weightB);
    const double grainA = readDelay (channel, minimumDelay + phaseA * windowSamples, quality);
    const double grainB = readDelay (channel, minimumDelay + phaseB * windowSamples, quality);
    const double shifted = (grainA * weightA + grainB * weightB) / weightTotal;

    const double pitchBlend = clamp01 (std::abs (semitones) / 0.35);
    const double result = unitySample + (shifted - unitySample) * pitchBlend;

    const double phaseIncrement = (1.0 - ratio) / std::max (32.0, windowSamples);
    channel.phase[static_cast<std::size_t> (voiceIndex)] = wrapPhase (phaseA + phaseIncrement);
    return result;
}

double PitchEngine::nextLfo (double rateHz, int shape) noexcept
{
    const double phase = lfoPhase;
    double value = 0.0;
    switch (shape)
    {
        case 1:
            value = 1.0 - 4.0 * std::abs (phase - 0.5);
            break;
        case 2:
            value = phase < 0.5 ? 1.0 : -1.0;
            break;
        case 3:
            value = heldRandom;
            break;
        default:
            value = std::sin (2.0 * kPi * phase);
            break;
    }

    lfoPhase += clampValue (rateHz, 0.01, 30.0) / sampleRate;
    if (lfoPhase >= 1.0)
    {
        lfoPhase -= std::floor (lfoPhase);
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        heldRandom = (static_cast<double> (randomState) /
                      static_cast<double> (std::numeric_limits<std::uint32_t>::max ())) * 2.0 - 1.0;
    }
    return value;
}

void PitchEngine::processFrame (const double* input, double* output, int channels,
                                const NormalizedParameters& parameters) noexcept
{
    channels = std::max (1, std::min (preparedChannels, std::min (kMaxChannels, channels)));
    if (delaySize < 64)
    {
        for (int channel = 0; channel < channels; ++channel)
            output[channel] = input[channel];
        return;
    }

    double peak = 0.0;
    for (int channel = 0; channel < channels; ++channel)
        peak = std::max (peak, std::abs (input[channel]));

    const double riseMs = linearFromNormalized (parameters[kRiseId], 1.0, 250.0);
    const double fallMs = linearFromNormalized (parameters[kFallId], 10.0, 1000.0);
    const double envelopeCoefficient = onePoleCoefficient (peak > envelope ? riseMs : fallMs, sampleRate);
    envelope = envelopeCoefficient * envelope + (1.0 - envelopeCoefficient) * peak;

    const double sensitivityDb = linearFromNormalized (parameters[kSensitivityId], -60.0, 0.0);
    const double sensitivity = decibelsToGain (sensitivityDb);
    double automaticPedal = clamp01 ((envelope - sensitivity) / std::max (1.0e-9, 1.0 - sensitivity));
    if (listIndex (parameters[kAutoDirectionId], 2) == 1)
        automaticPedal = 1.0 - automaticPedal;

    double pedal = parameters[kPedalId] +
                   (automaticPedal - parameters[kPedalId]) * clamp01 (parameters[kAutoAmountId]);
    const double lfoRate = linearFromNormalized (parameters[kLfoRateId], 0.05, 12.0);
    const int lfoShape = listIndex (parameters[kLfoShapeId], 4);
    pedal += nextLfo (lfoRate, lfoShape) * clamp01 (parameters[kLfoDepthId]) * 0.5;
    pedal = clamp01 (pedal);
    lastPedalPosition = pedal;

    const double curve = linearFromNormalized (parameters[kCurveId], 0.25, 4.0);
    const double curvedPedal = std::pow (pedal, curve);
    const double heel = linearFromNormalized (parameters[kHeelId], -36.0, 36.0);
    const double toe = linearFromNormalized (parameters[kToeId], -36.0, 36.0);
    const double voice1Trim = linearFromNormalized (parameters[kVoice1ShiftId], -24.0, 24.0);
    const double voice2Shift = linearFromNormalized (parameters[kVoice2ShiftId], -24.0, 24.0);
    const double fine = linearFromNormalized (parameters[kFineId], -100.0, 100.0) / 100.0;
    const double detune = linearFromNormalized (parameters[kDetuneId], 0.0, 100.0) / 100.0;
    const int mode = listIndex (parameters[kModeId], kNumModes);

    double voice1Pitch = 0.0;
    double voice2Pitch = 0.0;
    bool useSecondVoice = false;
    switch (mode)
    {
        case kDiveBombMode:
            voice1Pitch = heel - std::max (1.0, std::abs (toe - heel)) * curvedPedal;
            break;
        case kOctaveUpMode:
            voice1Pitch = 12.0 * curvedPedal;
            break;
        case kOctaveDownMode:
            voice1Pitch = -12.0 * curvedPedal;
            break;
        case kDualOctaveMode:
            voice1Pitch = 12.0 * curvedPedal;
            voice2Pitch = -12.0 * curvedPedal;
            useSecondVoice = true;
            break;
        case kHarmonyMode:
            voice1Pitch = heel + (toe - heel) * curvedPedal;
            voice2Pitch = voice1Pitch + voice2Shift;
            useSecondVoice = true;
            break;
        case kDetuneMode:
            voice1Pitch = -detune * curvedPedal;
            voice2Pitch = detune * curvedPedal;
            useSecondVoice = true;
            break;
        case kWhammyMode:
        default:
            voice1Pitch = heel + (toe - heel) * curvedPedal;
            break;
    }

    voice1Pitch += voice1Trim;
    if (mode == kDualOctaveMode)
        voice2Pitch += voice2Shift;

    if (parameters[kQuantizeId] >= 0.5)
    {
        voice1Pitch = std::round (voice1Pitch);
        voice2Pitch = std::round (voice2Pitch);
    }
    voice1Pitch += fine;
    voice2Pitch += fine;
    lastPitchSemitones = voice1Pitch;

    const double windowMs = linearFromNormalized (parameters[kWindowId], 6.0, 120.0);
    const double windowSamples = clampValue (windowMs * 0.001 * sampleRate, 32.0,
                                             static_cast<double> (delaySize - 32));
    const double smoothMs = linearFromNormalized (parameters[kSmoothingId], 0.0, 250.0);
    const double pitchCoefficient = onePoleCoefficient (smoothMs, sampleRate);
    const int windowShape = listIndex (parameters[kWindowShapeId], 3);
    const int quality = listIndex (parameters[kQualityId], 3);

    const double inputGain = decibelsToGain (linearFromNormalized (parameters[kInputId], -24.0, 24.0));
    const double driveDb = linearFromNormalized (parameters[kDriveId], 0.0, 24.0);
    const double driveGain = decibelsToGain (driveDb);
    const double driveMix = driveDb / 24.0;
    const double driveNormalization = std::max (1.0e-9, std::tanh (driveGain));
    const double feedback = clamp01 (parameters[kFeedbackId]) * 0.85;
    const bool freeze = parameters[kFreezeId] >= 0.5;

    const double gateThresholdDb = linearFromNormalized (parameters[kGateId], -90.0, -24.0);
    const double gateTarget = envelope >= decibelsToGain (gateThresholdDb) ? 1.0 : 0.0;
    const double gateCoefficient = onePoleCoefficient (gateTarget > gateGain ? 2.0 : 80.0, sampleRate);
    gateGain = gateCoefficient * gateGain + (1.0 - gateCoefficient) * gateTarget;

    const double highPassHz = linearFromNormalized (parameters[kLowCutId], 20.0, 800.0);
    const double lowPassHz = linearFromNormalized (parameters[kToneId], 1000.0, 20000.0);
    const double highPassCoefficient = std::exp (-2.0 * kPi * highPassHz / sampleRate);
    const double lowPassCoefficient = std::exp (-2.0 * kPi * lowPassHz / sampleRate);
    const double spreadCents = linearFromNormalized (parameters[kStereoSpreadId], 0.0, 40.0);

    std::array<double, kMaxChannels> original {{0.0, 0.0}};
    std::array<double, kMaxChannels> drySignal {{0.0, 0.0}};
    std::array<double, kMaxChannels> wetSignal {{0.0, 0.0}};

    for (int channelIndex = 0; channelIndex < channels; ++channelIndex)
    {
        auto& channel = channelStates[static_cast<std::size_t> (channelIndex)];
        original[static_cast<std::size_t> (channelIndex)] = input[channelIndex];
        drySignal[static_cast<std::size_t> (channelIndex)] = input[channelIndex] * inputGain;

        const double gained = input[channelIndex] * inputGain;
        const double saturated = std::tanh (gained * driveGain) / driveNormalization;
        const double driven = (gained + (saturated - gained) * driveMix) * gateGain;
        const double writeSample = clampValue (driven + channel.feedbackSample * feedback, -4.0, 4.0);

        if (!freeze)
            channel.delay[static_cast<std::size_t> (writeIndex)] = writeSample;

        const double unitySample = freeze ? readDelay (channel, 8.0, quality) : driven;
        const double channelSpread = channels == 2 ?
            (channelIndex == 0 ? -0.5 : 0.5) * spreadCents / 100.0 : 0.0;

        const double targetVoice1 = voice1Pitch + channelSpread;
        const double targetVoice2 = voice2Pitch - channelSpread;
        channel.smoothedPitch[0] = pitchCoefficient * channel.smoothedPitch[0] +
                                   (1.0 - pitchCoefficient) * targetVoice1;
        channel.smoothedPitch[1] = pitchCoefficient * channel.smoothedPitch[1] +
                                   (1.0 - pitchCoefficient) * targetVoice2;

        const double voice1 = renderVoice (channel, 0, channel.smoothedPitch[0], windowSamples,
                                           windowShape, quality, unitySample);
        double combined = voice1 * clamp01 (parameters[kVoice1LevelId]);
        if (useSecondVoice)
        {
            const double voice2 = renderVoice (channel, 1, channel.smoothedPitch[1], windowSamples,
                                               windowShape, quality, unitySample);
            combined += voice2 * clamp01 (parameters[kVoice2LevelId]);
        }
        channel.feedbackSample = clampValue (combined, -2.0, 2.0);

        const double highPassed = highPassCoefficient *
            (channel.highPassY + combined - channel.highPassX);
        channel.highPassX = combined;
        channel.highPassY = highPassed;
        channel.lowPassY = (1.0 - lowPassCoefficient) * highPassed +
                           lowPassCoefficient * channel.lowPassY;
        wetSignal[static_cast<std::size_t> (channelIndex)] = channel.lowPassY;
    }

    if (channels == 2)
    {
        const double width = linearFromNormalized (parameters[kWidthId], 0.0, 2.0);
        const double mid = 0.5 * (wetSignal[0] + wetSignal[1]);
        const double side = 0.5 * (wetSignal[0] - wetSignal[1]) * width;
        wetSignal[0] = mid + side;
        wetSignal[1] = mid - side;
    }

    const double dryLevel = clamp01 (parameters[kDryId]);
    const double wetLevel = clamp01 (parameters[kWetId]);
    const double outputGain = decibelsToGain (linearFromNormalized (parameters[kOutputId], -24.0, 12.0));
    const double ceiling = decibelsToGain (linearFromNormalized (parameters[kLimiterId], -12.0, 0.0));
    const double bypassTarget = parameters[kBypassId] >= 0.5 ? 1.0 : 0.0;
    const double bypassCoefficient = onePoleCoefficient (3.0, sampleRate);
    bypassMix = bypassCoefficient * bypassMix + (1.0 - bypassCoefficient) * bypassTarget;

    for (int channelIndex = 0; channelIndex < channels; ++channelIndex)
    {
        double processed = (drySignal[static_cast<std::size_t> (channelIndex)] * dryLevel +
                            wetSignal[static_cast<std::size_t> (channelIndex)] * wetLevel) * outputGain;
        processed = clampValue (processed, -ceiling, ceiling);
        double result = processed + (original[static_cast<std::size_t> (channelIndex)] - processed) * bypassMix;
        if (!std::isfinite (result))
            result = 0.0;
        output[channelIndex] = result;
    }

    if (++writeIndex >= delaySize)
        writeIndex = 0;
}
} // namespace PitchPanic
