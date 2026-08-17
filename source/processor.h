#pragma once

#include "cids.h"
#include "parameters.h"
#include "pitchengine.h"

#include "public.sdk/source/vst/utility/rttransfer.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <utility>
#include <vector>

namespace PitchPanic
{
class Processor : public Steinberg::Vst::AudioEffect
{
public:
    Processor ();
    ~Processor () override = default;

    static Steinberg::FUnknown* PLUGIN_API createInstance (void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*> (new Processor ());
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements (
        Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    using ParameterChange = std::pair<Steinberg::Vst::ParamID, Steinberg::Vst::ParamValue>;
    using ParameterState = std::vector<ParameterChange>;

    void beginParameterChanges (Steinberg::Vst::IParameterChanges* changes) noexcept;
    void endParameterChanges () noexcept;

    template <typename Sample>
    bool processAudio (Steinberg::Vst::ProcessData& data, Sample** input, Sample** output,
                       Steinberg::int32 channels) noexcept;

    std::array<Steinberg::Vst::SampleAccurate::Parameter, kNumParameters> automatedParameters;
    Steinberg::Vst::RTTransferT<ParameterState> stateTransfer;
    PitchEngine pitchEngine;
};
} // namespace PitchPanic
