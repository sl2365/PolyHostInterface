#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(settings.getWindowWidth(), settings.getWindowHeight());
    audioEngine.initialise(settings.getAudioDeviceState());
    audioEngine.getDeviceManager().addChangeListener(this);
    addAndMakeVisible(menuBar);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xFF16213E));
    tabs.setTabBarDepth(34);
    tabs.getTabbedButtonBar().setMinimumTabScaleFactor(0.7f);
    addAndMakeVisible(tabs);
    addEmptyTab();
    addEmptyTab();

    auto savedMidiDevice = settings.getMidiDeviceName();
    if (savedMidiDevice.isNotEmpty())
    {
        midiEngine.openDevice(savedMidiDevice);
        statusBar.setText("PolyHost 0.1  |  MIDI: " + midiEngine.getCurrentDeviceName() + "  |  Ready",
                          juce::dontSendNotification);
    }
    else
    {
        statusBar.setText("PolyHost 0.1  |  No MIDI device selected  |  Ready",
                          juce::dontSendNotification);
    }
    statusBar.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF0F3460));
    statusBar.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusBar.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusBar);
    markSessionClean();
    updateWindowTitle();
}

MainComponent::~MainComponent()
{
    audioEngine.getDeviceManager().removeChangeListener(this);
    settings.setAudioDeviceState(audioEngine.createAudioDeviceStateXml());
    audioEngine.shutdown();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioEngine.getDeviceManager())
    {
        settings.setAudioDeviceState(audioEngine.createAudioDeviceStateXml());
        return;
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (source == tc)
            {
                refreshTabAppearance(i);

                if (!isLoadingPreset)
                    markSessionDirty();

                return;
            }
        }
    }
}

void MainComponent::updateWindowTitle()
{
    juce::String title = "PolyHost";

    if (currentPresetFile.existsAsFile())
        title += " - " + currentPresetFile.getFileNameWithoutExtension();
    else
        title += " - Untitled";

    if (isSessionDirty)
        title += " *";

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setName(title);
}

void MainComponent::loadPluginFromFile(const juce::File& file)
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
            {
                if (tc->loadPlugin(file))
                {
                    tabs.setCurrentTabIndex(i);
                    refreshTabAppearance(i);
                    markSessionDirty();
                }
                return;
            }
        }
    }

    addEmptyTab();

    if (auto* tc = getTabComponent(tabs.getNumTabs() - 1))
    {
        if (tc->loadPlugin(file))
        {
            tabs.setCurrentTabIndex(tabs.getNumTabs() - 1);
            refreshTabAppearance(tabs.getNumTabs() - 1);
            markSessionDirty();
        }
    }
}

void MainComponent::addEmptyTab()
{
    auto* tc = new PluginTabComponent(audioEngine, tabs.getNumTabs());
    tc->addChangeListener(this);
    tabs.addTab("Empty", PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty), tc, true);
    markSessionDirty();
}

void MainComponent::refreshTabAppearance(int index)
{
    if (auto* tc = getTabComponent(index))
    {
        juce::String name = "Empty";

        if (tc->hasPlugin())
            name = tc->getPluginName();

        tabs.setTabName(index, name);
        tabs.setTabBackgroundColour(index, PluginTabComponent::colourForType(tc->getType()));
    }
}

int MainComponent::countTabsOfType(PluginTabComponent::SlotType type) const
{
    int count = 0;
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i)) if (tc->getType() == type) ++count;
    return count;
}

PluginTabComponent* MainComponent::getTabComponent(int index) const
{
    return dynamic_cast<PluginTabComponent*>(tabs.getTabContentComponent(index));
}

void MainComponent::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF1A1A2E)); }

void MainComponent::resized()
{
    auto area = getLocalBounds();
    menuBar.setBounds(area.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
    statusBar.setBounds(area.removeFromBottom(24));
    tabs.setBounds(area);
}

juce::StringArray MainComponent::getMenuBarNames() { return { "File", "Audio", "MIDI", "Recording", "Options", "Help" }; }

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    switch (index)
    {
        case 0:
            menu.addItem(100, "New Preset");
            menu.addSeparator();
            menu.addItem(101, "Add Tab");
            menu.addSeparator();
            menu.addItem(103, "Save Preset");
            menu.addItem(104, "Save Preset As...");
            menu.addItem(105, "Load Preset...");
            menu.addItem(107, "Locate Missing Plugins...",
                         !unresolvedMissingPlugins.isEmpty());
            menu.addItem(106, "Delete Preset...");
            menu.addSeparator();
            menu.addItem(199, "Quit");
            break;
        case 1: menu.addItem(201, "Audio Device Settings"); break;
        case 2: menu.addItem(301, "Select MIDI Input Device"); menu.addItem(302, "MIDI Monitor"); break;
        case 3: menu.addItem(401, "Record Audio...  [TODO]"); menu.addItem(402, "Record MIDI...  [TODO]"); break;
        case 4: menu.addItem(501, "Preferences...  [TODO]"); break;
        case 5: menu.addItem(601, "About PolyHost"); break;
        default: break;
    }
    return menu;
}

