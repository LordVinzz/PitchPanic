#pragma once

#include "pluginterfaces/vst/vsttypes.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace PitchPanic
{
enum ParameterId : Steinberg::Vst::ParamID
{
    kBypassId = 0,
    kModeId,
    kPedalId,
    kHeelId,
    kToeId,
    kCurveId,
    kQuantizeId,
    kFineId,
    kDetuneId,
    kVoice1ShiftId,
    kVoice2ShiftId,
    kVoice1LevelId,
    kVoice2LevelId,
    kDryId,
    kWetId,
    kInputId,
    kOutputId,
    kWindowId,
    kSmoothingId,
    kFeedbackId,
    kFreezeId,
    kWindowShapeId,
    kQualityId,
    kToneId,
    kLowCutId,
    kDriveId,
    kGateId,
    kStereoSpreadId,
    kWidthId,
    kAutoAmountId,
    kSensitivityId,
    kRiseId,
    kFallId,
    kAutoDirectionId,
    kLfoRateId,
    kLfoDepthId,
    kLfoShapeId,
    kLimiterId,
    kNumParameters
};

enum Mode
{
    kWhammyMode = 0,
    kDiveBombMode,
    kOctaveUpMode,
    kOctaveDownMode,
    kDualOctaveMode,
    kHarmonyMode,
    kDetuneMode,
    kNumModes
};

inline constexpr Steinberg::uint32 kStateMagic = 0x5050414E; // "PPAN"
inline constexpr Steinberg::uint32 kStateVersion = 1;

inline constexpr std::array<double, kNumParameters> kDefaultValues {{
    0.0,                       // bypass
    0.0,                       // mode: Whammy
    0.0,                       // pedal: heel
    0.5,                       // heel: 0 st
    5.0 / 6.0,                 // toe: +24 st
    0.2,                       // curve: 1.0
    0.0,                       // chromatic quantize
    0.5,                       // fine: 0 cent
    0.18,                      // detune: 18 cent
    0.5,                       // voice 1 trim: 0 st
    0.5,                       // voice 2 shift: 0 st
    1.0,                       // voice 1 level
    0.65,                      // voice 2 level
    0.0,                       // dry
    1.0,                       // wet
    0.5,                       // input: 0 dB
    2.0 / 3.0,                 // output: 0 dB
    34.0 / 114.0,              // window: 40 ms
    0.14,                      // pitch smoothing: 35 ms
    0.0,                       // feedback
    0.0,                       // freeze
    0.0,                       // Hann window
    0.5,                       // HQ cubic quality
    17.0 / 19.0,               // tone: 18 kHz
    0.0,                       // low cut: 20 Hz
    0.0,                       // drive
    0.0,                       // gate: -90 dB
    0.0,                       // stereo spread
    0.5,                       // width: 100%
    0.0,                       // envelope auto amount
    0.6,                       // sensitivity: -24 dB
    29.0 / 249.0,              // rise: 30 ms
    240.0 / 990.0,             // fall: 250 ms
    0.0,                       // auto direction: rise
    0.45 / 11.95,              // LFO: 0.5 Hz
    0.0,                       // LFO depth
    0.0,                       // LFO shape: sine
    0.975                      // limiter ceiling: -0.3 dBFS
}};

inline double clamp01 (double value)
{
    return std::max (0.0, std::min (1.0, value));
}

inline double linearFromNormalized (double normalized, double minimum, double maximum)
{
    return minimum + clamp01 (normalized) * (maximum - minimum);
}

inline double decibelsToGain (double decibels)
{
    return std::pow (10.0, decibels / 20.0);
}

inline int listIndex (double normalized, int count)
{
    return std::max (0, std::min (count - 1,
        static_cast<int> (std::floor (clamp01 (normalized) * (count - 1) + 0.5))));
}
} // namespace PitchPanic
