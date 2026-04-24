#include "MainComponent.h"
#include <cmath>

namespace
{
    class MidiAssignmentsCallout final : public juce::Component
    {
    public:
        MidiAssignmentsCallout(const juce::Array<juce::MidiDeviceInfo>& availableDevices,
                               const juce::StringArray& assignedDeviceIdentifiers,
                               std::function<void(const juce::String& deviceIdentifier)> onToggle)
            : toggleAssignment(std::move(onToggle))
        {
            for (auto& device : availableDevices)
            {
                auto* button = deviceButtons.add(new juce::ToggleButton(device.name));
                button->setToggleState(assignedDeviceIdentifiers.contains(device.identifier),
                                       juce::dontSendNotification);

                const auto identifier = device.identifier;
                button->onClick = [this, identifier]
                {
                    if (toggleAssignment)
                        toggleAssignment(identifier);
                };

                addAndMakeVisible(button);
            }

            addAndMakeVisible(closeButton);
            closeButton.setButtonText("Close");
            closeButton.onClick = [this]
            {
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->setVisible(false);
            };

            setSize(260, 40 + (deviceButtons.size() * 28) + 40);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);

            for (auto* button : deviceButtons)
                button->setBounds(area.removeFromTop(28));

            area.removeFromTop(8);
            closeButton.setBounds(area.removeFromTop(28).removeFromRight(80));
        }

    private:
        juce::OwnedArray<juce::ToggleButton> deviceButtons;
        juce::TextButton closeButton;
        std::function<void(const juce::String& deviceIdentifier)> toggleAssignment;
    };
}

MainComponent::MainComponent(bool shouldLoadLastPreset)
{
    setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);
    audioEngine.initialise(settings.getAudioDeviceState());
    audioEngine.getDeviceManager().addChangeListener(this);
    midiEngine.addChangeListener(this);
    addAndMakeVisible(menuBar);
    toolbar.addDefaultItems(*this);
    addAndMakeVisible(toolbar);
    toolbar.setColour(juce::Toolbar::backgroundColourId, juce::Colour(0xFF1D2230));

    addAndMakeVisible(beatIndicator);

    addAndMakeVisible(tempoEditor);
    startTimerHz(10);
    tempoEditor.setInputRestrictions(6, "0123456789.");
    tempoEditor.setJustification(juce::Justification::centred);
    tempoEditor.addListener(this);
    tempoEditor.setSelectAllWhenFocused(true);
    tempoEditor.onReturnKey = [this]
    {
        setHostTempoBpm(tempoEditor.getText().getDoubleValue());
    };
    tempoEditor.onFocusLost = [this]
    {
        setHostTempoBpm(tempoEditor.getText().getDoubleValue());
    };

    addAndMakeVisible(tapTempoButton);
    tapTempoButton.onClick = [this]
    {
        registerTapTempo();
    };

    addAndMakeVisible(presetDropdown);
    presetDropdown.setTextWhenNothingSelected("Select Preset");
    presetDropdown.onBeforePopup = [this]
    {
        refreshPresetLists();
    };
    presetDropdown.onChange = [this]
    {
        loadPresetFromDropdown();
    };

    refreshPresetDropdown();

    refreshTempoUiFromEngine();
    audioEngine.setTempoBpm(transportState.hostTempoBpm);

    tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xFF16213E));
    tabs.setTabBarDepth(34);
    tabs.getTabbedButtonBar().setMinimumTabScaleFactor(0.7f);
    tabs.getTabbedButtonBar().setLookAndFeel(&tabBarLookAndFeel);
    tabs.getTabbedButtonBar().addMouseListener(&tabBarMouseListener, true);
    addAndMakeVisible(tabs);
    updateStatusBarText();

    routingView.onShowMidiAssignments = [this](int tabIndex, juce::Component* anchorComponent)
    {
        showMidiAssignmentsCallout(tabIndex, anchorComponent);
    };

    routingView.onRefreshMidiDevices = [this]
    {
        refreshMidiDevices();
    };

    addAndMakeVisible(routingView);
    routingView.setVisible(false);
    routingView.onToggleBypass = [this](int tabIndex)
    {
        toggleTabBypass(tabIndex);
    };

    routingView.onSelectTab = [this](int tabIndex)
    {
        if (juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        {
            tabs.setCurrentTabIndex(tabIndex);
            setRoutingViewVisible(false);
        }
    };

    routingView.onMoveUp = [this](int tabIndex)
    {
        moveTabEarlier(tabIndex);
    };

    routingView.onMoveDown = [this](int tabIndex)
    {
        moveTabLater(tabIndex);
    };

    addEmptyTab();
    addEmptyTab();

    if (shouldLoadLastPreset)
    {
        auto lastPresetPath = settings.getLastPresetPath().trim();

        if (lastPresetPath.isNotEmpty())
        {
            juce::File lastPresetFile(lastPresetPath);

            if (lastPresetFile.existsAsFile())
                loadPresetFromFile(lastPresetFile);
        }
    }

    refreshRoutingView();

    auto enabledMidiDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

    for (auto& identifier : enabledMidiDeviceIdentifiers)
        midiEngine.openDevice(identifier);

    auto openNames = midiEngine.getOpenDeviceNames();

    if (!openNames.isEmpty())
    {
        statusBar.setText("PolyHost 0.1  |  MIDI: " + openNames.joinIntoString(", ") + "  |  Ready",
                          juce::dontSendNotification);
    }
    else
    {
        auto legacySavedMidiDevice = settings.getMidiDeviceName();

        if (legacySavedMidiDevice.isNotEmpty())
        {
            midiEngine.openDevice(legacySavedMidiDevice);
            statusBar.setText("PolyHost 0.1  |  MIDI: " + midiEngine.getCurrentDeviceName() + "  |  Ready",
                              juce::dontSendNotification);
        }
        else
        {
            statusBar.setText("PolyHost 0.1  |  No MIDI device selected  |  Ready",
                              juce::dontSendNotification);
        }
    }

    midiEngine.onMidiMessageReceived = [this](const MidiEngine::IncomingMidiMessage& incoming)
    {
        juce::Array<juce::AudioProcessorGraph::NodeID> targetNodeIds;

        for (int i = 0; i < tabs.getNumTabs(); ++i)
        {
            auto* tc = getTabComponent(i);
            if (tc == nullptr || !tc->hasPlugin())
                continue;

            if (tc->isBypassed())
                continue;

            if (auto* midiState = getMidiRoutingStateForTab(i))
            {
                if (midiState->assignedDeviceIdentifiers.contains(incoming.deviceIdentifier))
                    targetNodeIds.add(tc->getNodeID());
            }
        }

        if (!targetNodeIds.isEmpty())
            audioEngine.queueMidiToNodes(incoming.message, targetNodeIds);
    };

    statusBar.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF0F3460));
    statusBar.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusBar.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusBar);

    markSessionClean();
    updateWindowTitle();
}

MainComponent::~MainComponent()
{
    tabs.getTabbedButtonBar().removeMouseListener(&tabBarMouseListener);
    tabs.getTabbedButtonBar().setLookAndFeel(nullptr);
    audioEngine.getDeviceManager().removeChangeListener(this);
    midiEngine.removeChangeListener(this);
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

    if (source == &midiEngine)
    {
        refreshRoutingView();
        updateStatusBarText();
        return;
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (source == tc)
            {
                refreshTabAppearance(i);
                syncRoutingToAudioEngine();
                refreshRoutingView();
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
                    refreshRoutingView();
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
            refreshRoutingView();
            markSessionDirty();
        }
    }
}