void MainComponent::menuItemSelected(int itemId, int)
{
    switch (itemId)
    {
        case 100: newPreset(); break;
        case 101: addEmptyTab(); break;
        case 103: savePreset(); break;
        case 104: savePresetAs(); break;
        case 105: loadPreset(); break;
        case 107: locateMissingPlugins(); break;
        case 106: deletePreset(); break;
        case 199:
            if (requestQuit())
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        case 201:
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                audioEngine.getDeviceManager(),
                0, 2,
                0, 2,
                true,
                true,
                true,
                false);

            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned(selector);
            o.dialogTitle                  = "Audio Device Settings";
            o.dialogBackgroundColour       = juce::Colours::darkgrey;
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar            = true;
            o.resizable                    = true;

            auto* w = o.launchAsync();
            w->centreWithSize(500, 400);
            break;
        }
        case 301:
        {
            juce::PopupMenu midiMenu;
            auto names = midiEngine.getAvailableDeviceNames();

            if (names.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "MIDI Input",
                    "No MIDI input devices found.");
                break;
            }

            for (int i = 0; i < names.size(); ++i)
                midiMenu.addItem(1000 + i, names[i]);

            midiMenu.showMenuAsync(
                juce::PopupMenu::Options(),
                [this, names](int result)
                {
                    if (result >= 1000 && result < 1000 + names.size())
                    {
                        auto selectedName = names[result - 1000];
                        midiEngine.openDevice(selectedName);
                        settings.setMidiDeviceName(selectedName);
                        statusBar.setText(
                            "PolyHost 0.1  |  MIDI: " + midiEngine.getCurrentDeviceName() + "  |  Ready",
                            juce::dontSendNotification);
                    }
                });

            break;
        }
        case 302:
        {
            if (midiMonitorWindow == nullptr)
                midiMonitorWindow = std::make_unique<MidiMonitorWindow>(midiEngine);

            midiMonitorWindow->setVisible(true);
            midiMonitorWindow->toFront(true);
            break;
        }
        case 601:
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "About PolyHost",
                "PolyHost v0.1\n\nA lightweight tabbed plugin host for VST3 and CLAP.\nBuilt with JUCE.\n\nVST2 and 32-bit bridging planned.");
            break;
        default: break;
    }
}

void MainComponent::clearAllPlugins()
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();
    markSessionDirty();
}

void MainComponent::deletePreset()
{
    juce::FileChooser chooser("Delete Preset",
                              AppSettings::getPresetsDirectory(),
                              "*.xml");

    if (!chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();

    if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                           "Delete Preset",
                                           "Delete preset:\n" + file.getFileName() + "?"))
    {
        if (file.deleteFile() && file == currentPresetFile)
        {
            currentPresetFile = {};
            updateWindowTitle();
        }
    }
}

