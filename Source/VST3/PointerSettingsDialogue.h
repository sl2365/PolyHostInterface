#pragma once

#include <JuceHeader.h>

class AppSettings;

namespace PointerSettingsDialogue
{
    void show(AppSettings& appSettings,
              float currentTolerance,
              juce::Component* centreAroundComponent);
}