// ===================================
// TABS
// ===================================
void MainComponent::configurePluginTabComponent(PluginTabComponent& tabComponent)
{
    tabComponent.onOpenDroppedPluginInNewTab = [this](const juce::File& file)
    {
        addEmptyTab();

        const int newTabIndex = tabs.getNumTabs() - 1;

        if (auto* newTab = getTabComponent(newTabIndex))
        {
            if (newTab->loadPlugin(file))
            {
                tabs.setCurrentTabIndex(newTabIndex);
                refreshTabAppearance(newTabIndex);
                refreshRoutingView();
                markSessionDirty();
            }
        }
    };
}

void MainComponent::addEmptyTab()
{
    auto* tc = new PluginTabComponent(audioEngine, tabs.getNumTabs());
    configurePluginTabComponent(*tc);
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

        auto baseColour = PluginTabComponent::colourForType(tc->getType());
        const bool isSelected = (index == tabs.getCurrentTabIndex());

        auto tabColour = isSelected
            ? baseColour.brighter(0.10f)
            : baseColour.darker(0.50f);

        tabs.setTabName(index, name);
        tabs.setTabBackgroundColour(index, tabColour);
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

void MainComponent::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF1D2230)); }

void MainComponent::resized()
{
    auto area = getLocalBounds();

    menuBar.setBounds(area.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));

    // Toolbar  Button height
    auto topRow = area.removeFromTop(32);

    auto presetArea = topRow.removeFromLeft(280);
    presetArea.removeFromLeft(8);
    presetArea.removeFromRight(6);

    auto presetBounds = presetArea.reduced(2);
    presetBounds = presetBounds.withSizeKeepingCentre(presetBounds.getWidth(), topRow.getHeight() - 4);
    presetDropdown.setBounds(presetBounds);

    auto tempoArea = topRow.removeFromRight(286);
    tempoArea.removeFromRight(6);

    auto tapBounds = tempoArea.removeFromRight(48).reduced(2);
    tapBounds = tapBounds.withSizeKeepingCentre(tapBounds.getWidth(), topRow.getHeight() - 4);
    tapTempoButton.setBounds(tapBounds);

    tempoArea.removeFromRight(4);

    auto editorBounds = tempoArea.removeFromRight(70).reduced(2);
    editorBounds = editorBounds.withSizeKeepingCentre(editorBounds.getWidth(), 22);
    tempoEditor.setBounds(editorBounds);

    tempoArea.removeFromRight(6);

    auto beatBounds = tempoArea.removeFromRight(96).reduced(2);
    beatBounds = beatBounds.withSizeKeepingCentre(beatBounds.getWidth(), 18);
    beatIndicator.setBounds(beatBounds);

    toolbar.setBounds(topRow);

    statusBar.setBounds(area.removeFromBottom(24));

    tabs.setBounds(area);
    routingView.setBounds(area);
}

juce::StringArray MainComponent::getMenuBarNames() { return { "File", "Audio", "MIDI", "Recording", "Options", "Help" }; }

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    switch (index)
    {
        case 0:
        {
            refreshPresetLists();

            menu.addItem(100, "New Preset");
            menu.addSeparator();
            menu.addItem(101, "New Tab");
            menu.addItem(102, "Close Current Tab");
            menu.addSeparator();
            menu.addItem(103, "Save Preset");
            menu.addItem(104, "Save Preset As...");
            menu.addItem(105, "Load Preset");

            juce::PopupMenu recentMenu;
            settings.removeMissingRecentPresetPaths();
            auto recentPresetPaths = settings.getRecentPresetPaths();

            int recentItemCount = 0;

            for (int i = 0; i < recentPresetPaths.size(); ++i)
            {
                juce::File presetFile(recentPresetPaths[i]);

                if (!presetFile.existsAsFile())
                    continue;

                recentMenu.addItem(menuRecentPresetBase + i,
                                   presetFile.getFileNameWithoutExtension());
                ++recentItemCount;
            }

            if (recentItemCount == 0)
                recentMenu.addItem(1, "(No Recent Presets)", false, false);

            menu.addSubMenu("Recent Presets", recentMenu);

            menu.addItem(107, "Locate Missing Plugins...",
                         !unresolvedMissingPlugins.isEmpty());
            menu.addItem(106, "Delete Current Preset");
            menu.addSeparator();
            menu.addItem(menuOpenPresetsFolder, "Open Presets Folder");
            menu.addSeparator();
            menu.addItem(199, "Quit");
            break;
        }
        case 1:
            menu.addItem(201, "Audio Device Settings");
            menu.addSeparator();
            menu.addItem(202, "Routing");
            break;
        case 2:
            menu.addItem(301, "Select MIDI Input Device");
            menu.addItem(302, "MIDI Monitor");
            menu.addSeparator();
            menu.addItem(menuHostSyncToggle, "Host Sync", true, transportState.hostSyncEnabled);
            menu.addSeparator();
            menu.addItem(menuSetFakeHostTempo, "Set Fake Host Tempo 128.0");
            menu.addItem(menuClearFakeHostTempo, "Clear Fake Host Tempo");
            break;
        case 3: menu.addItem(401, "Record Audio...  [TODO]"); menu.addItem(402, "Record MIDI...  [TODO]"); break;
        case 4:
            menu.addItem(501,
                         "Auto-Save Preset After Plugin Repair",
                         true,
                         settings.getAutoSaveAfterPluginRepair());
            menu.addSeparator();
            menu.addItem(502, "Add Plugin Scan Folder...");
            menu.addItem(503, "Show Plugin Scan Folders");
            menu.addItem(504, "Clear Plugin Scan Folders",
                         settings.getPluginScanFolders().size() > 0);
            menu.addSeparator();
            menu.addItem(500, "Preferences...  [TODO]");
            break;
        case 5: menu.addItem(601, "About PolyHost"); break;
        default: break;
    }
    return menu;
}