void MainComponent::loadPreset()
{
    if (!maybeSaveChanges())
        return;

    juce::FileChooser chooser("Load Preset",
                              AppSettings::getPresetsDirectory(),
                              "*.xml");

    if (!chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();
    auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr || !xml->hasTagName("PolyHostPreset"))
        return;

    isLoadingPreset = true;
    unresolvedMissingPlugins.clear();

    juce::StringArray loadErrors;
    juce::Array<MissingPluginEntry> missingPlugins;

    rebuildTabsFromPresetXml(*xml);

    int tabIndex = 0;
    for (auto* tabXml : xml->getChildIterator())
    {
        if (!tabXml->hasTagName("Tab"))
            continue;

        if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
            break;

        if (auto* tc = getTabComponent(tabIndex))
        {
            auto pluginName               = tabXml->getStringAttribute("pluginName", "Unknown Plugin");
            auto pluginPath               = tabXml->getStringAttribute("pluginPath");
            auto pluginPathRelative       = tabXml->getStringAttribute("pluginPathRelative");
            auto pluginPathDriveFlexible  = tabXml->getStringAttribute("pluginPathDriveFlexible");
            auto stateBase64              = tabXml->getStringAttribute("pluginState");
            auto typeString = tabXml->getStringAttribute("type");
            auto pluginFormatName        = tabXml->getStringAttribute("pluginFormatName");
            auto isInstrument            = tabXml->getBoolAttribute("isInstrument", typeString == "Synth");
            auto pluginManufacturer      = tabXml->getStringAttribute("pluginManufacturer");
            auto pluginVersion           = tabXml->getStringAttribute("pluginVersion");

            if (pluginPath.isNotEmpty() || pluginPathRelative.isNotEmpty() || pluginPathDriveFlexible.isNotEmpty())
            {
                auto pluginFile = AppSettings::resolvePluginPath(pluginPath,
                                                                 pluginPathRelative,
                                                                 pluginPathDriveFlexible);

                if (pluginFile == juce::File())
                {
                    juce::String displayPath = pluginPathRelative.isNotEmpty() ? pluginPathRelative : pluginPath;
                    if (displayPath.isEmpty())
                        displayPath = pluginPathDriveFlexible;

                    loadErrors.add("Missing plugin: " + pluginName + "\n  Path: " + displayPath);

                    MissingPluginEntry entry;
                    entry.tabIndex = tabIndex;
                    entry.slotType = (typeString == "Synth")
                        ? PluginTabComponent::SlotType::Synth
                        : PluginTabComponent::SlotType::FX;
                    entry.pluginName = pluginName;
                    entry.pluginPath = pluginPath;
                    entry.pluginPathRelative = pluginPathRelative;
                    entry.pluginPathDriveFlexible = pluginPathDriveFlexible;
                    entry.pluginStateBase64 = stateBase64;
                    entry.pluginFormatName = pluginFormatName;
                    entry.isInstrument = isInstrument;
                    entry.pluginManufacturer = pluginManufacturer;
                    entry.pluginVersion = pluginVersion;
                    missingPlugins.add(entry);
                }
                else if (!tc->loadPlugin(pluginFile))
                {
                    loadErrors.add("Failed to load plugin: " + pluginName + "\n  Path: " + pluginFile.getFullPathName());
                }
                else
                {
                    juce::MemoryBlock state;
                    if (state.fromBase64Encoding(stateBase64))
                        tc->restorePluginState(state);

                    refreshTabAppearance(tabIndex);
                }
            }
        }

        ++tabIndex;
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    auto selectedTab = xml->getIntAttribute("selectedTab", 0);
    if (juce::isPositiveAndBelow(selectedTab, tabs.getNumTabs()))
        tabs.setCurrentTabIndex(selectedTab);

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
    {
        auto w = xml->getIntAttribute("windowWidth", top->getWidth());
        auto h = xml->getIntAttribute("windowHeight", top->getHeight());
        top->setSize(juce::jmax(900, w), juce::jmax(550, h));
    }

    unresolvedMissingPlugins = missingPlugins;
    currentPresetFile = file;
    showPresetLoadErrors(loadErrors);

    if (promptToLocateMissingPlugins(unresolvedMissingPlugins))
    {
        isLoadingPreset = false;
        locateMissingPlugins();
    }
    else
    {
        juce::MessageManager::callAsync([this]
        {
            isLoadingPreset = false;
            markSessionClean();
        });
    }
}

void MainComponent::showPresetLoadErrors(const juce::StringArray& errors)
{
    if (errors.isEmpty())
        return;

    juce::String message = "Some plugins could not be restored:\n\n";

    for (int i = 0; i < errors.size(); ++i)
    {
        message += errors[i];

        if (i < errors.size() - 1)
            message += "\n\n";
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon,
        "Preset Load Warnings",
        message);
}

bool MainComponent::locateMissingPlugin(MissingPluginEntry& entry)
{
    juce::File startDir = AppSettings::getAppDirectory();

    if (lastPluginRepairDirectory.isDirectory())
    {
        startDir = lastPluginRepairDirectory;
    }
    else if (entry.pluginPath.isNotEmpty())
    {
        juce::File originalFile(entry.pluginPath);
        auto parent = originalFile.getParentDirectory();
        if (parent.exists())
            startDir = parent;
    }

    juce::File replacementFile = tryAutoLocateReplacement(entry);

    if (replacementFile == juce::File())
    {
        juce::FileChooser chooser("Locate Plugin: " + entry.pluginName,
                                  startDir,
                                  "*.vst3;*.dll;*.clap");

        if (!chooser.browseForFileToOpen())
            return false;

        replacementFile = chooser.getResult();
    }

    if (!confirmReplacementPlugin(entry, replacementFile))
        return false;

    if (!validateReplacementPluginCompatibility(entry, replacementFile))
        return false;

    if (!juce::isPositiveAndBelow(entry.tabIndex, tabs.getNumTabs()))
        return false;

    if (auto* tc = getTabComponent(entry.tabIndex))
    {
        if (!tc->loadPlugin(replacementFile))
            return false;

        lastPluginRepairDirectory = replacementFile.getParentDirectory();

        juce::MemoryBlock state;
        if (state.fromBase64Encoding(entry.pluginStateBase64))
            tc->restorePluginState(state);

        refreshTabAppearance(entry.tabIndex);
        markSessionDirty();
        return true;
    }

    return false;
}

void MainComponent::locateMissingPlugins()
{
    if (unresolvedMissingPlugins.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Locate Missing Plugins",
            "There are no unresolved missing plugins.");
        return;
    }

    juce::StringArray restored;
    juce::StringArray skipped;
    juce::Array<MissingPluginEntry> stillMissing;

    for (int i = 0; i < unresolvedMissingPlugins.size(); ++i)
    {
        auto entry = unresolvedMissingPlugins.getReference(i);

        if (locateMissingPlugin(entry))
            restored.add(entry.pluginName);
        else
        {
            skipped.add(entry.pluginName);
            stillMissing.add(entry);
        }
    }

    unresolvedMissingPlugins = stillMissing;

    juce::StringArray failed;
    showMissingPluginRepairResult(restored, failed, skipped);
}

