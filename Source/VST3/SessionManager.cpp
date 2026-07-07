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

std::unique_ptr<juce::XmlElement> SessionManager::createXmlFromSessionData(const SessionData& session)
{
    auto presetXml = std::make_unique<juce::XmlElement>("PolyHostPreset");
    presetXml->setAttribute("name", session.name);
    presetXml->setAttribute("hostTempoBpm", session.hostTempoBpm);
    presetXml->setAttribute("selectedTabIndex", session.selectedTabIndex);

    for (auto& tab : session.tabs)
    {
        auto tabXml = std::make_unique<juce::XmlElement>("Tab");
        tabXml->setAttribute("index", tab.index);
        tabXml->setAttribute("type", slotTypeToString(tab.type));
        tabXml->setAttribute("tabName", tab.tabName);
        tabXml->setAttribute("bypassed", tab.bypassed);
        tabXml->setAttribute("hasSavedWindowBounds", tab.hasSavedWindowBounds);
        tabXml->setAttribute("savedWindowWidth", tab.savedWindowWidth);
        tabXml->setAttribute("savedWindowHeight", tab.savedWindowHeight);
        tabXml->setAttribute("pointerLaneTolerance", tab.pointerLaneTolerance);
        tabXml->setAttribute("pointerAdjustSensitivity", tab.pointerAdjustSensitivity);
        tabXml->setAttribute("pointerAdjustMethodOverride", tab.pointerAdjustMethodOverride);
        tabXml->setAttribute("preferGlobalPointerMap", tab.preferGlobalPointerMap);

        if (tab.hasCustomPointerMap)
        {
            auto* pointerPointsXml = tabXml->createNewChildElement("PointerJumpPoints");

            for (auto& point : tab.pointerJumpPoints)
            {
                auto* pointXml = pointerPointsXml->createNewChildElement("Point");
                pointXml->setAttribute("x", point.x);
                pointXml->setAttribute("y", point.y);
            }
        }

        if (! tab.midiAssignedDeviceIdentifiers.isEmpty())
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
            tabXml->setAttribute("pluginDescriptiveName", tab.plugin.pluginDescriptiveName);
            tabXml->setAttribute("pluginPath", tab.plugin.pluginPath);
            tabXml->setAttribute("pluginPathRelative", tab.plugin.pluginPathRelative);
            tabXml->setAttribute("pluginPathDriveFlexible", tab.plugin.pluginPathDriveFlexible);
            tabXml->setAttribute("pluginState", tab.plugin.pluginStateBase64);
            tabXml->setAttribute("pluginFormatName", tab.plugin.pluginFormatName);
            tabXml->setAttribute("pluginFileOrIdentifier", tab.plugin.pluginFileOrIdentifier);
            tabXml->setAttribute("pluginUniqueId", tab.plugin.pluginUniqueId);
            tabXml->setAttribute("isInstrument", tab.plugin.isInstrument);
            tabXml->setAttribute("pluginManufacturer", tab.plugin.pluginManufacturer);
            tabXml->setAttribute("pluginVersion", tab.plugin.pluginVersion);
        }

        presetXml->addChildElement(tabXml.release());
    }

    if (! session.macroMappings.isEmpty())
    {
        auto* macroMappingsXml = presetXml->createNewChildElement("MacroMappings");

        for (auto& mapping : session.macroMappings)
        {
            auto* mappingXml = macroMappingsXml->createNewChildElement("Macro");
            mappingXml->setAttribute("macroIndex", mapping.macroIndex);
            mappingXml->setAttribute("label", mapping.label);
            mappingXml->setAttribute("tabIndex", mapping.tabIndex);
            mappingXml->setAttribute("pluginName", mapping.pluginName);
            mappingXml->setAttribute("parameterIndex", mapping.parameterIndex);
            mappingXml->setAttribute("parameterName", mapping.parameterName);
            mappingXml->setAttribute("enabled", mapping.enabled);
        }
    }

    return presetXml;
}