void MainComponent::menuItemSelected(int itemId, int)
{
    if (itemId >= menuRecentPresetBase && itemId < menuRecentPresetBase + 100)
    {
        auto recentPresetPaths = settings.getRecentPresetPaths();
        int index = itemId - menuRecentPresetBase;

        if (juce::isPositiveAndBelow(index, recentPresetPaths.size()))
        {
            juce::File file(recentPresetPaths[index]);

            if (file.existsAsFile())
            {
                if (!maybeSaveChanges())
                    return;

                loadPresetFromFile(file);
            }
        }

        return;
    }
    switch (itemId)
    {
        case 100: newPreset(); break;
        case 101: addEmptyTab(); break;
        case 102: closeCurrentTab(); break;
        case 103: savePreset(); break;
        case 104: savePresetAs(); break;
        case 105: loadPreset(); break;
        case 107: locateMissingPlugins(); break;
        case 106: deletePreset(); break;
        case menuOpenPresetsFolder:
            AppSettings::getPresetsDirectory().startAsProcess();
            break;
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
        case 202:
            toggleRoutingView();
            break;

        case 301:
        {
            juce::PopupMenu midiMenu;
            auto devices = midiEngine.getAvailableDevices();

            if (devices.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "MIDI Input",
                    "No MIDI input devices found.");
                break;
            }

            for (int i = 0; i < devices.size(); ++i)
            {
                const auto& device = devices.getReference(i);
                midiMenu.addItem(1000 + i,
                                 device.name,
                                 true,
                                 midiEngine.isDeviceOpen(device.identifier));
            }

            midiMenu.showMenuAsync(
                juce::PopupMenu::Options(),
                [this, devices](int result)
                {
                    if (result >= 1000 && result < 1000 + devices.size())
                    {
                        const auto& device = devices.getReference(result - 1000);

                        if (midiEngine.isDeviceOpen(device.identifier))
                            midiEngine.closeDevice(device.identifier);
                        else
                            midiEngine.openDevice(device.identifier);

                        auto openNames = midiEngine.getOpenDeviceNames();

                        if (openNames.isEmpty())
                        {
                            statusBar.setText(
                                "PolyHost 0.1  |  No MIDI device selected  |  Ready",
                                juce::dontSendNotification);
                            settings.setMidiDeviceName({});
                            settings.setEnabledMidiDeviceIdentifiers({});
                        }
                        else
                        {
                            statusBar.setText(
                                "PolyHost 0.1  |  MIDI: " + openNames.joinIntoString(", ") + "  |  Ready",
                                juce::dontSendNotification);

                            settings.setMidiDeviceName(openNames[0]); // legacy compatibility
                            settings.setEnabledMidiDeviceIdentifiers(midiEngine.getOpenDeviceIdentifiers());
                        }
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
        case menuHostSyncToggle:
            transportState.hostSyncEnabled = !transportState.hostSyncEnabled;
            audioEngine.setHostSyncEnabled(transportState.hostSyncEnabled);
            lastDisplayedSyncedTempoBpm = -1.0;
            lastDisplayedHostTempoAvailable = audioEngine.isExternalHostTempoAvailable();
            refreshTempoUiFromEngine();
            markSessionDirty();
            break;

        case menuSetFakeHostTempo:
            audioEngine.setExternalHostTempoBpm(128.0);
            refreshTempoUiFromEngine();
            break;

        case menuClearFakeHostTempo:
            audioEngine.clearExternalHostTempo();
            refreshTempoUiFromEngine();
            break;

        case 501:
            settings.setAutoSaveAfterPluginRepair(!settings.getAutoSaveAfterPluginRepair());
            break;
        case 502: addPluginScanFolder(); break;
        case 503: showPluginScanFolders(); break;
        case 504: clearPluginScanFolders(); break;

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
    midiRoutingStates.clear();
    refreshRoutingView();
    syncRoutingToAudioEngine();
    markSessionDirty();
}

// ===================================
// SAVE / LOAD
// ===================================
void MainComponent::newPreset()
{
    if (!maybeSaveChanges())
        return;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();
    midiRoutingStates.clear();

    addEmptyTab();
    addEmptyTab();

    refreshRoutingView();
    syncRoutingToAudioEngine();
    currentPresetFile = {};
    settings.setLastPresetPath({});
    unresolvedMissingPlugins.clear();

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);

    refreshPresetDropdown();
    markSessionClean();
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
    {
        settings.setLastPresetPath(currentPresetFile.getFullPathName());
        settings.addRecentPresetPath(currentPresetFile.getFullPathName());
        markSessionClean();
        refreshPresetDropdown();
    }
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
        settings.setLastPresetPath(currentPresetFile.getFullPathName());
        settings.addRecentPresetPath(currentPresetFile.getFullPathName());
        markSessionClean();
        refreshPresetDropdown();
    }
}

bool MainComponent::writePresetToFile(const juce::File& file)
{
    juce::XmlElement presetXml("PolyHostPreset");
    presetXml.setAttribute("name", file.getFileNameWithoutExtension());
    presetXml.setAttribute("selectedTab", tabs.getCurrentTabIndex());
    presetXml.setAttribute("hostTempoBpm", transportState.hostTempoBpm);
    presetXml.setAttribute("hostSyncEnabled", transportState.hostSyncEnabled);

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
            tabXml->setAttribute("bypassed", tc->isBypassed());

            if (auto* midiState = getMidiRoutingStateForTab(i))
            {
                if (!midiState->assignedDeviceIdentifiers.isEmpty())
                {
                    auto* midiAssignmentsXml = tabXml->createNewChildElement("MidiAssignments");

                    for (auto& identifier : midiState->assignedDeviceIdentifiers)
                    {
                        auto* deviceXml = midiAssignmentsXml->createNewChildElement("Device");
                        deviceXml->setAttribute("identifier", identifier);
                    }
                }
            }

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
    transportState.hostSyncEnabled = presetXml.getBoolAttribute("hostSyncEnabled", false);
    audioEngine.setHostSyncEnabled(transportState.hostSyncEnabled);
    transportState.defaultTempoBpm = presetXml.getDoubleAttribute("hostTempoBpm", 120.0);
    setHostTempoBpm(transportState.defaultTempoBpm);
    lastDisplayedSyncedTempoBpm = -1.0;
    lastDisplayedHostTempoAvailable = audioEngine.isExternalHostTempoAvailable();
    refreshTempoUiFromEngine();

    for (auto* tabXml : presetXml.getChildIterator())
    {
        if (!tabXml->hasTagName("Tab"))
            continue;

        auto* tc = new PluginTabComponent(audioEngine, tabs.getNumTabs());
        configurePluginTabComponent(*tc);
        tc->addChangeListener(this);
        tabs.addTab("Empty",
                    PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty),
                    tc,
                    true);
    }

    if (tabs.getNumTabs() == 0)
        addEmptyTab();
}

void MainComponent::deletePreset()
{
    if (currentPresetFile == juce::File() || !currentPresetFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Delete Preset",
            "There is no current preset file to delete.");
        return;
    }

    auto fileToDelete = currentPresetFile;

    if (!juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                            "Delete Preset",
                                            "Delete current preset:\n" + fileToDelete.getFileName() + "?",
                                            "Delete",
                                            "Cancel"))
    {
        return;
    }

    if (!fileToDelete.deleteFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Delete Preset",
            "Failed to delete preset:\n" + fileToDelete.getFullPathName());
        return;
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();
    midiRoutingStates.clear();

    addEmptyTab();
    addEmptyTab();

    refreshRoutingView();
    syncRoutingToAudioEngine();

    currentPresetFile = {};
    settings.setLastPresetPath({});
    unresolvedMissingPlugins.clear();

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);

    refreshPresetDropdown();
    markSessionClean();
}