bool MainComponent::promptToLocateMissingPlugins(const juce::Array<MissingPluginEntry>& missingPlugins)
{
    if (missingPlugins.isEmpty())
        return false;

    juce::String message = "Some plugins could not be found.\n\n";
    message += "Would you like to locate them now?\n\n";

    for (int i = 0; i < missingPlugins.size(); ++i)
    {
        message += missingPlugins.getReference(i).pluginName;

        if (i < missingPlugins.size() - 1)
            message += "\n";
    }

    return juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                                              "Locate Missing Plugins",
                                              message,
                                              "Locate",
                                              "Later");
}

bool MainComponent::confirmReplacementPlugin(const MissingPluginEntry& entry, const juce::File& replacementFile)
{
    juce::String expectedType = entry.isInstrument ? "Synth" : "FX";

    juce::String message = "Use this file as the replacement plugin?\n\n";
    message += juce::String("Expected: ") + entry.pluginName + "\n";
    message += juce::String("Expected Type: ") + expectedType + "\n";

    if (entry.pluginFormatName.isNotEmpty())
        message += juce::String("Expected Format: ") + entry.pluginFormatName + "\n";

    if (entry.pluginManufacturer.isNotEmpty())
        message += juce::String("Expected Manufacturer: ") + entry.pluginManufacturer + "\n";

    if (entry.pluginVersion.isNotEmpty())
        message += juce::String("Expected Version: ") + entry.pluginVersion + "\n";

    message += juce::String("Selected: ") + replacementFile.getFileName() + "\n\n";
    message += replacementFile.getFullPathName();

    return juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                                              "Confirm Replacement Plugin",
                                              message,
                                              "Use Replacement",
                                              "Cancel");
}

void MainComponent::showMissingPluginRepairResult(const juce::StringArray& restored,
                                                  const juce::StringArray& failed,
                                                  const juce::StringArray& skipped)
{
    juce::String message;

    if (!restored.isEmpty())
    {
        message += "Restored:\n";
        for (int i = 0; i < restored.size(); ++i)
            message += "  - " + restored[i] + "\n";
        message += "\n";
    }

    if (!failed.isEmpty())
    {
        message += "Failed:\n";
        for (int i = 0; i < failed.size(); ++i)
            message += "  - " + failed[i] + "\n";
        message += "\n";
    }

    if (!skipped.isEmpty())
    {
        message += "Not repaired:\n";
        for (int i = 0; i < skipped.size(); ++i)
            message += "  - " + skipped[i] + "\n";
        message += "\n";
    }

    if (unresolvedMissingPlugins.isEmpty())
        message += "All missing plugins have been resolved.\n";
    else
        message += "Remaining unresolved plugins: " + juce::String(unresolvedMissingPlugins.size()) + "\n";

    if (!restored.isEmpty())
    {
        message += "\nSave the preset now to store repaired plugin paths.";

        auto saveNow = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::InfoIcon,
            "Missing Plugin Repair Complete",
            message.trimEnd(),
            "Save Now",
            "Later");

        if (saveNow)
        {
            if (currentPresetFile.existsAsFile())
                savePreset();
            else
                savePresetAs();
        }

        return;
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Missing Plugin Repair Complete",
        message.trimEnd());
}

