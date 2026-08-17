#include "processor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace PitchPanic
{
using namespace Steinberg;
using namespace Steinberg::Vst;

Processor::Processor ()
{
    setControllerClass (kControllerUID);
    for (int32 index = 0; index < static_cast<int32> (kNumParameters); ++index)
    {
        automatedParameters[static_cast<std::size_t> (index)].setParamID (
            static_cast<ParamID> (index));
        automatedParameters[static_cast<std::size_t> (index)].setValue (
            kDefaultValues[static_cast<std::size_t> (index)]);
    }
}

tresult PLUGIN_API Processor::initialize (FUnknown* context)
{
    const auto result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Instrument / Line In"), SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Catastrophic Pitch Out"), SpeakerArr::kStereo);
    addEventInput (STR16 ("Pedal MIDI In"), 1);
    return kResultOk;
}

tresult PLUGIN_API Processor::terminate ()
{
    stateTransfer.clear_ui ();
    return AudioEffect::terminate ();
}

tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& setup)
{
    const auto result = AudioEffect::setupProcessing (setup);
    if (result != kResultOk)
        return result;
    pitchEngine.prepare (setup.sampleRate, setup.maxSamplesPerBlock, PitchEngine::kMaxChannels);
    return kResultOk;
}

tresult PLUGIN_API Processor::setActive (TBool state)
{
    if (state)
        pitchEngine.reset ();
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API Processor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                   SpeakerArrangement* outputs, int32 numOuts)
{
    if (!inputs || !outputs || numIns != 1 || numOuts != 1 || inputs[0] != outputs[0])
        return kResultFalse;

    const int32 channels = SpeakerArr::getChannelCount (inputs[0]);
    if (channels != 1 && channels != 2)
        return kResultFalse;

    getAudioInput (0)->setArrangement (inputs[0]);
    getAudioOutput (0)->setArrangement (outputs[0]);
    return kResultTrue;
}

tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64 ?
        kResultTrue : kResultFalse;
}

void Processor::beginParameterChanges (IParameterChanges* changes) noexcept
{
    if (!changes)
        return;

    const int32 count = changes->getParameterCount ();
    for (int32 index = 0; index < count; ++index)
    {
        if (auto* queue = changes->getParameterData (index))
        {
            const ParamID id = queue->getParameterId ();
            if (id < kNumParameters)
                automatedParameters[static_cast<std::size_t> (id)].beginChanges (queue);
        }
    }
}

void Processor::endParameterChanges () noexcept
{
    for (auto& parameter : automatedParameters)
        parameter.endChanges ();
}

template <typename Sample>
bool Processor::processAudio (ProcessData& data, Sample** input, Sample** output,
                              int32 channels) noexcept
{
    PitchEngine::NormalizedParameters values {};
    std::array<double, PitchEngine::kMaxChannels> inputFrame {{0.0, 0.0}};
    std::array<double, PitchEngine::kMaxChannels> outputFrame {{0.0, 0.0}};
    bool producedAudio = false;

    for (int32 sampleIndex = 0; sampleIndex < data.numSamples; ++sampleIndex)
    {
        for (int32 parameterIndex = 0;
             parameterIndex < static_cast<int32> (kNumParameters); ++parameterIndex)
        {
            values[static_cast<std::size_t> (parameterIndex)] =
                automatedParameters[static_cast<std::size_t> (parameterIndex)].advance (1);
        }

        for (int32 channel = 0; channel < channels; ++channel)
        {
            inputFrame[static_cast<std::size_t> (channel)] = input && input[channel] ?
                static_cast<double> (input[channel][sampleIndex]) : 0.0;
        }

        pitchEngine.processFrame (inputFrame.data (), outputFrame.data (), channels, values);

        for (int32 channel = 0; channel < channels; ++channel)
        {
            const double sample = outputFrame[static_cast<std::size_t> (channel)];
            if (output && output[channel])
                output[channel][sampleIndex] = static_cast<Sample> (sample);
            producedAudio = producedAudio || std::abs (sample) > 1.0e-12;
        }
    }
    return producedAudio;
}

tresult PLUGIN_API Processor::process (ProcessData& data)
{
    stateTransfer.accessTransferObject_rt ([this] (const ParameterState& state) {
        for (const auto& change : state)
        {
            if (change.first < kNumParameters)
                automatedParameters[static_cast<std::size_t> (change.first)].setValue (
                    clamp01 (change.second));
        }
    });

    beginParameterChanges (data.inputParameterChanges);

    if (data.numSamples <= 0 || data.numInputs <= 0 || data.numOutputs <= 0)
    {
        endParameterChanges ();
        return kResultOk;
    }

    const int32 channels = std::min ({data.inputs[0].numChannels, data.outputs[0].numChannels,
                                     static_cast<int32> (PitchEngine::kMaxChannels)});
    if (channels <= 0)
    {
        endParameterChanges ();
        return kResultOk;
    }

    bool producedAudio = false;
    if (data.symbolicSampleSize == kSample32)
    {
        producedAudio = processAudio<float> (data, data.inputs[0].channelBuffers32,
                                              data.outputs[0].channelBuffers32, channels);
    }
    else if (data.symbolicSampleSize == kSample64)
    {
        producedAudio = processAudio<double> (data, data.inputs[0].channelBuffers64,
                                               data.outputs[0].channelBuffers64, channels);
    }
    else
    {
        endParameterChanges ();
        return kResultFalse;
    }

    data.outputs[0].silenceFlags = producedAudio ? 0 :
        ((static_cast<uint64> (1) << static_cast<uint64> (channels)) - 1);
    endParameterChanges ();
    return kResultOk;
}

tresult PLUGIN_API Processor::setState (IBStream* state)
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

    auto restored = std::make_unique<ParameterState> ();
    restored->reserve (std::min<uint32> (count, kNumParameters));
    for (uint32 index = 0; index < count; ++index)
    {
        uint32 id = 0;
        double value = 0.0;
        if (!streamer.readInt32u (id) || !streamer.readDouble (value))
            return kResultFalse;
        if (id < kNumParameters)
            restored->emplace_back (static_cast<ParamID> (id), clamp01 (value));
    }
    stateTransfer.transferObject_ui (std::move (restored));
    return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* state)
{
    if (!state)
        return kInvalidArgument;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeInt32u (kStateMagic) || !streamer.writeInt32u (kStateVersion) ||
        !streamer.writeInt32u (kNumParameters))
        return kResultFalse;

    for (uint32 id = 0; id < kNumParameters; ++id)
    {
        if (!streamer.writeInt32u (id) ||
            !streamer.writeDouble (automatedParameters[static_cast<std::size_t> (id)].getValue ()))
            return kResultFalse;
    }
    return kResultOk;
}
} // namespace PitchPanic
