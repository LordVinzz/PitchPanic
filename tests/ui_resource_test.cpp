#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/cresourcedescription.h"
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/uidescription/uidescription.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>

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
    }

    VSTGUI::exit ();
    std::cout << "Pitch Panic UI smoke test passed (38 tags, 620x390 view instantiated)\n";
    return 0;
}