bool MainComponent::scanPluginFile(const juce::File& pluginFile, juce::PluginDescription& desc) const
{
    if (!pluginFile.existsAsFile())
        return false;

    juce::AudioPluginFormatManager formatManager;
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
   #endif

    juce::OwnedArray<juce::PluginDescription> results;

    for (auto* format : formatManager.getFormats())
    {
        format->findAllTypesForFile(results, pluginFile.getFullPathName());
        if (!results.isEmpty())
            break;
    }

    if (results.isEmpty())
        return false;

    desc = *results.getFirst();
    return true;
}

bool MainComponent::validateReplacementPluginCompatibility(const MissingPluginEntry& entry,
                                                          const juce::File& replacementFile)
{
    juce::PluginDescription desc;
    if (!scanPluginFile(replacementFile, desc))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Replacement Plugin",
            juce::String("Could not scan the selected plugin file:\n\n") + replacementFile.getFullPathName());
        return false;
    }

    juce::StringArray mismatches;

    auto selectedType = desc.isInstrument ? PluginTabComponent::SlotType::Synth
                                          : PluginTabComponent::SlotType::FX;

    if (selectedType != entry.slotType)
    {
        juce::String expectedType = (entry.slotType == PluginTabComponent::SlotType::Synth) ? "Synth" : "FX";
        juce::String selectedTypeName = (selectedType == PluginTabComponent::SlotType::Synth) ? "Synth" : "FX";

        mismatches.add("Type: expected " + expectedType + ", selected " + selectedTypeName);
    }

    if (entry.pluginFormatName.isNotEmpty()
        && !desc.pluginFormatName.equalsIgnoreCase(entry.pluginFormatName))
    {
        mismatches.add("Format: expected " + entry.pluginFormatName
                       + ", selected " + desc.pluginFormatName);
    }

    if (entry.pluginManufacturer.isNotEmpty()
        && !desc.manufacturerName.equalsIgnoreCase(entry.pluginManufacturer))
    {
        mismatches.add("Manufacturer: expected " + entry.pluginManufacturer
                       + ", selected " + desc.manufacturerName);
    }

    if (entry.pluginVersion.isNotEmpty()
        && desc.version != entry.pluginVersion)
    {
        mismatches.add("Version: expected " + entry.pluginVersion
                       + ", selected " + desc.version);
    }

    if (mismatches.isEmpty())
        return true;

    juce::String message = "The selected replacement plugin does not fully match the original.\n\n";
    message += "Detected mismatches:\n";

    for (int i = 0; i < mismatches.size(); ++i)
        message += juce::String("  - ") + mismatches[i] + "\n";

    message += "\nDo you still want to use it?";

    return juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Replacement Plugin Mismatch",
        message,
        "Use Anyway",
        "Cancel");
}

juce::String MainComponent::getExpectedPluginFileName(const MissingPluginEntry& entry) const
{
    if (entry.pluginPath.isNotEmpty())
        return juce::File(entry.pluginPath).getFileName();

    if (entry.pluginPathRelative.isNotEmpty())
        return juce::File(entry.pluginPathRelative).getFileName();

    return {};
}

juce::File MainComponent::tryAutoLocateReplacement(const MissingPluginEntry& entry) const
{
    if (!lastPluginRepairDirectory.isDirectory())
        return {};

    auto expectedFileName = getExpectedPluginFileName(entry);
    if (expectedFileName.isEmpty())
        return {};

    auto candidate = lastPluginRepairDirectory.getChildFile(expectedFileName);

    if (candidate.existsAsFile())
        return candidate;

    return {};
}

void MainComponent::markSessionDirty()
{
    isSessionDirty = true;
    updateWindowTitle();
}

void MainComponent::markSessionClean()
{
    isSessionDirty = false;
    updateWindowTitle();
}

void MainComponent::newPreset()
{
    if (!maybeSaveChanges())
        return;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();

    addEmptyTab();
    addEmptyTab();

    currentPresetFile = {};
    unresolvedMissingPlugins.clear();
    markSessionClean();
}