bool MainComponent::loadPresetFromFile(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr || !xml->hasTagName("PolyHostPreset"))
        return false;

    isLoadingPreset = true;
    unresolvedMissingPlugins.clear();

    juce::StringArray loadErrors;
    juce::Array<MissingPluginEntry> missingPlugins;

    rebuildTabsFromPresetXml(*xml);
    ensureMidiRoutingStateForCurrentTabs();

    int tabIndex = 0;
    for (auto* tabXml : xml->getChildIterator())
    {
        if (!tabXml->hasTagName("Tab"))
            continue;

        if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
            break;

        if (auto* tc = getTabComponent(tabIndex))
        {
            auto pluginName              = tabXml->getStringAttribute("pluginName", "Unknown Plugin");
            auto pluginPath              = tabXml->getStringAttribute("pluginPath");
            auto pluginPathRelative      = tabXml->getStringAttribute("pluginPathRelative");
            auto pluginPathDriveFlexible = tabXml->getStringAttribute("pluginPathDriveFlexible");
            auto stateBase64             = tabXml->getStringAttribute("pluginState");
            auto typeString              = tabXml->getStringAttribute("type");
            auto pluginFormatName        = tabXml->getStringAttribute("pluginFormatName");
            auto isInstrument            = tabXml->getBoolAttribute("isInstrument", typeString == "Synth");
            auto pluginManufacturer      = tabXml->getStringAttribute("pluginManufacturer");
            auto pluginVersion           = tabXml->getStringAttribute("pluginVersion");
            auto bypassed                = tabXml->getBoolAttribute("bypassed", false);

            if (auto* midiState = getMidiRoutingStateForTab(tabIndex))
            {
                midiState->assignedDeviceIdentifiers.clear();

                if (auto* midiAssignmentsXml = tabXml->getChildByName("MidiAssignments"))
                {
                    for (auto* deviceXml : midiAssignmentsXml->getChildIterator())
                    {
                        if (deviceXml->hasTagName("Device"))
                        {
                            auto identifier = deviceXml->getStringAttribute("identifier").trim();

                            if (identifier.isNotEmpty())
                                midiState->assignedDeviceIdentifiers.addIfNotAlreadyThere(identifier);
                        }
                    }
                }
            }

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
                    loadErrors.add("Failed to load plugin: " + pluginName
                                   + "\n  Path: " + pluginFile.getFullPathName());
                }
                else
                {
                    juce::MemoryBlock state;

                    if (state.fromBase64Encoding(stateBase64))
                        tc->restorePluginState(state);

                    tc->setBypassed(bypassed);
                    refreshTabAppearance(tabIndex);
                }
            }
        }

        ++tabIndex;
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    refreshRoutingView();
    syncRoutingToAudioEngine();

    auto selectedTab = xml->getIntAttribute("selectedTab", 0);

    if (juce::isPositiveAndBelow(selectedTab, tabs.getNumTabs()))
        tabs.setCurrentTabIndex(selectedTab);

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
    {
        bool hasLoadedPlugin = false;

        for (int i = 0; i < tabs.getNumTabs(); ++i)
        {
            if (auto* tc = getTabComponent(i))
            {
                if (tc->hasPlugin())
                {
                    hasLoadedPlugin = true;
                    break;
                }
            }
        }

        int w = AppSettings::defaultWindowWidth;
        int h = AppSettings::defaultWindowHeight;

        if (hasLoadedPlugin)
        {
            w = xml->getIntAttribute("windowWidth", w);
            h = xml->getIntAttribute("windowHeight", h);
        }

        top->setSize(juce::jmax(AppSettings::defaultWindowWidth, w),
                     juce::jmax(AppSettings::defaultWindowHeight, h));
    }

    unresolvedMissingPlugins = missingPlugins;
    currentPresetFile = file;
    settings.setLastPresetPath(currentPresetFile.getFullPathName());
    settings.addRecentPresetPath(currentPresetFile.getFullPathName());
    refreshPresetDropdown();
    showPresetLoadErrors(loadErrors);

    promptToLocateMissingPlugins(unresolvedMissingPlugins);

    juce::MessageManager::callAsync([this]
    {
        isLoadingPreset = false;
        markSessionClean();
    });

    return true;
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

    loadPresetFromFile(chooser.getResult());
}

void MainComponent::refreshPresetLists()
{
    settings.removeMissingRecentPresetPaths();
    refreshPresetDropdown();
}

void MainComponent::refreshPresetDropdown()
{
    presetDropdown.onChange = nullptr;
    presetDropdown.clear(juce::dontSendNotification);

    auto presetFiles = AppSettings::getPresetsDirectory().findChildFiles(juce::File::findFiles,
                                                                         false,
                                                                         "*.xml");

    presetFiles.sort();

    int itemId = 1;
    int selectedId = 0;

    for (auto& file : presetFiles)
    {
        auto presetName = file.getFileNameWithoutExtension();
        presetDropdown.addItem(presetName, itemId);

        if (currentPresetFile == file)
            selectedId = itemId;

        ++itemId;
    }

    if (selectedId != 0)
        presetDropdown.setSelectedId(selectedId, juce::dontSendNotification);
    else if (currentPresetFile.existsAsFile())
        presetDropdown.setText(currentPresetFile.getFileNameWithoutExtension(), juce::dontSendNotification);
    else
        presetDropdown.setSelectedItemIndex(-1, juce::dontSendNotification);

    presetDropdown.onChange = [this]
    {
        loadPresetFromDropdown();
    };
}

void MainComponent::loadPresetFromDropdown()
{
    auto selectedText = presetDropdown.getText().trim();

    if (selectedText.isEmpty())
        return;

    auto file = AppSettings::getPresetsDirectory().getChildFile(selectedText + ".xml");

    if (!file.existsAsFile())
        return;

    if (!maybeSaveChanges())
    {
        refreshPresetDropdown();
        return;
    }

    loadPresetFromFile(file);
    refreshPresetDropdown();
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

    auto replacementFile = findReplacementPluginFile(entry, startDir);

    if (replacementFile == juce::File())
        return false;

    if (!confirmReplacementPlugin(entry, replacementFile))
        return false;

    if (!validateReplacementPluginCompatibility(entry, replacementFile))
        return false;

    return applyReplacementPlugin(entry, replacementFile);
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

    promptToLocateMissingPlugins(unresolvedMissingPlugins);
}

bool MainComponent::promptToLocateMissingPlugins(const juce::Array<MissingPluginEntry>& missingPlugins)
{
    if (missingPlugins.isEmpty())
        return true;

    juce::StringArray restored;
    juce::StringArray failed;
    juce::StringArray skipped;

    juce::Array<int> resolvedTabIndices;
    juce::Array<int> skippedTabIndices;

    for (int i = 0; i < missingPlugins.size(); ++i)
    {
        auto entry = missingPlugins.getReference(i);

        auto action = promptForMissingPluginRepairAction(entry, i, missingPlugins.size());

        if (action == 0)
        {
            skipped.add(entry.pluginName + " (repair session cancelled)");
            skippedTabIndices.addIfNotAlreadyThere(entry.tabIndex);
            break;
        }

        if (action == 2)
        {
            skipped.add(entry.pluginName);
            skippedTabIndices.addIfNotAlreadyThere(entry.tabIndex);
            continue;
        }

        if (locateMissingPlugin(entry))
        {
            restored.add(entry.pluginName);
            resolvedTabIndices.addIfNotAlreadyThere(entry.tabIndex);
        }
        else
        {
            failed.add(entry.pluginName);
        }
    }

    unresolvedMissingPlugins.clear();

    for (int i = 0; i < missingPlugins.size(); ++i)
    {
        const auto& original = missingPlugins.getReference(i);

        if (resolvedTabIndices.contains(original.tabIndex))
            continue;

        if (skippedTabIndices.contains(original.tabIndex))
            continue;

        unresolvedMissingPlugins.add(original);
    }

    showMissingPluginRepairResult(restored, failed, skipped);

    return unresolvedMissingPlugins.isEmpty();
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
    auto message = buildMissingPluginRepairSummary(restored, failed, skipped);

    if (!restored.isEmpty())
    {
        if (settings.getAutoSaveAfterPluginRepair())
        {
            if (currentPresetFile.existsAsFile())
                savePreset();
            else
                savePresetAs();

            juce::MessageManager::callAsync([this]
            {
                markSessionClean();
            });

            message += "\n\nPreset was saved automatically with repaired plugin paths.";

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Missing Plugin Repair Complete",
                message);

            return;
        }

        message += "\n\nSave the preset now to store repaired plugin paths.";

        auto saveNow = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::InfoIcon,
            "Missing Plugin Repair Complete",
            message,
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
        message);
}

