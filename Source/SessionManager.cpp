#include "SessionManager.h"

namespace
{
    juce::String slotTypeToString(PluginSlotType type)
    {
        switch (type)
        {
            case PluginSlotType::Synth: return "Synth";
            case PluginSlotType::FX:    return "FX";
            case PluginSlotType::Empty: return "Empty";
        }

        return "Empty";
    }

    PluginSlotType slotTypeFromString(const juce::String& type)
    {
        if (type == "Synth")
            return PluginSlotType::Synth;

        if (type == "FX")
            return PluginSlotType::FX;

        return PluginSlotType::Empty;
    }
}

bool SessionManager::saveSessionToFile(const SessionData& session, const juce::File& file)
{
    juce::XmlElement presetXml("PolyHostPreset");
    presetXml.setAttribute("name", session.name);
    presetXml.setAttribute("selectedTab", session.selectedTab);
    presetXml.setAttribute("hostTempoBpm", session.hostTempoBpm);
    presetXml.setAttribute("hostSyncEnabled", session.hostSyncEnabled);
    presetXml.setAttribute("windowWidth", session.window.width);
    presetXml.setAttribute("windowHeight", session.window.height);

    for (auto& tab : session.tabs)
    {
        auto tabXml = std::make_unique<juce::XmlElement>("Tab");
        tabXml->setAttribute("index", tab.index);
        tabXml->setAttribute("type", slotTypeToString(tab.type));
        tabXml->setAttribute("tabName", tab.tabName);
        tabXml->setAttribute("bypassed", tab.bypassed);

        if (!tab.midiAssignedDeviceIdentifiers.isEmpty())
        {
            auto* midiAssignmentsXml = tabXml->createNewChildElement("MidiAssignments");

            for (auto& identifier : tab.midiAssignedDeviceIdentifiers)
            {
                auto* deviceXml = midiAssignmentsXml->createNewChildElement("Device");
                deviceXml->setAttribute("identifier", identifier);
            }
        }

        if (tab.hasPlugin)
        {
            tabXml->setAttribute("pluginName", tab.plugin.pluginName);
            tabXml->setAttribute("pluginPath", tab.plugin.pluginPath);
            tabXml->setAttribute("pluginPathRelative", tab.plugin.pluginPathRelative);
            tabXml->setAttribute("pluginPathDriveFlexible", tab.plugin.pluginPathDriveFlexible);
            tabXml->setAttribute("pluginState", tab.plugin.pluginStateBase64);
            tabXml->setAttribute("pluginFormatName", tab.plugin.pluginFormatName);
            tabXml->setAttribute("isInstrument", tab.plugin.isInstrument);
            tabXml->setAttribute("pluginManufacturer", tab.plugin.pluginManufacturer);
            tabXml->setAttribute("pluginVersion", tab.plugin.pluginVersion);
        }

        presetXml.addChildElement(tabXml.release());
    }

    return presetXml.writeTo(file, {});
}

bool SessionManager::loadSessionFromFile(const juce::File& file,
                                         SessionData& session,
                                         juce::StringArray& warnings)
{
    warnings.clear();

    auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr || !xml->hasTagName("PolyHostPreset"))
        return false;

    session = {};
    session.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
    session.selectedTab = xml->getIntAttribute("selectedTab", 0);
    session.hostTempoBpm = xml->getDoubleAttribute("hostTempoBpm", 120.0);
    session.hostSyncEnabled = xml->getBoolAttribute("hostSyncEnabled", false);
    session.window.width = xml->getIntAttribute("windowWidth", 900);
    session.window.height = xml->getIntAttribute("windowHeight", 520);

    for (auto* tabXml : xml->getChildIterator())
    {
        if (!tabXml->hasTagName("Tab"))
            continue;

        SessionTabData tab;
        tab.index = tabXml->getIntAttribute("index", session.tabs.size());
        tab.type = slotTypeFromString(tabXml->getStringAttribute("type", "Empty"));
        tab.tabName = tabXml->getStringAttribute("tabName", "Empty");
        tab.bypassed = tabXml->getBoolAttribute("bypassed", false);

        if (auto* midiAssignmentsXml = tabXml->getChildByName("MidiAssignments"))
        {
            for (auto* deviceXml : midiAssignmentsXml->getChildIterator())
            {
                if (deviceXml->hasTagName("Device"))
                {
                    auto identifier = deviceXml->getStringAttribute("identifier").trim();

                    if (identifier.isNotEmpty())
                        tab.midiAssignedDeviceIdentifiers.addIfNotAlreadyThere(identifier);
                }
            }
        }

        tab.plugin.pluginName = tabXml->getStringAttribute("pluginName");
        tab.plugin.pluginPath = tabXml->getStringAttribute("pluginPath");
        tab.plugin.pluginPathRelative = tabXml->getStringAttribute("pluginPathRelative");
        tab.plugin.pluginPathDriveFlexible = tabXml->getStringAttribute("pluginPathDriveFlexible");
        tab.plugin.pluginStateBase64 = tabXml->getStringAttribute("pluginState");
        tab.plugin.pluginFormatName = tabXml->getStringAttribute("pluginFormatName");
        tab.plugin.isInstrument = tabXml->getBoolAttribute("isInstrument", tab.type == PluginSlotType::Synth);
        tab.plugin.pluginManufacturer = tabXml->getStringAttribute("pluginManufacturer");
        tab.plugin.pluginVersion = tabXml->getStringAttribute("pluginVersion");

        tab.hasPlugin = tab.plugin.pluginPath.isNotEmpty()
                        || tab.plugin.pluginPathRelative.isNotEmpty()
                        || tab.plugin.pluginPathDriveFlexible.isNotEmpty();

        session.tabs.add(tab);
    }

    return true;
}