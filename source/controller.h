#pragma once

#include "cids.h"

#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace PitchPanic
{
class Controller : public Steinberg::Vst::EditControllerEx1,
                   public Steinberg::Vst::IMidiMapping
{
public:
    Controller () = default;
    ~Controller () override = default;

    static Steinberg::FUnknown* PLUGIN_API createInstance (void*)
    {
        return static_cast<Steinberg::Vst::IEditController*> (new Controller ());
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment (
        Steinberg::int32 busIndex, Steinberg::int16 channel,
        Steinberg::Vst::CtrlNumber midiControllerNumber,
        Steinberg::Vst::ParamID& tag) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API queryInterface (const char* iid, void** object) SMTG_OVERRIDE;

    DELEGATE_REFCOUNT (Steinberg::Vst::EditControllerEx1)
};
} // namespace PitchPanic