juce::File MainComponent::tryAutoLocateReplacementByMetadata(const MissingPluginEntry& entry) const
{
    if (!lastPluginRepairDirectory.isDirectory())
        return {};

    auto candidates = collectPluginFilesInFolder(lastPluginRepairDirectory, false);

    juce::Array<juce::File> strongMatches;

    for (auto& file : candidates)
    {
        juce::PluginDescription desc;
        if (!scanPluginFile(file, desc))
            continue;

        bool nameMatches = entry.pluginName.isNotEmpty()
                           && desc.name.equalsIgnoreCase(entry.pluginName);

        bool formatMatches = entry.pluginFormatName.isEmpty()
                             || desc.pluginFormatName.equalsIgnoreCase(entry.pluginFormatName);

        bool manufacturerMatches = entry.pluginManufacturer.isEmpty()
                                   || desc.manufacturerName.equalsIgnoreCase(entry.pluginManufacturer);

        bool typeMatches = (desc.isInstrument == entry.isInstrument);

        if (nameMatches && formatMatches && manufacturerMatches && typeMatches)
            strongMatches.add(file);
    }

    if (strongMatches.size() == 1)
        return strongMatches[0];

    return {};
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

bool MainComponent::requestQuit()
{
    return maybeSaveChanges();
}

// ===================================
// PLUGINS
// ===================================
void MainComponent::addPluginScanFolder()
{
    juce::FileChooser chooser("Select Plugin Scan Folder",
                              AppSettings::getAppDirectory(),
                              "*");

    if (!chooser.browseForDirectory())
        return;

    auto folder = chooser.getResult();

    if (!folder.isDirectory())
        return;

    settings.addPluginScanFolder(folder.getFullPathName());

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folder Added",
        "Added:\n" + folder.getFullPathName());
}

void MainComponent::showPluginScanFolders()
{
    auto folders = settings.getPluginScanFolders();

    if (folders.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Plugin Scan Folders",
            "No plugin scan folders have been configured.");
        return;
    }

    juce::String message = "Configured plugin scan folders:\n\n";

    for (int i = 0; i < folders.size(); ++i)
        message += folders[i] + "\n";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folders",
        message.trimEnd());
}

void MainComponent::clearPluginScanFolders()
{
    auto folders = settings.getPluginScanFolders();

    if (folders.isEmpty())
        return;

    auto confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Clear Plugin Scan Folders",
        "Remove all configured plugin scan folders?",
        "Clear All",
        "Cancel");

    if (!confirmed)
        return;

    settings.setPluginScanFolders({});

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folders",
        "All plugin scan folders have been cleared.");
}

int MainComponent::getPluginMatchScore(const MissingPluginEntry& entry,
                                       const juce::PluginDescription& desc) const
{
    int score = 0;

    if (entry.pluginName.isNotEmpty() && desc.name.equalsIgnoreCase(entry.pluginName))
        score += 100;

    if (entry.pluginFormatName.isNotEmpty() && desc.pluginFormatName.equalsIgnoreCase(entry.pluginFormatName))
        score += 40;

    if (entry.pluginManufacturer.isNotEmpty() && desc.manufacturerName.equalsIgnoreCase(entry.pluginManufacturer))
        score += 25;

    if (desc.isInstrument == entry.isInstrument)
        score += 20;

    if (entry.pluginVersion.isNotEmpty() && desc.version.equalsIgnoreCase(entry.pluginVersion))
        score += 10;

    return score;
}

// ===================================
// REBUILD MISSING PLUGINS
// ===================================
juce::Array<MainComponent::PluginReplacementCandidate>
MainComponent::findReplacementCandidates(const MissingPluginEntry& entry) const
{
    juce::Array<PluginReplacementCandidate> rankedCandidates;
    juce::StringArray scanFolders = settings.getPluginScanFolders();

    constexpr int maxFilesToInspect = 200;
    int filesInspected = 0;

    for (auto& folderPath : scanFolders)
    {
        juce::File folder(folderPath);

        if (!folder.isDirectory())
            continue;

        auto files = collectPluginFilesInFolder(folder, true);

        for (auto& file : files)
        {
            if (filesInspected >= maxFilesToInspect)
                break;

            ++filesInspected;

            juce::PluginDescription desc;
            if (!scanPluginFile(file, desc))
                continue;

            auto score = getPluginMatchScore(entry, desc);

            if (score <= 0)
                continue;

            PluginReplacementCandidate candidate;
            candidate.file = file;
            candidate.desc = desc;
            candidate.score = score;
            rankedCandidates.add(candidate);
        }

        if (filesInspected >= maxFilesToInspect)
            break;
    }

    std::sort(rankedCandidates.begin(), rankedCandidates.end(),
              [](const PluginReplacementCandidate& a, const PluginReplacementCandidate& b)
              {
                  return a.score > b.score;
              });

    return rankedCandidates;
}

juce::File MainComponent::chooseReplacementCandidate(
    const MissingPluginEntry& entry,
    const juce::Array<PluginReplacementCandidate>& candidates)
{
    if (candidates.isEmpty())
        return {};

    if (candidates.size() == 1)
        return candidates[0].file;

    juce::PopupMenu menu;
    menu.addSectionHeader("Choose replacement for " + entry.pluginName);

    for (int i = 0; i < candidates.size(); ++i)
    {
        const auto& candidate = candidates.getReference(i);

        juce::String label = candidate.desc.name;

        if (candidate.desc.manufacturerName.isNotEmpty())
            label += " - " + candidate.desc.manufacturerName;

        if (candidate.desc.pluginFormatName.isNotEmpty())
            label += " [" + candidate.desc.pluginFormatName + "]";

        label += " {" + candidate.file.getFileName() + "}";

        menu.addItem(i + 1, label);
    }

    auto result = menu.showMenu(juce::PopupMenu::Options().withTargetComponent(this));

    if (result <= 0 || result > candidates.size())
        return {};

    return candidates[result - 1].file;
}

int MainComponent::promptForMissingPluginRepairAction(const MissingPluginEntry& entry,
                                                      int currentIndex,
                                                      int totalCount) const
{
    juce::String message;
    message << "Missing plugin " << (currentIndex + 1) << " of " << totalCount << ":\n\n";
    message << "Name: " << entry.pluginName << "\n";

    if (entry.pluginManufacturer.isNotEmpty())
        message << "Manufacturer: " << entry.pluginManufacturer << "\n";

    if (entry.pluginFormatName.isNotEmpty())
        message << "Format: " << entry.pluginFormatName << "\n";

    message << "\nWould you like to repair this plugin now?";

    return juce::AlertWindow::showYesNoCancelBox(
        juce::AlertWindow::WarningIcon,
        "Repair Missing Plugin",
        message,
        "Repair",
        "Skip",
        "Cancel");
}

