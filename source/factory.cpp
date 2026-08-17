#include "cids.h"
#include "controller.h"
#include "processor.h"
#include "version.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF (PITCHPANIC_VENDOR, PITCHPANIC_URL, PITCHPANIC_EMAIL)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (PitchPanic::kProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                PITCHPANIC_PLUGIN_NAME,
                Vst::kDistributable,
                Vst::PlugType::kFxPitchShift,
                PITCHPANIC_VERSION_STRING,
                kVstVersionString,
                PitchPanic::Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (PitchPanic::kControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                PITCHPANIC_PLUGIN_NAME " Controller",
                0,
                "",
                PITCHPANIC_VERSION_STRING,
                kVstVersionString,
                PitchPanic::Controller::createInstance)

END_FACTORY
