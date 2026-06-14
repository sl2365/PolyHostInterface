#pragma once
#include <JuceHeader.h>
#include "SessionManager.h"

struct Tab
{
    juce::String name { "Empty" };
    PluginSlotType type = PluginSlotType::Empty;
    bool selected = true;
};

class TabModel
{
public:
    TabModel();

    int getNumTabs() const;
    const Tab& getTab(int index) const;
    Tab& getTab(int index);

    int getSelectedTabIndex() const;
    void selectTab(int index);

    void clear();
    void addTab(const juce::String& name,
                PluginSlotType type = PluginSlotType::Empty,
                bool selected = false);
    void removeTab(int index);

private:
    juce::Array<Tab> tabs;
};