juce::Array<juce::File> MainComponent::collectPluginFilesInFolder(const juce::File& folder,
                                                                  bool recursive) const
{
    juce::Array<juce::File> files;

    if (!folder.isDirectory())
        return files;

    auto addMatches = [&](const juce::String& pattern)
    {
        auto matches = folder.findChildFiles(juce::File::findFiles, recursive, pattern);
        for (auto& f : matches)
            files.addIfNotAlreadyThere(f);
    };

    addMatches("*.vst3");
    addMatches("*.dll");
    addMatches("*.clap");

    return files;
}

juce::String MainComponent::buildMissingPluginRepairSummary(const juce::StringArray& restored,
                                                            const juce::StringArray& failed,
                                                            const juce::StringArray& skipped) const
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

    return message.trimEnd();
}

juce::File MainComponent::browseForReplacementPlugin(const MissingPluginEntry& entry,
                                                     const juce::File& startDir) const
{
    juce::FileChooser chooser("Locate Plugin: " + entry.pluginName,
                              startDir,
                              "*.vst3;*.dll;*.clap");

    if (!chooser.browseForFileToOpen())
        return {};

    return chooser.getResult();
}

bool MainComponent::applyReplacementPlugin(const MissingPluginEntry& entry,
                                           const juce::File& replacementFile)
{
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
        syncRoutingToAudioEngine();
        refreshRoutingView();
        markSessionDirty();
        return true;
    }

    return false;
}

juce::File MainComponent::findReplacementPluginFile(const MissingPluginEntry& entry,
                                                    const juce::File& startDir)
{
    juce::File replacementFile = tryAutoLocateReplacement(entry);

    if (replacementFile == juce::File())
        replacementFile = tryAutoLocateReplacementByMetadata(entry);

    if (replacementFile == juce::File())
    {
        auto candidates = findReplacementCandidates(entry);
        replacementFile = chooseReplacementCandidate(entry, candidates);
    }

    if (replacementFile == juce::File())
        replacementFile = browseForReplacementPlugin(entry, startDir);

    return replacementFile;
}

// ===================================
// TABS
// ===================================
void MainComponent::closeCurrentTab()
{
    auto currentIndex = tabs.getCurrentTabIndex();

    if (!juce::isPositiveAndBelow(currentIndex, tabs.getNumTabs()))
        return;

    if (tabs.getNumTabs() == 1)
    {
        if (auto* tc = getTabComponent(currentIndex))
        {
            tc->clearPlugin();
            refreshTabAppearance(currentIndex);
            unresolvedMissingPlugins.clear();
            markSessionDirty();
        }
        return;
    }

    if (auto* tc = getTabComponent(currentIndex))
        tc->removeChangeListener(this);

    const int newIndex = juce::jlimit(0, tabs.getNumTabs() - 2, currentIndex);

    tabs.removeTab(currentIndex);
    tabs.setCurrentTabIndex(newIndex);

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    unresolvedMissingPlugins.clear();
    refreshRoutingView();
    markSessionDirty();
}

bool MainComponent::confirmCloseTab(int tabIndex) const
{
    if (auto* tc = getTabComponent(tabIndex))
    {
        if (!tc->hasPlugin())
            return true;

        auto name = tc->getPluginName();
        if (name.isEmpty())
            name = "this tab";

        return juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Close Tab",
            "Close tab for \"" + name + "\"?",
            "Close Tab",
            "Cancel");
    }

    return false;
}

void MainComponent::showTabContextMenu(int tabIndex)
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Tab");
    menu.addItem(2, "Clear Tab",
                 juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()));
    menu.addItem(3, "Close Tab",
                 juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()));

    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [this, tabIndex](int result)
                       {
                           if (result == 1)
                           {
                               addEmptyTab();
                               return;
                           }

                           if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
                               return;

                           if (result == 2)
                           {
                               if (!confirmClearTab(tabIndex))
                                   return;

                               if (auto* tc = getTabComponent(tabIndex))
                               {
                                   tc->clearPlugin();
                                   refreshTabAppearance(tabIndex);
                                   refreshRoutingView();
                                   unresolvedMissingPlugins.clear();
                                   markSessionDirty();
                               }
                               return;
                           }

                           if (result != 3)
                               return;

                           if (!confirmCloseTab(tabIndex))
                               return;

                           if (tabIndex == tabs.getCurrentTabIndex())
                           {
                               closeCurrentTab();
                               return;
                           }

                           if (tabs.getNumTabs() == 1)
                           {
                               if (auto* tc = getTabComponent(tabIndex))
                               {
                                   tc->clearPlugin();
                                   refreshTabAppearance(tabIndex);
                                   refreshRoutingView();
                                   unresolvedMissingPlugins.clear();
                                   markSessionDirty();
                               }
                               return;
                           }

                           if (auto* tc = getTabComponent(tabIndex))
                               tc->removeChangeListener(this);

                           auto currentIndex = tabs.getCurrentTabIndex();

                           tabs.removeTab(tabIndex);

                           if (tabIndex < currentIndex)
                               --currentIndex;

                           currentIndex = juce::jlimit(0, tabs.getNumTabs() - 1, currentIndex);
                           tabs.setCurrentTabIndex(currentIndex);

                           for (int i = 0; i < tabs.getNumTabs(); ++i)
                               refreshTabAppearance(i);

                           refreshRoutingView();
                           unresolvedMissingPlugins.clear();
                           markSessionDirty();
                       });
}

bool MainComponent::confirmClearTab(int tabIndex) const
{
    if (auto* tc = getTabComponent(tabIndex))
    {
        if (!tc->hasPlugin())
            return true;

        auto name = tc->getPluginName();
        if (name.isEmpty())
            name = "this tab";

        return juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Clear Tab",
            "Remove the plugin from \"" + name + "\" and keep the tab open?",
            "Clear Tab",
            "Cancel");
    }

    return false;
}

void MainComponent::moveTabEarlier(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    int targetIndex = -1;
    for (int i = tabIndex - 1; i >= 0; --i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (tc->hasPlugin())
            {
                targetIndex = i;
                break;
            }
        }
    }

    if (targetIndex < 0)
        return;

    juce::Array<TabSnapshot> snapshots;
    snapshots.ensureStorageAllocated(tabs.getNumTabs());

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        TabSnapshot snap;

        if (auto* tc = getTabComponent(i))
        {
            snap.hasPlugin = tc->hasPlugin();
            snap.bypassed = tc->isBypassed();
            snap.type = tc->getType();
            snap.tabName = tabs.getTabNames()[i];

            if (auto* midiState = getMidiRoutingStateForTab(i))
                snap.midiAssignedDeviceIdentifiers = midiState->assignedDeviceIdentifiers;

            if (snap.hasPlugin)
            {
                snap.pluginFile = tc->getPluginFile();
                snap.pluginState = tc->getPluginState();
            }
        }

        snapshots.add(snap);
    }

    snapshots.swap(tabIndex, targetIndex);

    auto selectedIndex = tabs.getCurrentTabIndex();

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();

    for (int i = 0; i < snapshots.size(); ++i)
    {
        auto* tc = new PluginTabComponent(audioEngine, i);
        configurePluginTabComponent(*tc);
        tc->addChangeListener(this);
        tc->setAllowEditorWindowResize(false);

        tabs.addTab("Empty",
                    PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty),
                    tc,
                    true);

        const auto& snap = snapshots.getReference(i);

        if (snap.hasPlugin)
        {
            if (tc->loadPlugin(snap.pluginFile))
            {
                tc->restorePluginState(snap.pluginState);
                tc->setBypassed(snap.bypassed);
            }
        }

        if (auto* midiState = getMidiRoutingStateForTab(i))
            midiState->assignedDeviceIdentifiers = snap.midiAssignedDeviceIdentifiers;

        refreshTabAppearance(i);
        tc->setAllowEditorWindowResize(true);
    }

    if (selectedIndex == tabIndex)
        selectedIndex = targetIndex;
    else if (selectedIndex == targetIndex)
        selectedIndex = tabIndex;

    selectedIndex = juce::jlimit(0, tabs.getNumTabs() - 1, selectedIndex);
    tabs.setCurrentTabIndex(selectedIndex);

    syncRoutingToAudioEngine();
    refreshRoutingView();
    markSessionDirty();
}

