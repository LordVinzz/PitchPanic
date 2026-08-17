#pragma once

#include "vstgui/plugin-bindings/vst3editor.h"

namespace PitchPanic
{
class Editor final : public VSTGUI::AspectRatioVST3Editor
{
public:
    using AspectRatioVST3Editor::AspectRatioVST3Editor;

    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID iid,
                                                   void** object) override
    {
#if SMTG_OS_MACOS && defined(VST3_CONTENT_SCALE_SUPPORT)
        // NSView ViewRect coordinates are already expressed in logical points. Advertising
        // Windows-style host content scaling here makes some macOS hosts apply the Retina
        // factor to the editor dimensions a second time. The resulting 2x minimum-size clamp
        // prevents shrinking and drives AspectRatioVST3Editor back toward a 0.5 zoom.
        if (Steinberg::FUnknownPrivate::iidEqual (
                iid, Steinberg::IPlugViewContentScaleSupport::iid))
        {
            *object = nullptr;
            return Steinberg::kNoInterface;
        }
#endif
        return AspectRatioVST3Editor::queryInterface (iid, object);
    }

protected:
    Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize) override
    {
        if (!canCalculateAspectRatio ())
            return AspectRatioVST3Editor::onSize (newSize);

        const auto result = AspectRatioVST3Editor::onSize (newSize);
        if (result != Steinberg::kResultTrue)
            return result;

        // AspectRatioVST3Editor updates only the VSTGUI zoom in this path. Also run the
        // VST3Editor handler so CPluginView::rect is updated and a subsequent getSize()
        // reports the accepted host-window dimensions instead of the original 620x390.
        return VST3Editor::onSize (newSize);
    }
};
} // namespace PitchPanic