bool MainComponent::requestQuit()
{
    return maybeSaveChanges();
}

bool MainComponent::maybeSaveChanges()
{
    if (!isSessionDirty)
        return true;

    auto result = juce::NativeMessageBox::showYesNoCancelBox(
        juce::AlertWindow::WarningIcon,
        "Unsaved Changes",
        "Save changes to the current preset before continuing?");

    if (result == 0) // cancel
        return false;

    if (result == 1) // yes
    {
        if (currentPresetFile == juce::File())
            savePresetAs();
        else
            savePreset();

        return !isSessionDirty;
    }

    if (result == 2) // no
        return true;

    return true;
}

void MainComponent::savePreset()
{
    if (currentPresetFile == juce::File())
    {
        savePresetAs();
        return;
    }

    if (writePresetToFile(currentPresetFile))
        markSessionClean();
}

void MainComponent::savePresetAs()
{
    juce::AlertWindow w("Save Preset As", "Enter a preset name:", juce::AlertWindow::NoIcon);
    w.addTextEditor("name",
                    currentPresetFile.existsAsFile() ? currentPresetFile.getFileNameWithoutExtension() : "",
                    "Preset Name:");
    w.addButton("Save", 1);
    w.addButton("Cancel", 0);

    if (w.runModalLoop() != 1)
        return;

    auto presetName = w.getTextEditorContents("name").trim();
    if (presetName.isEmpty())
        return;

    auto file = AppSettings::getPresetsDirectory().getChildFile(presetName + ".xml");

    if (writePresetToFile(file))
    {
        currentPresetFile = file;
        markSessionClean();
    }
}

bool MainComponent::writePresetToFile(const juce::File& file)
{
    juce::XmlElement presetXml("PolyHostPreset");
    presetXml.setAttribute("name", file.getFileNameWithoutExtension());
    presetXml.setAttribute("selectedTab", tabs.getCurrentTabIndex());

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
    {
        presetXml.setAttribute("windowWidth", top->getWidth());
        presetXml.setAttribute("windowHeight", top->getHeight());
    }
    else
    {
        presetXml.setAttribute("windowWidth", getWidth());
        presetXml.setAttribute("windowHeight", getHeight());
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            auto tabXml = std::make_unique<juce::XmlElement>("Tab");
            tabXml->setAttribute("index", i);
            tabXml->setAttribute("type", tc->getType() == PluginTabComponent::SlotType::Synth ? "Synth" : "FX");
            tabXml->setAttribute("tabName", tabs.getTabNames()[i]);

            if (tc->hasPlugin())
            {
                auto pluginFile = tc->getPluginFile();

                tabXml->setAttribute("pluginName", tc->getPluginName());
                tabXml->setAttribute("pluginPath", pluginFile.getFullPathName());

                auto relativePath = AppSettings::makePathPortable(pluginFile);
                if (relativePath.isNotEmpty())
                    tabXml->setAttribute("pluginPathRelative", relativePath);

                auto driveFlexiblePath = AppSettings::getDriveFlexiblePath(pluginFile);
                if (driveFlexiblePath.isNotEmpty())
                    tabXml->setAttribute("pluginPathDriveFlexible", driveFlexiblePath);

                juce::PluginDescription desc;
                if (scanPluginFile(pluginFile, desc))
                {
                    tabXml->setAttribute("pluginFormatName", desc.pluginFormatName);
                    tabXml->setAttribute("isInstrument", desc.isInstrument);
                    tabXml->setAttribute("pluginManufacturer", desc.manufacturerName);
                    tabXml->setAttribute("pluginVersion", desc.version);
                }

                auto state = tc->getPluginState();
                tabXml->setAttribute("pluginState", state.toBase64Encoding());
            }

            presetXml.addChildElement(tabXml.release());
        }
    }

    return presetXml.writeTo(file, {});
}

void MainComponent::rebuildTabsFromPresetXml(const juce::XmlElement& presetXml)
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
    if (auto* tc = getTabComponent(i))
        tc->removeChangeListener(this);

    tabs.clearTabs();

    for (auto* tabXml : presetXml.getChildIterator())
    {
        if (!tabXml->hasTagName("Tab"))
            continue;

        auto* tc = new PluginTabComponent(audioEngine, tabs.getNumTabs());
        tc->addChangeListener(this);
        tabs.addTab("Empty",
                    PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty),
                    tc,
                    true);
    }

    if (tabs.getNumTabs() == 0)
        addEmptyTab();
}