void MainComponent::moveTabLater(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    int targetIndex = -1;
    for (int i = tabIndex + 1; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (tc->hasPlugin())
            {
                targetIndex = i;
                break;
            }
        }
    }

    if (targetIndex < 0)
        return;

    juce::Array<TabSnapshot> snapshots;
    snapshots.ensureStorageAllocated(tabs.getNumTabs());

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        TabSnapshot snap;

        if (auto* tc = getTabComponent(i))
        {
            snap.hasPlugin = tc->hasPlugin();
            snap.bypassed = tc->isBypassed();
            snap.type = tc->getType();
            snap.tabName = tabs.getTabNames()[i];

            if (auto* midiState = getMidiRoutingStateForTab(i))
                snap.midiAssignedDeviceIdentifiers = midiState->assignedDeviceIdentifiers;

            if (snap.hasPlugin)
            {
                snap.pluginFile = tc->getPluginFile();
                snap.pluginState = tc->getPluginState();
            }
        }

        snapshots.add(snap);
    }

    snapshots.swap(tabIndex, targetIndex);

    auto selectedIndex = tabs.getCurrentTabIndex();

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();

    for (int i = 0; i < snapshots.size(); ++i)
    {
        auto* tc = new PluginTabComponent(audioEngine, i);
        configurePluginTabComponent(*tc);
        tc->addChangeListener(this);
        tc->setAllowEditorWindowResize(false);

        tabs.addTab("Empty",
                    PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty),
                    tc,
                    true);

        const auto& snap = snapshots.getReference(i);

        if (snap.hasPlugin)
        {
            if (tc->loadPlugin(snap.pluginFile))
            {
                tc->restorePluginState(snap.pluginState);
                tc->setBypassed(snap.bypassed);
            }
        }

        if (auto* midiState = getMidiRoutingStateForTab(i))
            midiState->assignedDeviceIdentifiers = snap.midiAssignedDeviceIdentifiers;

        refreshTabAppearance(i);
        tc->setAllowEditorWindowResize(true);
    }

    if (selectedIndex == tabIndex)
        selectedIndex = targetIndex;
    else if (selectedIndex == targetIndex)
        selectedIndex = tabIndex;

    selectedIndex = juce::jlimit(0, tabs.getNumTabs() - 1, selectedIndex);
    tabs.setCurrentTabIndex(selectedIndex);

    syncRoutingToAudioEngine();
    refreshRoutingView();
    markSessionDirty();
}

// ===================================
// ROUTING - AUDIO / MIDI
// ===================================
void MainComponent::getAllToolbarItemIds(juce::Array<int>& ids)
{
    ids.add(toolbarRoutingToggle);
}

void MainComponent::getDefaultItemSet(juce::Array<int>& ids)
{
    ids.add(toolbarRoutingToggle);
}

juce::String MainComponent::getToolbarIconGlyph(int itemId)
{
    switch (itemId)
    {
        case toolbarRoutingToggle:
            return juce::String::charToString((juce_wchar) 0xf003);
        default:
            break;
    }

    return "?";
}

juce::ToolbarItemComponent* MainComponent::createItem(int itemId)
{
    if (itemId == toolbarRoutingToggle)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Toggle Routing View",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               32);

        button->onClick = [this, button]
        {
            toggleRoutingView();
            button->setToggleState(showingRoutingView, juce::dontSendNotification);
            button->repaint();
        };

        button->setToggleState(showingRoutingView, juce::dontSendNotification);
        return button;
    }

    return nullptr;
}

void MainComponent::setRoutingViewVisible(bool shouldShow)
{
    showingRoutingView = shouldShow;

    if (showingRoutingView)
        refreshRoutingView();

    tabs.setVisible(!showingRoutingView);
    routingView.setVisible(showingRoutingView);

    if (auto* item = toolbar.getItemComponent(toolbarRoutingToggle))
        if (auto* button = dynamic_cast<juce::Button*>(item))
        {
            button->setToggleState(showingRoutingView, juce::dontSendNotification);
            button->repaint();
        }

    resized();
}

void MainComponent::toggleRoutingView()
{
    setRoutingViewVisible(!showingRoutingView);
}

void MainComponent::refreshRoutingView()
{
    ensureMidiRoutingStateForCurrentTabs();
    juce::Array<RoutingView::ModuleEntry> modules;

    juce::Array<int> loadedTabIndices;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
                continue;

            loadedTabIndices.add(i);
        }
    }

    for (int listIndex = 0; listIndex < loadedTabIndices.size(); ++listIndex)
    {
        const int tabIndex = loadedTabIndices[listIndex];

        if (auto* tc = getTabComponent(tabIndex))
        {
            RoutingView::ModuleEntry entry;
            entry.tabIndex = tabIndex;
            entry.name = tc->getPluginName();
            entry.type = tc->getType();
            entry.isBypassed = tc->isBypassed();
            entry.canMoveUp = (listIndex > 0);
            entry.canMoveDown = (listIndex < loadedTabIndices.size() - 1);

            if (auto* midiState = getMidiRoutingStateForTab(tabIndex))
            {
                int availableAssignedCount = 0;
                auto availableDevices = midiEngine.getAvailableDevices();

                for (auto& identifier : midiState->assignedDeviceIdentifiers)
                {
                    for (auto& device : availableDevices)
                    {
                        if (device.identifier == identifier)
                        {
                            ++availableAssignedCount;
                            break;
                        }
                    }
                }

                entry.midiAssignmentCount = availableAssignedCount;
            }

            modules.add(entry);
        }
    }

    routingView.setModules(modules);
}

void MainComponent::toggleTabBypass(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    if (auto* tc = getTabComponent(tabIndex))
    {
        if (!tc->hasPlugin())
            return;

        tc->setBypassed(!tc->isBypassed());
        syncRoutingToAudioEngine();
        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::syncRoutingToAudioEngine()
{
    juce::Array<juce::AudioProcessorGraph::NodeID> activeSynthIds;
    juce::Array<juce::AudioProcessorGraph::NodeID> activeFxIds;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
                continue;

            if (tc->isBypassed())
                continue;

            if (tc->getType() == PluginTabComponent::SlotType::Synth)
                activeSynthIds.add(tc->getNodeID());
            else if (tc->getType() == PluginTabComponent::SlotType::FX)
                activeFxIds.add(tc->getNodeID());
        }
    }

    audioEngine.setRoutingState(activeSynthIds, activeFxIds);
}

MainComponent::MidiTabRoutingState* MainComponent::getMidiRoutingStateForTab(int tabIndex)
{
    for (auto& state : midiRoutingStates)
        if (state.tabIndex == tabIndex)
            return &state;

    return nullptr;
}

