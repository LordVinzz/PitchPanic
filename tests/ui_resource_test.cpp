#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/cresourcedescription.h"
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/uidescription/uidescription.h"

#include "editor.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>

// Required by the SDK's module initialization support when VST3Editor is linked
// into this standalone smoke-test executable.
void* moduleHandle = nullptr;

namespace
{
[[noreturn]] void fail (const char* message)
{
    std::cerr << "Pitch Panic UI smoke test failed: " << message << '\n';
    VSTGUI::exit ();
    std::exit (1);
}
} // namespace

int main ()
{
#if defined(__APPLE__)
    VSTGUI::init (CFBundleGetMainBundle ());
#elif defined(_WIN32)
    VSTGUI::init (GetModuleHandle (nullptr));
#else
    VSTGUI::init (nullptr);
#endif

    {
        VSTGUI::UIDescription description (VSTGUI::CResourceDescription (PITCHPANIC_UIDESC_PATH));
        if (!description.parse ())
            fail ("VSTGUI could not parse pitchpanic.uidesc");

        std::list<const std::string*> tagNames;
        description.collectControlTagNames (tagNames);
        if (tagNames.size () != 38)
            fail ("the editor does not expose all 38 control tags");
        if (description.getTagForName ("Bypass") != 0 ||
            description.getTagForName ("Limiter") != 37)
            fail ("control-tag endpoints are incorrect");

        const auto* attributes = description.getViewAttributes ("view");
        VSTGUI::CPoint minimumSize;
        VSTGUI::CPoint maximumSize;
        if (!attributes || !attributes->getPointAttribute ("minSize", minimumSize) ||
            !attributes->getPointAttribute ("maxSize", maximumSize))
            fail ("editor resize constraints are missing");
        if (minimumSize != VSTGUI::CPoint (310.0, 195.0) ||
            maximumSize != VSTGUI::CPoint (1240.0, 780.0))
            fail ("editor resize constraints do not cover the intended 50%-200% range");

        auto view = VSTGUI::owned (description.createView ("view", nullptr));
        if (!view)
            fail ("VSTGUI could not instantiate the editor view tree");

        const auto size = view->getViewSize ();
        if (std::abs (size.getWidth () - 620.0) > 0.01 ||
            std::abs (size.getHeight () - 390.0) > 0.01)
            fail ("editor dimensions differ from the declared 620x390 panel");

        auto* root = dynamic_cast<VSTGUI::CViewContainer*> (view.get ());
        if (!root || root->getNbViews () < 12)
            fail ("editor root view is unexpectedly sparse");

        auto* editor = new PitchPanic::Editor (
            nullptr, "view", PITCHPANIC_UIDESC_PATH);
        void* contentScaleSupport = nullptr;
        const auto contentScaleResult = editor->queryInterface (
            Steinberg::IPlugViewContentScaleSupport::iid, &contentScaleSupport);
#if defined(__APPLE__)
        if (contentScaleResult != Steinberg::kNoInterface || contentScaleSupport != nullptr)
            fail ("macOS editor advertises host content scaling and can be double-scaled");
#else
        if (contentScaleResult != Steinberg::kResultOk || contentScaleSupport == nullptr)
            fail ("editor does not advertise host content scaling on this platform");
        static_cast<Steinberg::IPlugViewContentScaleSupport*> (contentScaleSupport)->release ();
#endif
        editor->release ();
    }

    VSTGUI::exit ();
    std::cout << "Pitch Panic UI smoke test passed (38 tags, 620x390 view instantiated)\n";
    return 0;
}
