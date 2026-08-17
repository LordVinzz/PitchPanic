#include "controller.h"

#include "parameters.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <algorithm>
#include <string_view>

namespace PitchPanic
{
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
void addRange (ParameterContainer& parameters, const TChar* title, ParamID id,
               const TChar* units, double minimum, double maximum,
               double defaultNormalized, int32 precision = 1)
{
    const double defaultPlain = minimum + defaultNormalized * (maximum - minimum);
    auto* parameter = new RangeParameter (title, id, units, minimum, maximum, defaultPlain,
                                          0, ParameterInfo::kCanAutomate);
    parameter->setPrecision (precision);
    parameters.addParameter (parameter);
}

void addToggle (ParameterContainer& parameters, const TChar* title, ParamID id,
                double defaultValue, int32 extraFlags = 0)
{
    parameters.addParameter (title, nullptr, 1, defaultValue,
                             ParameterInfo::kCanAutomate | extraFlags, id);
}

StringListParameter* addList (ParameterContainer& parameters, const TChar* title,
                              ParamID id, double defaultValue)
{
    auto* parameter = new StringListParameter (title, id);
    parameter->getInfo ().defaultNormalizedValue = defaultValue;
    parameter->setNormalized (defaultValue);
    parameters.addParameter (parameter);
    return parameter;
}
} // namespace

tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
    const auto result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    addToggle (parameters, STR16 ("Global Bypass"), kBypassId, kDefaultValues[kBypassId],
               ParameterInfo::kIsBypass);

    auto* mode = addList (parameters, STR16 ("Operational Mode"), kModeId, kDefaultValues[kModeId]);
    mode->appendString (STR16 ("WHAMMY / VECTOR"));
    mode->appendString (STR16 ("DIVE BOMB"));
    mode->appendString (STR16 ("OCTAVE ASCENT"));
    mode->appendString (STR16 ("OCTAVE DESCENT"));
    mode->appendString (STR16 ("DUAL OCTAVER"));
    mode->appendString (STR16 ("HARMONY STACK"));
    mode->appendString (STR16 ("MICRO DETUNE"));

    addRange (parameters, STR16 ("Expression Pedal"), kPedalId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kPedalId], 1);
    addRange (parameters, STR16 ("Heel Pitch"), kHeelId, STR16 ("st"), -36.0, 36.0,
              kDefaultValues[kHeelId], 1);
    addRange (parameters, STR16 ("Toe Pitch"), kToeId, STR16 ("st"), -36.0, 36.0,
              kDefaultValues[kToeId], 1);
    addRange (parameters, STR16 ("Pedal Curve"), kCurveId, nullptr, 0.25, 4.0,
              kDefaultValues[kCurveId], 2);
    addToggle (parameters, STR16 ("Chromatic Quantize"), kQuantizeId, kDefaultValues[kQuantizeId]);
    addRange (parameters, STR16 ("Master Fine"), kFineId, STR16 ("cent"), -100.0, 100.0,
              kDefaultValues[kFineId], 1);
    addRange (parameters, STR16 ("Detune Depth"), kDetuneId, STR16 ("cent"), 0.0, 100.0,
              kDefaultValues[kDetuneId], 1);

    addRange (parameters, STR16 ("Voice 1 Shift"), kVoice1ShiftId, STR16 ("st"), -24.0, 24.0,
              kDefaultValues[kVoice1ShiftId], 1);
    addRange (parameters, STR16 ("Voice 2 Shift"), kVoice2ShiftId, STR16 ("st"), -24.0, 24.0,
              kDefaultValues[kVoice2ShiftId], 1);
    addRange (parameters, STR16 ("Voice 1 Level"), kVoice1LevelId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kVoice1LevelId], 1);
    addRange (parameters, STR16 ("Voice 2 Level"), kVoice2LevelId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kVoice2LevelId], 1);

    addRange (parameters, STR16 ("Dry Matrix"), kDryId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kDryId], 1);
    addRange (parameters, STR16 ("Wet Matrix"), kWetId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kWetId], 1);
    addRange (parameters, STR16 ("Input Trim"), kInputId, STR16 ("dB"), -24.0, 24.0,
              kDefaultValues[kInputId], 1);
    addRange (parameters, STR16 ("Output Trim"), kOutputId, STR16 ("dB"), -24.0, 12.0,
              kDefaultValues[kOutputId], 1);

    addRange (parameters, STR16 ("Grain Window"), kWindowId, STR16 ("ms"), 6.0, 120.0,
              kDefaultValues[kWindowId], 1);
    addRange (parameters, STR16 ("Pitch Momentum"), kSmoothingId, STR16 ("ms"), 0.0, 250.0,
              kDefaultValues[kSmoothingId], 1);
    addRange (parameters, STR16 ("Regeneration"), kFeedbackId, STR16 ("%"), 0.0, 85.0,
              kDefaultValues[kFeedbackId], 1);
    addToggle (parameters, STR16 ("Buffer Freeze"), kFreezeId, kDefaultValues[kFreezeId]);

    auto* windowShape = addList (parameters, STR16 ("Window Topology"), kWindowShapeId,
                                 kDefaultValues[kWindowShapeId]);
    windowShape->appendString (STR16 ("HANN / SEALED"));
    windowShape->appendString (STR16 ("TRIANGLE / HARD"));
    windowShape->appendString (STR16 ("SINE / EQUAL POWER"));

    auto* quality = addList (parameters, STR16 ("Interpolation Reactor"), kQualityId,
                             kDefaultValues[kQualityId]);
    quality->appendString (STR16 ("ECONOMY / LINEAR"));
    quality->appendString (STR16 ("SURGICAL / CUBIC"));
    quality->appendString (STR16 ("CATASTROPHE / SINC-8"));

    addRange (parameters, STR16 ("Wet Lowpass"), kToneId, STR16 ("Hz"), 1000.0, 20000.0,
              kDefaultValues[kToneId], 0);
    addRange (parameters, STR16 ("Wet Highpass"), kLowCutId, STR16 ("Hz"), 20.0, 800.0,
              kDefaultValues[kLowCutId], 0);
    addRange (parameters, STR16 ("Input Drive"), kDriveId, STR16 ("dB"), 0.0, 24.0,
              kDefaultValues[kDriveId], 1);
    addRange (parameters, STR16 ("Gate Threshold"), kGateId, STR16 ("dB"), -90.0, -24.0,
              kDefaultValues[kGateId], 1);
    addRange (parameters, STR16 ("Stereo Detune"), kStereoSpreadId, STR16 ("cent"), 0.0, 40.0,
              kDefaultValues[kStereoSpreadId], 1);
    addRange (parameters, STR16 ("Wet Width"), kWidthId, STR16 ("%"), 0.0, 200.0,
              kDefaultValues[kWidthId], 1);

    addRange (parameters, STR16 ("Envelope Override"), kAutoAmountId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kAutoAmountId], 1);
    addRange (parameters, STR16 ("Envelope Sensitivity"), kSensitivityId, STR16 ("dB"), -60.0, 0.0,
              kDefaultValues[kSensitivityId], 1);
    addRange (parameters, STR16 ("Envelope Rise"), kRiseId, STR16 ("ms"), 1.0, 250.0,
              kDefaultValues[kRiseId], 1);
    addRange (parameters, STR16 ("Envelope Fall"), kFallId, STR16 ("ms"), 10.0, 1000.0,
              kDefaultValues[kFallId], 1);

    auto* autoDirection = addList (parameters, STR16 ("Envelope Polarity"), kAutoDirectionId,
                                   kDefaultValues[kAutoDirectionId]);
    autoDirection->appendString (STR16 ("RISE WITH LEVEL"));
    autoDirection->appendString (STR16 ("FALL WITH LEVEL"));

    addRange (parameters, STR16 ("LFO Frequency"), kLfoRateId, STR16 ("Hz"), 0.05, 12.0,
              kDefaultValues[kLfoRateId], 2);
    addRange (parameters, STR16 ("LFO Pedal Injection"), kLfoDepthId, STR16 ("%"), 0.0, 100.0,
              kDefaultValues[kLfoDepthId], 1);

    auto* lfoShape = addList (parameters, STR16 ("LFO Geometry"), kLfoShapeId,
                              kDefaultValues[kLfoShapeId]);
    lfoShape->appendString (STR16 ("SINE"));
    lfoShape->appendString (STR16 ("TRIANGLE"));
    lfoShape->appendString (STR16 ("SQUARE"));
    lfoShape->appendString (STR16 ("SAMPLE + HOLD"));

    addRange (parameters, STR16 ("Output Containment"), kLimiterId, STR16 ("dBFS"), -12.0, 0.0,
              kDefaultValues[kLimiterId], 1);
    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kInvalidArgument;

    IBStreamer streamer (state, kLittleEndian);
    uint32 magic = 0;
    uint32 version = 0;
    uint32 count = 0;
    if (!streamer.readInt32u (magic) || !streamer.readInt32u (version) ||
        !streamer.readInt32u (count) || magic != kStateMagic || version > kStateVersion)
        return kResultFalse;

    for (uint32 index = 0; index < count; ++index)
    {
        uint32 id = 0;
        double value = 0.0;
        if (!streamer.readInt32u (id) || !streamer.readDouble (value))
            return kResultFalse;
        if (id < kNumParameters)
            setParamNormalized (static_cast<ParamID> (id), clamp01 (value));
    }
    return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView (FIDString name)
{
    if (name && std::string_view (name) == ViewType::kEditor)
    {
        auto* editor = new VSTGUI::AspectRatioVST3Editor (
            this, "view", "pitchpanic.uidesc");
        editor->setMinZoomFactor (0.5);
        return editor;
    }
    return nullptr;
}

tresult PLUGIN_API Controller::getMidiControllerAssignment (int32 busIndex, int16,
                                                             CtrlNumber controller,
                                                             ParamID& tag)
{
    if (busIndex == 0 && (controller == kCtrlExpression || controller == kPitchBend))
    {
        tag = kPedalId;
        return kResultTrue;
    }
    return kResultFalse;
}

tresult PLUGIN_API Controller::queryInterface (const char* iid, void** object)
{
    QUERY_INTERFACE (iid, object, IMidiMapping::iid, IMidiMapping)
    return EditControllerEx1::queryInterface (iid, object);
}
} // namespace PitchPanic