bool SessionManager::restoreSessionDataFromXml(const juce::XmlElement& xml,
                                               SessionData& session,
                                               juce::StringArray& warnings)
{
    warnings.clear();

    if (! xml.hasTagName("PolyHostPreset"))
        return false;

    session = {};
    session.name = xml.getStringAttribute("name", "Untitled");
    session.hostTempoBpm = xml.getDoubleAttribute("hostTempoBpm", 120.0);
    session.selectedTabIndex = juce::jmax(0, xml.getIntAttribute("selectedTabIndex", 0));

    for (auto* tabXml : xml.getChildIterator())
    {
        if (! tabXml->hasTagName("Tab"))
            continue;

        SessionTabData tab;
        tab.index = tabXml->getIntAttribute("index", session.tabs.size());
        tab.type = slotTypeFromString(tabXml->getStringAttribute("type", "Empty"));
        tab.tabName = tabXml->getStringAttribute("tabName", "Empty");
        tab.bypassed = tabXml->getBoolAttribute("bypassed", false);
        tab.hasSavedWindowBounds = tabXml->getBoolAttribute("hasSavedWindowBounds", false);
        tab.savedWindowWidth = tabXml->getIntAttribute("savedWindowWidth", 0);
        tab.savedWindowHeight = tabXml->getIntAttribute("savedWindowHeight", 0);
        tab.pointerLaneTolerance = (float) tabXml->getDoubleAttribute("pointerLaneTolerance", 30.0);
        tab.pointerAdjustSensitivity = juce::jlimit(1, 20,
            tabXml->getIntAttribute("pointerAdjustSensitivity", 1));
        tab.pointerAdjustMethodOverride = juce::jlimit(0, 2,
            tabXml->getIntAttribute("pointerAdjustMethodOverride", 0));
        tab.preferGlobalPointerMap = tabXml->getBoolAttribute("preferGlobalPointerMap", false);

        if (auto* pointerPointsXml = tabXml->getChildByName("PointerJumpPoints"))
        {
            tab.hasCustomPointerMap = true;

            for (auto* pointXml : pointerPointsXml->getChildIterator())
            {
                if (! pointXml->hasTagName("Point"))
                    continue;

                SessionTabData::PointerJumpPoint point;
                point.x = (float) pointXml->getDoubleAttribute("x", 0.0);
                point.y = (float) pointXml->getDoubleAttribute("y", 0.0);
                tab.pointerJumpPoints.add(point);
            }
        }

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
        tab.plugin.pluginDescriptiveName = tabXml->getStringAttribute("pluginDescriptiveName");
        tab.plugin.pluginPath = tabXml->getStringAttribute("pluginPath");
        tab.plugin.pluginPathRelative = tabXml->getStringAttribute("pluginPathRelative");
        tab.plugin.pluginPathDriveFlexible = tabXml->getStringAttribute("pluginPathDriveFlexible");
        tab.plugin.pluginStateBase64 = tabXml->getStringAttribute("pluginState");
        tab.plugin.pluginFormatName = tabXml->getStringAttribute("pluginFormatName");
        tab.plugin.pluginFileOrIdentifier = tabXml->getStringAttribute("pluginFileOrIdentifier");
        tab.plugin.pluginUniqueId = tabXml->getIntAttribute("pluginUniqueId", 0);
        tab.plugin.isInstrument = tabXml->getBoolAttribute("isInstrument", tab.type == PluginSlotType::Synth);
        tab.plugin.pluginManufacturer = tabXml->getStringAttribute("pluginManufacturer");
        tab.plugin.pluginVersion = tabXml->getStringAttribute("pluginVersion");

        tab.hasPlugin = tab.plugin.pluginPath.isNotEmpty()
                     || tab.plugin.pluginPathRelative.isNotEmpty()
                     || tab.plugin.pluginPathDriveFlexible.isNotEmpty();

        session.tabs.add(tab);
    }

    if (auto* macroMappingsXml = xml.getChildByName("MacroMappings"))
    {
        for (auto* mappingXml : macroMappingsXml->getChildIterator())
        {
            if (! mappingXml->hasTagName("Macro"))
                continue;

            SessionData::MacroMapping mapping;
            mapping.macroIndex = mappingXml->getIntAttribute("macroIndex", -1);
            mapping.label = mappingXml->getStringAttribute("label");
            mapping.tabIndex = mappingXml->getIntAttribute("tabIndex", -1);
            mapping.pluginName = mappingXml->getStringAttribute("pluginName");
            mapping.parameterIndex = mappingXml->getIntAttribute("parameterIndex", -1);
            mapping.parameterName = mappingXml->getStringAttribute("parameterName");
            mapping.enabled = mappingXml->getBoolAttribute("enabled", true);

            if (mapping.macroIndex >= 0)
                session.macroMappings.add(mapping);
        }
    }

    return true;
}

bool SessionManager::saveSessionToFile(const SessionData& session, const juce::File& file)
{
    auto presetXml = createXmlFromSessionData(session);

    if (presetXml == nullptr)
        return false;

    return presetXml->writeTo(file, {});
}

bool SessionManager::loadSessionFromFile(const juce::File& file,
                                         SessionData& session,
                                         juce::StringArray& warnings)
{
    warnings.clear();

    auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr)
        return false;

    return restoreSessionDataFromXml(*xml, session, warnings);
}