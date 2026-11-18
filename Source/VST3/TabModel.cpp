#include "TabModel.h"

TabModel::TabModel()
{
    tabs.add({ "Empty", PluginSlotType::Empty, true });
}

int TabModel::getNumTabs() const
{
    return tabs.size();
}

const Tab& TabModel::getTab(int index) const
{
    return tabs.getReference(index);
}

Tab& TabModel::getTab(int index)
{
    return tabs.getReference(index);
}

int TabModel::getSelectedTabIndex() const
{
    for (int i = 0; i < tabs.size(); ++i)
    {
        if (tabs.getReference(i).selected)
            return i;
    }

    return 0;
}

void TabModel::selectTab(int index)
{
    for (int i = 0; i < tabs.size(); ++i)
        tabs.getReference(i).selected = (i == index);
}

void TabModel::clear()
{
    tabs.clear();
}

void TabModel::addTab(const juce::String& name,
                      PluginSlotType type,
                      bool selected)
{
    if (selected)
    {
        for (int i = 0; i < tabs.size(); ++i)
            tabs.getReference(i).selected = false;
    }

    tabs.add({ name, type, selected });

    if (tabs.size() == 1)
        tabs.getReference(0).selected = true;
}

void TabModel::removeTab(int index)
{
    if (! juce::isPositiveAndBelow(index, tabs.size()))
        return;

    bool wasSelected = tabs.getReference(index).selected;
    tabs.remove(index);

    if (tabs.isEmpty())
    {
        tabs.add({ "Empty", PluginSlotType::Empty, true });
        return;
    }

    if (wasSelected)
    {
        int newSelected = juce::jlimit(0, tabs.size() - 1, index);
        selectTab(newSelected);
    }
}