const MainComponent::MidiTabRoutingState* MainComponent::getMidiRoutingStateForTab(int tabIndex) const
{
    for (auto& state : midiRoutingStates)
        if (state.tabIndex == tabIndex)
            return &state;

    return nullptr;
}

void MainComponent::ensureMidiRoutingStateForCurrentTabs()
{
    juce::Array<int> existingTabs;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        existingTabs.add(i);

    for (int i = midiRoutingStates.size(); --i >= 0;)
    {
        if (!existingTabs.contains(midiRoutingStates.getReference(i).tabIndex))
            midiRoutingStates.remove(i);
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (getMidiRoutingStateForTab(i) == nullptr)
        {
            MidiTabRoutingState state;
            state.tabIndex = i;
            state.expanded = true;
            midiRoutingStates.add(state);
        }
    }
}

void MainComponent::toggleMidiAssignment(int tabIndex, const juce::String& deviceIdentifier)
{
    if (auto* state = getMidiRoutingStateForTab(tabIndex))
    {
        if (state->assignedDeviceIdentifiers.contains(deviceIdentifier))
            state->assignedDeviceIdentifiers.removeString(deviceIdentifier);
        else
            state->assignedDeviceIdentifiers.add(deviceIdentifier);

        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::refreshMidiDevices()
{
    refreshRoutingView();
    updateStatusBarText();
}

void MainComponent::showMidiAssignmentsCallout(int tabIndex, juce::Component* anchorComponent)
{
    if (anchorComponent == nullptr)
        return;

    auto* state = getMidiRoutingStateForTab(tabIndex);
    if (state == nullptr)
        return;

    auto availableDevices = midiEngine.getAvailableDevices();

    auto content = std::make_unique<MidiAssignmentsCallout>(
        availableDevices,
        state->assignedDeviceIdentifiers,
        [this, tabIndex](const juce::String& deviceIdentifier)
        {
            toggleMidiAssignment(tabIndex, deviceIdentifier);
        });

    juce::CallOutBox::launchAsynchronously(std::move(content),
                                           anchorComponent->getScreenBounds(),
                                           nullptr);
}

// ===================================
// TEMPO
// ===================================
void MainComponent::setHostTempoBpm(double bpm)
{
    bpm = juce::jlimit(20.0, 300.0, bpm);
    bpm = std::round(bpm * 10.0) / 10.0;

    transportState.hostTempoBpm = bpm;
    audioEngine.setTempoBpm(transportState.hostTempoBpm);
    updateTempoUi();
    markSessionDirty();
}

void MainComponent::updateTempoUi()
{
    const double displayedTempo = transportState.hostSyncEnabled ? audioEngine.getCurrentTempoBpm()
                                                  : transportState.hostTempoBpm;

    tempoEditor.setText(juce::String(displayedTempo, 1), juce::dontSendNotification);
}

void MainComponent::registerTapTempo()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (! tapTimesMs.isEmpty())
    {
        const double gapSinceLastTap = nowMs - tapTimesMs.getLast();

        if (gapSinceLastTap > 2000.0)
            tapTimesMs.clear();
    }

    tapTimesMs.add(nowMs);

    while (tapTimesMs.size() > 6)
        tapTimesMs.remove(0);

    if (tapTimesMs.size() < 2)
        return;

    juce::Array<double> validIntervalsMs;

    for (int i = 1; i < tapTimesMs.size(); ++i)
    {
        const double intervalMs = tapTimesMs[i] - tapTimesMs[i - 1];

        if (intervalMs >= 200.0 && intervalMs <= 3000.0)
            validIntervalsMs.add(intervalMs);
    }

    if (validIntervalsMs.isEmpty())
        return;

    double totalMs = 0.0;

    for (auto interval : validIntervalsMs)
        totalMs += interval;

    const double averageIntervalMs = totalMs / (double) validIntervalsMs.size();
    const double bpm = 60000.0 / averageIntervalMs;

    setHostTempoBpm(bpm);
}

void MainComponent::commitTempoFromEditor()
{
    auto text = tempoEditor.getText().trim();

    if (text.isEmpty())
    {
        updateTempoUi();
        return;
    }

    if (text.containsOnly("0123456789.") == false
        || text.containsChar('.')
           && text.fromFirstOccurrenceOf(".", false, false).containsChar('.'))
    {
        updateTempoUi();
        return;
    }

    auto parsed = text.getDoubleValue();

    if (parsed <= 0.0)
    {
        updateTempoUi();
        return;
    }

    setHostTempoBpm(parsed);
}

void MainComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &tempoEditor)
    {
        commitTempoFromEditor();
        editor.moveKeyboardFocusToSibling(true);
    }
}

void MainComponent::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &tempoEditor)
        updateTempoUi();
}

void MainComponent::textEditorFocusLost(juce::TextEditor& editor)
{
    if (&editor == &tempoEditor)
        commitTempoFromEditor();
}

void MainComponent::updateTempoTooltip()
{
    if (transportState.hostSyncEnabled)
    {
        if (audioEngine.isExternalHostTempoAvailable())
            tempoEditor.setTooltip("Host Sync enabled.\nTempo is controlled by the host.");
        else
            tempoEditor.setTooltip("Host Sync enabled.\nWaiting for external host tempo.");
    }
    else
    {
        tempoEditor.setTooltip("Scroll to adjust. Shift+Scroll: Fine Adjust.\nDouble-click: Reset to "
                               + juce::String(transportState.defaultTempoBpm, 1) + ".");
    }
}

void MainComponent::updateTempoControlState()
{
    const bool editable = !transportState.hostSyncEnabled;

    tempoEditor.setEnabled(editable);
    tapTempoButton.setEnabled(editable);
}

void MainComponent::updateStatusBarText()
{
    juce::String midiText = "No MIDI device selected";
    auto openNames = midiEngine.getOpenDeviceNames();

    if (!openNames.isEmpty())
        midiText = "MIDI: " + openNames.joinIntoString(", ");

    juce::String tempoModeText;

    if (transportState.hostSyncEnabled)
    {
        if (audioEngine.isExternalHostTempoAvailable())
            tempoModeText = "Host Sync ON";
        else
            tempoModeText = "Host Sync ON (No Host Tempo)";
    }
    else
    {
        tempoModeText = "Internal Tempo";
    }

    statusBar.setText("PolyHost 0.1  |  " + midiText + "  |  " + tempoModeText + "  |  Ready",
                      juce::dontSendNotification);
}

bool MainComponent::isTempoEditorInteractive() const
{
    return !transportState.hostSyncEnabled;
}

void MainComponent::refreshTempoUiFromEngine()
{
    updateTempoControlState();
    updateTempoTooltip();
    updateTempoUi();
    updateStatusBarText();
}

void MainComponent::timerCallback()
{
    if (!transportState.hostSyncEnabled)
        return;

    const double currentTempo = audioEngine.getCurrentTempoBpm();
    const bool hostTempoAvailable = audioEngine.isExternalHostTempoAvailable();

    const bool tempoChanged = std::abs(currentTempo - lastDisplayedSyncedTempoBpm) >= 0.05;
    const bool availabilityChanged = hostTempoAvailable != lastDisplayedHostTempoAvailable;

    if (tempoChanged || availabilityChanged)
    {
        lastDisplayedSyncedTempoBpm = currentTempo;
        lastDisplayedHostTempoAvailable = hostTempoAvailable;
        refreshTempoUiFromEngine();
    }
}

