#include "MainComponent.h"
#include <cmath>

namespace
{
    class MidiAssignmentsCallout final : public juce::Component
    {
    public:
        MidiAssignmentsCallout(const juce::Array<juce::MidiDeviceInfo>& availableDevicesIn,
                               const juce::StringArray& assignedDeviceIdentifiers,
                               std::function<void(const juce::String& deviceIdentifier)> onToggle,
                               std::function<void()> onAssignAllEnabled,
                               std::function<void()> onClearAll,
                               std::function<juce::StringArray()> getAssignedIdentifiers)
            : availableDevices(availableDevicesIn),
              toggleAssignment(std::move(onToggle)),
              assignAllEnabled(std::move(onAssignAllEnabled)),
              clearAllAssignments(std::move(onClearAll)),
              getAssignedIdentifiers(std::move(getAssignedIdentifiers))
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

                    refreshToggleStatesFromAssignedIdentifiers();
                };

                addAndMakeVisible(button);
            }

            addAndMakeVisible(assignAllButton);
            assignAllButton.setButtonText("Assign All Enabled");
            assignAllButton.onClick = [this]
            {
                if (assignAllEnabled)
                    assignAllEnabled();

                refreshToggleStatesFromAssignedIdentifiers();
            };

            addAndMakeVisible(clearAllButton);
            clearAllButton.setButtonText("Clear All");
            clearAllButton.onClick = [this]
            {
                if (clearAllAssignments)
                    clearAllAssignments();

                refreshToggleStatesFromAssignedIdentifiers();
            };

            addAndMakeVisible(closeButton);
            closeButton.setButtonText("Close");
            closeButton.onClick = [this]
            {
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->dismiss();
            };

            const int width = 280;
            const int height = 20
                             + (deviceButtons.size() * 28)
                             + 13   // space between device list and buttons
                             + 28
                             + 13   // space between buttons
                             + 28
                             + 2;   // bottom margin

            setSize(width, height);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);

            for (auto* button : deviceButtons)
                button->setBounds(area.removeFromTop(28));

            area.removeFromTop(13); // original 8 + 5 lower

            auto actionRow = area.removeFromTop(28);
            const int gap = 8;
            const int assignWidth = 130;
            const int clearWidth = 80;
            const int totalWidth = assignWidth + gap + clearWidth;
            auto centredActionRow = actionRow.withSizeKeepingCentre(totalWidth, 28);

            assignAllButton.setBounds(centredActionRow.removeFromLeft(assignWidth));
            centredActionRow.removeFromLeft(gap);
            clearAllButton.setBounds(centredActionRow.removeFromLeft(clearWidth));

            area.removeFromTop(13); // more separation before Close
            closeButton.setBounds(area.removeFromTop(28).withSizeKeepingCentre(80, 28));
        }

    private:
        void refreshToggleStatesFromAssignedIdentifiers()
        {
            if (!getAssignedIdentifiers)
                return;

            auto assigned = getAssignedIdentifiers();

            for (int i = 0; i < deviceButtons.size(); ++i)
            {
                if (auto* button = deviceButtons[i])
                    button->setToggleState(assigned.contains(availableDevices[i].identifier),
                                           juce::dontSendNotification);
            }

            repaint();
        }

        juce::Array<juce::MidiDeviceInfo> availableDevices;
        juce::OwnedArray<juce::ToggleButton> deviceButtons;
        juce::TextButton assignAllButton;
        juce::TextButton clearAllButton;
        juce::TextButton closeButton;
        std::function<void(const juce::String& deviceIdentifier)> toggleAssignment;
        std::function<void()> assignAllEnabled;
        std::function<void()> clearAllAssignments;
        std::function<juce::StringArray()> getAssignedIdentifiers;
    };

    class AboutDialogContent final : public juce::Component
    {
    public:
        AboutDialogContent()
        {
            titleLabel.setText("PolyHost", juce::dontSendNotification);
            titleLabel.setJustificationType(juce::Justification::centred);
            titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(titleLabel);

            versionLabel.setText("Version " + juce::String(POLYHOST_VERSION_STRING), juce::dontSendNotification);
            versionLabel.setJustificationType(juce::Justification::centred);
            versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(versionLabel);

            infoLabel.setText("A lightweight tabbed plugin host for VST3 and CLAP.\nBuilt with JUCE.\n\nVST2 and 32-bit bridging planned.",
                              juce::dontSendNotification);
            infoLabel.setJustificationType(juce::Justification::centred);
            infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(infoLabel);

            closeButton.setButtonText("Close");
            closeButton.onClick = [this]
            {
                if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
                    parent->exitModalState(0);
            };
            addAndMakeVisible(closeButton);

            setSize(360, 220);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF1D2230));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(20);

            titleLabel.setBounds(area.removeFromTop(34));
            versionLabel.setBounds(area.removeFromTop(22));
            area.removeFromTop(14);
            infoLabel.setBounds(area.removeFromTop(80));
            area.removeFromTop(12);
            closeButton.setBounds(area.removeFromTop(30).withSizeKeepingCentre(90, 30));
        }

    private:
        juce::Label titleLabel;
        juce::Label versionLabel;
        juce::Label infoLabel;
        juce::TextButton closeButton;
    };
}

    class FixedWidthSpacer final : public juce::ToolbarItemComponent
    {
    public:
        FixedWidthSpacer(int itemId, int widthIn)
            : juce::ToolbarItemComponent(itemId, "Spacer", false),
              width(widthIn)
        {
        }

        bool getToolbarItemSizes(int toolbarDepth, bool isVertical,
                                 int& preferredSize, int& minSize, int& maxSize) override
        {
            juce::ignoreUnused(toolbarDepth, isVertical);
            preferredSize = width;
            minSize = width;
            maxSize = width;
            return true;
        }

        void paintButtonArea(juce::Graphics&, int, int, bool, bool) override {}

        void contentAreaChanged(const juce::Rectangle<int>&) override {}

    private:
        int width = 10;
    };

void MainComponent::AddTabButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool /*isButtonDown*/)
{
    auto area = getLocalBounds().toFloat();

    auto baseColour = PluginTabComponent::colourForType(PluginTabComponent::SlotType::Empty);
    auto colour = baseColour.darker(0.50f);

    if (isMouseOverButton)
        colour = colour.brighter(0.1f);

    const float cornerSize = 6.0f;

    juce::Path tabShape;
    tabShape.startNewSubPath(area.getBottomLeft());
    tabShape.lineTo(area.getX(), area.getY() + cornerSize);
    tabShape.quadraticTo(area.getX(), area.getY(),
                         area.getX() + cornerSize, area.getY());
    tabShape.lineTo(area.getRight() - cornerSize, area.getY());
    tabShape.quadraticTo(area.getRight(), area.getY(),
                         area.getRight(), area.getY() + cornerSize);
    tabShape.lineTo(area.getRight(), area.getBottom());
    tabShape.closeSubPath();

    g.setColour(colour);
    g.fillPath(tabShape);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawFittedText("+",
                     getLocalBounds().reduced(2, 0),
                     juce::Justification::centred,
                     1);
}

MainComponent::MainComponent()
{
    setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);
    tooltipWindow.setOpaque(false);
    audioEngine.initialise(settings.getAudioDeviceState());
    audioEngine.setPlayHead(standaloneTempoSupport.getPlayHead());
    standaloneTempoSupport.setWrappedAudioCallback(&audioEngine.getGraphPlayer());
    standaloneTempoSupport.initialise(audioEngine.getDeviceManager());

    audioEngine.getDeviceManager().addChangeListener(this);
    midiEngine.addChangeListener(this);
    addAndMakeVisible(menuBar);
    toolbar.addDefaultItems(*this);
    addAndMakeVisible(toolbar);
    toolbar.setColour(juce::Toolbar::backgroundColourId, juce::Colour(0xFF1D2230));

    tempoStrip = standaloneTempoSupport.createTempoStripComponent();
    addAndMakeVisible(*tempoStrip);

    addAndMakeVisible(presetDropdown);
    presetDropdown.setTextWhenNothingSelected("Select Preset");
    // presetDropdown.setTextWhenNothingSelected("Untitled");
    presetDropdown.onBeforePopup = [this]
    {
        refreshPresetLists();
    };
    presetDropdown.onChange = [this]
    {
        loadPresetFromDropdown();
    };

    refreshPresetDropdown();

    const auto savedDefaultTempo = settings.getDefaultTempoBpm();
    standaloneTempoSupport.setDefaultTempoBpm(savedDefaultTempo);
    standaloneTempoSupport.setHostTempoBpm(savedDefaultTempo);
    standaloneTempoSupport.refreshTempoStrip();
    standaloneTempoSupport.onTempoChanged = [this]
    {
        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        if (!isLoadingPreset && nowMs >= ignoreDirtyChangesUntilMs)
        {
            if (!shouldSuppressDirtyForSinglePluginQuickOpen())
                markSessionDirty();
        }
    };

    standaloneTempoSupport.onSetCurrentTempoAsDefaultRequested = [this](double bpm)
    {
        settings.setDefaultTempoBpm(bpm);
        standaloneTempoSupport.setDefaultTempoBpm(bpm);
        standaloneTempoSupport.refreshTempoStrip();

        statusBar.setText("PolyHost 0.1  |  Default BPM set to " + juce::String(bpm, 1),
                          juce::dontSendNotification);

        auto safe = juce::Component::SafePointer<MainComponent>(this);

        juce::Timer::callAfterDelay(1500, [safe]
        {
            if (safe != nullptr)
                safe->updateStatusBarText();
        });
    };

    tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xFF16213E));
    tabs.setTabBarDepth(34);
    tabs.getTabbedButtonBar().setMinimumTabScaleFactor(0.7f);
    tabs.getTabbedButtonBar().setLookAndFeel(&tabBarLookAndFeel);
    tabs.getTabbedButtonBar().addMouseListener(&tabBarMouseListener, true);
    tabs.getTabbedButtonBar().addChangeListener(this);
    addAndMakeVisible(tabs);

    addAndMakeVisible(addTabButton);
    addTabButton.setTooltip("New Tab");
    addTabButton.onClick = [this]
    {
        addEmptyTab();
    };

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
            handleCurrentTabChanged();
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

    statusMeters = std::make_unique<StatusMetersComponent>(audioEngine);
    addAndMakeVisible(*statusMeters);

    markSessionClean();
    updateWindowTitle();

    resized();

    if (tempoStrip != nullptr)
        tempoStrip->toFront(false);
}

MainComponent::~MainComponent()
{
    tabs.getTabbedButtonBar().removeMouseListener(&tabBarMouseListener);
    tabs.getTabbedButtonBar().removeChangeListener(this);
    tabs.getTabbedButtonBar().setLookAndFeel(nullptr);
    audioEngine.getDeviceManager().removeChangeListener(this);
    midiEngine.removeChangeListener(this);
    standaloneTempoSupport.shutdown(audioEngine.getDeviceManager());
    settings.setAudioDeviceState(audioEngine.createAudioDeviceStateXml());
    audioEngine.shutdown();
}

SessionData MainComponent::buildSessionData() const
{
    SessionData session;

    session.name = sessionDocument.getDisplayName();
    session.hostTempoBpm = standaloneTempoSupport.getHostTempoBpm();

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            SessionTabData tab;
            tab.index = i;
            tab.type = tc->getType();
            tab.tabName = tabs.getTabNames()[i];
            tab.bypassed = tc->isBypassed();
            tab.hasSavedWindowBounds = tc->hasSavedWindowBounds();

            if (tab.hasSavedWindowBounds)
            {
                auto savedBounds = tc->getSavedWindowBounds();
                tab.savedWindowWidth = savedBounds.getWidth();
                tab.savedWindowHeight = savedBounds.getHeight();
            }

            if (auto* midiState = getMidiRoutingStateForTab(i))
                tab.midiAssignedDeviceIdentifiers = midiState->assignedDeviceIdentifiers;

            if (tc->hasPlugin())
            {
                auto pluginFile = tc->getPluginFile();

                tab.hasPlugin = true;

                auto desc = tc->getLoadedPluginDescription();

                tab.plugin.pluginName = desc.name;
                tab.plugin.pluginDescriptiveName = desc.descriptiveName;
                tab.plugin.pluginPath = pluginFile.getFullPathName();
                tab.plugin.pluginPathRelative = AppSettings::makePathPortable(pluginFile);
                tab.plugin.pluginPathDriveFlexible = AppSettings::getDriveFlexiblePath(pluginFile);
                tab.plugin.pluginFormatName = desc.pluginFormatName;
                tab.plugin.pluginFileOrIdentifier = desc.fileOrIdentifier;
                tab.plugin.pluginUniqueId = desc.uniqueId;
                tab.plugin.isInstrument = desc.isInstrument;
                tab.plugin.pluginManufacturer = desc.manufacturerName;
                tab.plugin.pluginVersion = desc.version;
                tab.plugin.pluginStateBase64 = tc->getPluginState().toBase64Encoding();
            }

            session.tabs.add(tab);
        }
    }

    return session;
}

void MainComponent::applySessionData(const SessionData& session,
                                     juce::StringArray& loadErrors,
                                     juce::Array<MissingPluginEntry>& missingPlugins)
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i))
            tc->removeChangeListener(this);

    tabs.clearTabs();
    midiRoutingStates.clear();

    standaloneTempoSupport.setDefaultTempoBpm(session.hostTempoBpm);
    standaloneTempoSupport.setHostTempoBpm(session.hostTempoBpm);
    standaloneTempoSupport.refreshTempoStrip();

    for (int i = 0; i < session.tabs.size(); ++i)
    {
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

    ensureMidiRoutingStateForCurrentTabs();

    for (int i = 0; i < session.tabs.size(); ++i)
    {
        if (!juce::isPositiveAndBelow(i, tabs.getNumTabs()))
            break;

        const auto& tabData = session.tabs.getReference(i);

        if (auto* midiState = getMidiRoutingStateForTab(i))
            midiState->assignedDeviceIdentifiers = tabData.midiAssignedDeviceIdentifiers;

        if (auto* tc = getTabComponent(i))
        {
            if (tabData.hasSavedWindowBounds
                && tabData.savedWindowWidth > 0
                && tabData.savedWindowHeight > 0)
            {
                tc->setSavedWindowBounds(tabData.savedWindowWidth,
                                         tabData.savedWindowHeight);
            }
            else
            {
                tc->clearSavedWindowBounds();
            }

            if (tabData.hasPlugin)
            {
                auto pluginFile = AppSettings::resolvePluginPath(tabData.plugin.pluginPath,
                                                                 tabData.plugin.pluginPathRelative,
                                                                 tabData.plugin.pluginPathDriveFlexible);

                if (pluginFile == juce::File())
                {
                    juce::String displayPath = tabData.plugin.pluginPathRelative.isNotEmpty()
                        ? tabData.plugin.pluginPathRelative
                        : tabData.plugin.pluginPath;

                    if (displayPath.isEmpty())
                        displayPath = tabData.plugin.pluginPathDriveFlexible;

                    loadErrors.add("Missing plugin: " + tabData.plugin.pluginName + "\n  Path: " + displayPath);

                    MissingPluginEntry entry;
                    entry.tabIndex = i;
                    entry.slotType = tabData.type;
                    entry.pluginName = tabData.plugin.pluginName;
                    entry.pluginPath = tabData.plugin.pluginPath;
                    entry.pluginPathRelative = tabData.plugin.pluginPathRelative;
                    entry.pluginPathDriveFlexible = tabData.plugin.pluginPathDriveFlexible;
                    entry.pluginStateBase64 = tabData.plugin.pluginStateBase64;
                    entry.pluginFormatName = tabData.plugin.pluginFormatName;
                    entry.isInstrument = tabData.plugin.isInstrument;
                    entry.pluginManufacturer = tabData.plugin.pluginManufacturer;
                    entry.pluginVersion = tabData.plugin.pluginVersion;
                    missingPlugins.add(entry);
                }
                else if (!tc->loadPluginFromSessionData(pluginFile, tabData.plugin))
                {
                    loadErrors.add("Failed to load plugin: " + tabData.plugin.pluginName
                                   + "\n  Path: " + pluginFile.getFullPathName());
                }
                else
                {
                    juce::MemoryBlock state;

                    if (state.fromBase64Encoding(tabData.plugin.pluginStateBase64))
                        tc->restorePluginState(state);

                    tc->setBypassed(tabData.bypassed);
                }
            }

            refreshTabAppearance(i);
        }
    }

    refreshRoutingView();
    syncRoutingToAudioEngine();

    if (tabs.getNumTabs() > 0)
        tabs.setCurrentTabIndex(0);

    setRoutingViewVisible(false);
    handleCurrentTabChanged();
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

    if (source == &tabs.getTabbedButtonBar())
    {
        handleCurrentTabChanged();
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

                const auto nowMs = juce::Time::getMillisecondCounterHiRes();
                if (!isLoadingPreset && nowMs >= ignoreDirtyChangesUntilMs)
                {
                    if (!shouldSuppressDirtyForSinglePluginQuickOpen())
                        markSessionDirty();
                }

                return;
            }
        }
    }
}

void MainComponent::updateWindowTitle()
{
    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setName(sessionDocument.buildWindowTitle());
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
                    autoAssignGlobalMidiToTabIfAppropriate(i);
                    refreshTabAppearance(i);
                    handleCurrentTabChanged();

                    suppressDirtyForSinglePluginQuickOpen = (getLoadedPluginTabCount() <= 1);

                    if (shouldSuppressDirtyForSinglePluginQuickOpen())
                        markSessionClean();
                    else
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
            autoAssignGlobalMidiToTabIfAppropriate(tabs.getNumTabs() - 1);
            refreshTabAppearance(tabs.getNumTabs() - 1);
            handleCurrentTabChanged();

            suppressDirtyForSinglePluginQuickOpen = (getLoadedPluginTabCount() <= 1);

            if (shouldSuppressDirtyForSinglePluginQuickOpen())
                markSessionClean();
            else
                markSessionDirty();
        }
    }
}

void MainComponent::browseAndLoadPluginInCurrentTab()
{
    juce::FileChooser chooser("Open Plugin", juce::File(), "*.vst3;*.dll;*.clap");

    if (!chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();
    auto currentIndex = tabs.getCurrentTabIndex();

    if (!juce::isPositiveAndBelow(currentIndex, tabs.getNumTabs()))
        currentIndex = 0;

    if (!juce::isPositiveAndBelow(currentIndex, tabs.getNumTabs()))
    {
        addEmptyTab();
        currentIndex = tabs.getNumTabs() - 1;
    }

    if (auto* tc = getTabComponent(currentIndex))
    {
        if (tc->loadPlugin(file))
        {
            tabs.setCurrentTabIndex(currentIndex);
            autoAssignGlobalMidiToTabIfAppropriate(currentIndex);
            refreshTabAppearance(currentIndex);
            handleCurrentTabChanged();
            markSessionDirty();
        }
    }
}

void MainComponent::browseAndLoadPluginInNewTab()
{
    juce::FileChooser chooser("Open Plugin", juce::File(), "*.vst3;*.dll;*.clap");

    if (!chooser.browseForFileToOpen())
        return;

    loadDroppedPluginInNewTab(chooser.getResult());
}

int MainComponent::promptForDroppedPluginAction(const juce::File& droppedFile, int targetTabIndex) const
{
    if (!juce::isPositiveAndBelow(targetTabIndex, tabs.getNumTabs()))
        return 0;

    auto* targetTab = getTabComponent(targetTabIndex);
    if (targetTab == nullptr || !targetTab->hasPlugin())
        return 1; // treat as open/load directly

    auto currentPluginName = targetTab->getPluginName().trim();
    if (currentPluginName.isEmpty())
        currentPluginName = "Current Plugin";

    juce::String message;
    message << "Replace this tab's plugin:\n"
            << currentPluginName
            << "\n\nWith dropped plugin:\n"
            << droppedFile.getFileName()
            << "\n\nWhat would you like to do?";

    juce::AlertWindow w("Plugin Drop",
                        message,
                        juce::AlertWindow::QuestionIcon);

    w.addButton("Open New Tab", 1);
    w.addButton("Replace Plugin", 2);
    w.addButton("Cancel", 0);

    return w.runModalLoop();
}

bool MainComponent::loadDroppedPluginInNewTab(const juce::File& file)
{
    addEmptyTab();

    const int newTabIndex = tabs.getNumTabs() - 1;

    if (auto* newTab = getTabComponent(newTabIndex))
    {
        if (newTab->loadPlugin(file))
        {
            tabs.setCurrentTabIndex(newTabIndex);
            refreshTabAppearance(newTabIndex);
            handleCurrentTabChanged();
            markSessionDirty();
            return true;
        }
    }

    return false;
}

bool MainComponent::handleDroppedPluginFile(const juce::File& file, int targetTabIndex)
{
    if (!PluginTabComponent::isPluginFile(file))
        return false;

    if (!juce::isPositiveAndBelow(targetTabIndex, tabs.getNumTabs()))
        return false;

    auto* targetTab = getTabComponent(targetTabIndex);
    if (targetTab == nullptr)
        return false;

    if (!targetTab->hasPlugin())
    {
        if (targetTab->loadPlugin(file))
        {
            tabs.setCurrentTabIndex(targetTabIndex);
            autoAssignGlobalMidiToTabIfAppropriate(targetTabIndex);
            refreshTabAppearance(targetTabIndex);
            handleCurrentTabChanged();
            markSessionDirty();
            return true;
        }

        return false;
    }

    const int action = promptForDroppedPluginAction(file, targetTabIndex);

    if (action == 1)
        return loadDroppedPluginInNewTab(file);

    if (action == 2)
    {
        if (targetTab->loadPlugin(file))
        {
            tabs.setCurrentTabIndex(targetTabIndex);
            autoAssignGlobalMidiToTabIfAppropriate(targetTabIndex);
            refreshTabAppearance(targetTabIndex);
            handleCurrentTabChanged();
            markSessionDirty();
            return true;
        }
    }

    return false;
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& path : files)
    {
        if (PluginTabComponent::isPluginFile(juce::File(path)))
            return true;
    }

    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    auto currentIndex = tabs.getCurrentTabIndex();

    for (auto& path : files)
    {
        juce::File file(path);

        if (handleDroppedPluginFile(file, currentIndex))
            return;
    }
}

// ===================================
// TABS
// ===================================
void MainComponent::configurePluginTabComponent(PluginTabComponent& tabComponent)
{
    tabComponent.onOpenDroppedPluginInNewTab = [this, &tabComponent](const juce::File& file)
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i)
        {
            if (getTabComponent(i) == &tabComponent)
            {
                handleDroppedPluginFile(file, i);
                return;
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
    if (!shouldSuppressDirtyForSinglePluginQuickOpen())
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

        tabs.setTabName(index, name);
        tabs.setTabBackgroundColour(index,
                                    isSelected ? baseColour
                                               : baseColour.darker(0.50f));
    }
}

int MainComponent::countTabsOfType(PluginTabComponent::SlotType type) const
{
    int count = 0;
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* tc = getTabComponent(i)) if (tc->getType() == type) ++count;
    return count;
}

int MainComponent::getLoadedPluginTabCount() const
{
    int count = 0;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (tc->hasPlugin())
                ++count;
        }
    }

    return count;
}

bool MainComponent::shouldSuppressDirtyForSinglePluginQuickOpen() const
{
    return suppressDirtyForSinglePluginQuickOpen
           && getLoadedPluginTabCount() <= 1;
}

PluginTabComponent* MainComponent::getTabComponent(int index) const
{
    return dynamic_cast<PluginTabComponent*>(tabs.getTabContentComponent(index));
}

void MainComponent::handleCurrentTabChanged()
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    refreshRoutingView();
    resizeWindowToFitCurrentTab();
}

void MainComponent::resizeWindowToFitCurrentTab()
{
    auto* top = findParentComponentOfClass<juce::DocumentWindow>();
    if (top == nullptr)
        return;

    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr)
        return;

    const int minWidth = AppSettings::defaultWindowWidth;
    const int minHeight = AppSettings::defaultWindowHeight;

    if (tc->hasSavedWindowBounds())
    {
        auto savedBounds = tc->getSavedWindowBounds();

        const int targetWidth = juce::jmax(minWidth, savedBounds.getWidth());
        const int targetHeight = juce::jmax(minHeight, savedBounds.getHeight());

        top->setSize(targetWidth, targetHeight);
        return;
    }

    const auto preferred = tc->getPreferredContentBounds();

    const int extraWidth = 32;
    const int extraHeight =
        juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()
        + 32
        + 34
        + 24
        + 32;

    const int targetWidth = juce::jmax(minWidth, preferred.getWidth() + extraWidth);
    const int targetHeight = juce::jmax(minHeight, preferred.getHeight() + extraHeight);

    top->setSize(targetWidth, targetHeight);
}

void MainComponent::saveCurrentWindowSizeForCurrentTab()
{
    auto* top = findParentComponentOfClass<juce::DocumentWindow>();
    if (top == nullptr)
        return;

    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr)
        return;

    tc->setSavedWindowBounds(top->getWidth(), top->getHeight());
    markSessionDirty();
    resizeWindowToFitCurrentTab();
    toolbar.repaint();
}

void MainComponent::clearSavedWindowSizeForCurrentTab()
{
    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr)
        return;

    if (!tc->hasSavedWindowBounds())
        return;

    tc->clearSavedWindowBounds();
    markSessionDirty();
    resizeWindowToFitCurrentTab();
    toolbar.repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1D2230));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    menuBar.setBounds(area.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));

    auto topRow = area.removeFromTop(32);

    auto presetArea = topRow.removeFromLeft(280);
    presetArea.removeFromLeft(8);
    presetArea.removeFromRight(6);

    auto presetBounds = presetArea.reduced(2);
    presetBounds = presetBounds.withSizeKeepingCentre(presetBounds.getWidth(), topRow.getHeight() - 8);
    presetDropdown.setBounds(presetBounds);

    // Tempo background width
    auto tempoArea = topRow.removeFromRight(260);
    tempoArea.removeFromRight(6);

    toolbar.setBounds(topRow);

    if (tempoStrip != nullptr)
    {
        tempoStrip->setBounds(tempoArea);
        tempoStrip->toFront(false);
    }

    auto statusArea = area.removeFromBottom(24);
    auto metersArea = statusArea.removeFromRight(150);

    statusBar.setBounds(statusArea);
    if (statusMeters != nullptr)
        statusMeters->setBounds(metersArea);

    tabs.setBounds(area);
    routingView.setBounds(area);

    const int plusButtonWidth = 28;
    const int plusButtonHeight = tabs.getTabBarDepth() - 4;
    const int plusButtonMarginRight = 6;
    const int plusButtonMarginTop = 2;

    addTabButton.setBounds(tabs.getRight() - plusButtonWidth - plusButtonMarginRight,
                           tabs.getY() + plusButtonMarginTop,
                           plusButtonWidth,
                           plusButtonHeight);
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

            presetSessionController.removeMissingRecentPresetPaths();
            auto recentMenu = recentPresetMenuHelper.buildRecentPresetMenu(menuRecentPresetBase);

            menu = fileMenuHelper.buildFileMenu(recentMenu,
                                                !unresolvedMissingPlugins.isEmpty(),
                                                menuOpenPresetsFolder);
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
            menu.addItem(303, "Panic");
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
    if (recentPresetMenuHelper.isRecentPresetItemId(itemId, menuRecentPresetBase))
    {
        auto recentFile = recentPresetMenuHelper.getRecentPresetFileForItemId(itemId, menuRecentPresetBase);

        if (recentFile.existsAsFile())
        {
            if (!maybeSaveChanges())
                return;

            loadPresetFromFile(recentFile);
        }

        return;
    }
    switch (itemId)
    {
        case FileMenuHelper::newPreset: newPreset(); break;
        case FileMenuHelper::newTab: addEmptyTab(); break;
        case FileMenuHelper::closeCurrentTab: closeCurrentTab(); break;
        case FileMenuHelper::replacePlugin: browseAndLoadPluginInCurrentTab(); break;
        case FileMenuHelper::newPlugin: browseAndLoadPluginInNewTab(); break;
        case FileMenuHelper::savePreset: savePreset(); break;
        case FileMenuHelper::savePresetAs: savePresetAs(); break;
        case FileMenuHelper::loadPreset: loadPreset(); break;
        case FileMenuHelper::locateMissingPlugins: locateMissingPlugins(); break;
        case FileMenuHelper::deleteCurrentPreset: deletePreset(); break;
        case menuOpenPresetsFolder:
            AppSettings::getPresetsDirectory().startAsProcess();
            break;
        case FileMenuHelper::quit:
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
        case 303:
            sendMidiPanic();
            break;

        case 501:
            settings.setAutoSaveAfterPluginRepair(!settings.getAutoSaveAfterPluginRepair());
            break;
        case 502: addPluginScanFolder(); break;
        case 503: showPluginScanFolders(); break;
        case 504: clearPluginScanFolders(); break;

        case 601:
            showAboutDialog();
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

void MainComponent::showAboutDialog()
{
    auto content = std::make_unique<AboutDialogContent>();

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = "About PolyHost";
    options.dialogBackgroundColour = juce::Colour(0xFF1D2230);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    if (auto* window = options.launchAsync())
        window->centreWithSize(360, 220);
}

// ===================================
// SAVE / LOAD
// ===================================
bool MainComponent::saveCurrentSessionOrSaveAs()
{
    if (presetSessionController.hasSavedFile())
        savePreset();
    else
        savePresetAs();

    return !sessionDocument.isDirty();
}

void MainComponent::handleSuccessfulPresetSave(const juce::File& file)
{
    presetSessionController.handleSuccessfulSave(file);
    updateWindowTitle();
    refreshPresetDropdown();
}

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

    const auto savedDefaultTempo = settings.getDefaultTempoBpm();
    standaloneTempoSupport.setDefaultTempoBpm(savedDefaultTempo);
    standaloneTempoSupport.setHostTempoBpm(savedDefaultTempo);
    standaloneTempoSupport.refreshTempoStrip();

    refreshRoutingView();
    syncRoutingToAudioEngine();
    presetSessionController.clearCurrentSessionReference();
    unresolvedMissingPlugins.clear();
    suppressDirtyForSinglePluginQuickOpen = false;

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);

    refreshPresetDropdown();
    markSessionClean();
    resizeWindowToFitCurrentTab();
}

bool MainComponent::maybeSaveChanges()
{
    if (!sessionDocument.isDirty())
        return true;

    auto decision = unsavedChangesHelper.promptToSaveChanges();

    if (decision == UnsavedChangesHelper::Decision::cancel)
        return false;

    if (decision == UnsavedChangesHelper::Decision::save)
        return saveCurrentSessionOrSaveAs();

    if (decision == UnsavedChangesHelper::Decision::discard)
        return true;

    return false;
}

void MainComponent::savePreset()
{
    if (presetSessionController.getCurrentFile() == juce::File())
    {
        savePresetAs();
        return;
    }

    if (writePresetToFile(presetSessionController.getCurrentFile()))
        handleSuccessfulPresetSave(presetSessionController.getCurrentFile());
}

void MainComponent::savePresetAs()
{
    auto presetName = presetNamePromptHelper.promptForPresetName(
        presetFileDialogHelper.getSuggestedSaveName());

    if (presetName.isEmpty())
        return;

    auto file = presetFileDialogHelper.buildSaveFileForPresetName(presetName);

    if (writePresetToFile(file))
        handleSuccessfulPresetSave(file);
}

bool MainComponent::writePresetToFile(const juce::File& file)
{
    auto session = buildSessionData();
    session.name = file.getFileNameWithoutExtension();
    return SessionManager::saveSessionToFile(session, file);
}

void MainComponent::rebuildTabsFromPresetXml(const juce::XmlElement& presetXml)
{
    juce::ignoreUnused(presetXml);
}

void MainComponent::deletePreset()
{
    if (presetSessionController.getCurrentFile() == juce::File() || !presetSessionController.getCurrentFile().existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Delete Preset",
            "There is no current preset file to delete.");
        return;
    }

    auto fileToDelete = presetSessionController.getCurrentFile();

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

    refreshRoutingView();
    syncRoutingToAudioEngine();

    presetSessionController.clearCurrentSessionReference();
    unresolvedMissingPlugins.clear();

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        top->setSize(AppSettings::defaultWindowWidth, AppSettings::defaultWindowHeight);

    refreshPresetDropdown();
    markSessionClean();
}

bool MainComponent::confirmRevertCurrentPreset() const
{
    if (!sessionDocument.isDirty())
        return true;

    juce::String targetName = "Untitled";

    if (presetSessionController.getCurrentFile().existsAsFile())
        targetName = presetSessionController.getCurrentFile().getFileNameWithoutExtension();

    return juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Revert Preset",
        "Discard all unsaved changes and reload \"" + targetName + "\"?",
        "Revert Changes",
        "Cancel");
}

void MainComponent::revertCurrentPreset()
{
    if (!confirmRevertCurrentPreset())
        return;

    auto currentFile = presetSessionController.getCurrentFile();

    if (currentFile.existsAsFile())
    {
        loadPresetFromFile(currentFile);
        refreshPresetDropdown();
        return;
    }

    newPreset();
    refreshPresetDropdown();
}

void MainComponent::performInitialSessionLoad(bool shouldLoadLastPreset)
{
    if (!shouldLoadLastPreset)
        return;

    auto lastPresetPath = settings.getLastPresetPath().trim();

    if (lastPresetPath.isEmpty())
        return;

    juce::File lastPresetFile(lastPresetPath);

    if (lastPresetFile.existsAsFile())
        loadPresetFromFile(lastPresetFile);
}

bool MainComponent::loadPresetFromFile(const juce::File& file)
{
    SessionData session;
    juce::StringArray parseWarnings;

    if (!SessionManager::loadSessionFromFile(file, session, parseWarnings))
        return false;

    isLoadingPreset = true;
    ignoreDirtyChangesUntilMs = juce::Time::getMillisecondCounterHiRes() + 500.0;
    unresolvedMissingPlugins.clear();
    suppressDirtyForSinglePluginQuickOpen = false;

    juce::StringArray loadErrors = parseWarnings;
    juce::Array<MissingPluginEntry> missingPlugins;

    applySessionData(session, loadErrors, missingPlugins);

    unresolvedMissingPlugins = missingPlugins;
    presetSessionController.rememberLoadedFile(file);
    refreshPresetDropdown();
    showPresetLoadErrors(loadErrors);

    promptToLocateMissingPlugins(unresolvedMissingPlugins);

    juce::Timer::callAfterDelay(500, [this]
    {
        isLoadingPreset = false;
        ignoreDirtyChangesUntilMs = 0.0;
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

    constexpr int newPresetItemId = 1;
    constexpr int firstPresetItemId = 3;

    presetDropdown.addItem("New Preset", newPresetItemId);
    presetDropdown.addSeparator();

    auto presetFiles = AppSettings::getPresetsDirectory().findChildFiles(juce::File::findFiles,
                                                                         false,
                                                                         "*.xml");

    presetFiles.sort();

    int itemId = firstPresetItemId;
    int selectedId = 0;

    for (auto& file : presetFiles)
    {
        auto presetName = file.getFileNameWithoutExtension();
        presetDropdown.addItem(presetName, itemId);

        if (presetSessionController.isCurrentFile(file))
            selectedId = itemId;

        ++itemId;
    }

    if (selectedId != 0 && !sessionDocument.isDirty())
        presetDropdown.setSelectedId(selectedId, juce::dontSendNotification);
    else
        updatePresetDropdownDisplayText();

    presetDropdown.onChange = [this]
    {
        loadPresetFromDropdown();
    };
}

void MainComponent::updatePresetDropdownDisplayText()
{
    juce::String displayName = "Untitled";

    if (presetSessionController.getCurrentFile().existsAsFile())
        displayName = presetSessionController.getCurrentFileDisplayName();

    if (sessionDocument.isDirty())
        displayName += " *";

    presetDropdown.setText(displayName, juce::dontSendNotification);
}

void MainComponent::loadPresetFromDropdown()
{
    constexpr int newPresetItemId = 1;
    constexpr int firstPresetItemId = 3;

    const int selectedId = presetDropdown.getSelectedId();

    if (selectedId == 0)
        return;

    if (selectedId == newPresetItemId)
    {
        if (!maybeSaveChanges())
        {
            refreshPresetDropdown();
            return;
        }

        newPreset();
        refreshPresetDropdown();
        return;
    }

    if (selectedId < firstPresetItemId)
        return;

    auto selectedText = presetDropdown.getText().trim();

    if (selectedText.isEmpty() || selectedText == "New Preset")
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
            if (presetSessionController.getCurrentFile().existsAsFile())
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
            if (presetSessionController.getCurrentFile().existsAsFile())
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
    if (getLoadedPluginTabCount() > 1)
        suppressDirtyForSinglePluginQuickOpen = false;

    sessionDocument.markDirty();
    updateWindowTitle();
    updatePresetDropdownDisplayText();
    toolbar.repaint();
}

void MainComponent::markSessionClean()
{
    sessionDocument.markClean();
    updateWindowTitle();
    updatePresetDropdownDisplayText();
    toolbar.repaint();
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
            tc->clearSavedWindowBounds();
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
    handleCurrentTabChanged();
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
                                   tc->clearSavedWindowBounds();
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
                                   tc->clearSavedWindowBounds();
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

                           handleCurrentTabChanged();
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
            snap.hasSavedWindowBounds = tc->hasSavedWindowBounds();

            if (snap.hasSavedWindowBounds)
            {
                auto savedBounds = tc->getSavedWindowBounds();
                snap.savedWindowWidth = savedBounds.getWidth();
                snap.savedWindowHeight = savedBounds.getHeight();
            }

            if (auto* midiState = getMidiRoutingStateForTab(i))
                snap.midiAssignedDeviceIdentifiers = midiState->assignedDeviceIdentifiers;

            if (snap.hasPlugin)
            {
                snap.pluginFile = tc->getPluginFile();
                snap.pluginState = tc->getPluginState();
                snap.pluginDescription = tc->getLoadedPluginDescription();
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

        if (snap.hasSavedWindowBounds)
            tc->setSavedWindowBounds(snap.savedWindowWidth, snap.savedWindowHeight);

        if (snap.hasPlugin)
        {
            if (tc->loadPlugin(snap.pluginFile, snap.pluginDescription))
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
    handleCurrentTabChanged();
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
            snap.hasSavedWindowBounds = tc->hasSavedWindowBounds();

            if (snap.hasSavedWindowBounds)
            {
                auto savedBounds = tc->getSavedWindowBounds();
                snap.savedWindowWidth = savedBounds.getWidth();
                snap.savedWindowHeight = savedBounds.getHeight();
            }

            if (auto* midiState = getMidiRoutingStateForTab(i))
                snap.midiAssignedDeviceIdentifiers = midiState->assignedDeviceIdentifiers;

            if (snap.hasPlugin)
            {
                snap.pluginFile = tc->getPluginFile();
                snap.pluginState = tc->getPluginState();
                snap.pluginDescription = tc->getLoadedPluginDescription();
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

        if (snap.hasSavedWindowBounds)
            tc->setSavedWindowBounds(snap.savedWindowWidth, snap.savedWindowHeight);

        if (snap.hasPlugin)
        {
            if (tc->loadPlugin(snap.pluginFile, snap.pluginDescription))
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
    handleCurrentTabChanged();
    markSessionDirty();
}

// ===================================
// ROUTING - AUDIO / MIDI
// ===================================
void MainComponent::getAllToolbarItemIds(juce::Array<int>& ids)
{
    ids.add(toolbarRoutingToggle);
    ids.add(toolbarSpacer);
    ids.add(toolbarRefitWindow);
    ids.add(toolbarSpacer);
    ids.add(toolbarSaveTabWindowSize);
    ids.add(toolbarSpacer);
    ids.add(toolbarClearTabWindowSize);
    ids.add(toolbarSpacer);
    ids.add(toolbarSavePreset);
    ids.add(toolbarSpacer);
    ids.add(toolbarSavePresetAs);
    ids.add(toolbarSpacer);
    ids.add(toolbarRevertPreset);
    ids.add(toolbarSpacer);
    ids.add(toolbarMidiPanic);
}

void MainComponent::getDefaultItemSet(juce::Array<int>& ids)
{
    ids.add(toolbarRoutingToggle);
    ids.add(toolbarSpacer);
    ids.add(toolbarRefitWindow);
    ids.add(toolbarSpacer);
    ids.add(toolbarSaveTabWindowSize);
    ids.add(toolbarSpacer);
    ids.add(toolbarClearTabWindowSize);
    ids.add(toolbarSpacer);
    ids.add(toolbarSavePreset);
    ids.add(toolbarSpacer);
    ids.add(toolbarSavePresetAs);
    ids.add(toolbarSpacer);
    ids.add(toolbarRevertPreset);
    ids.add(toolbarSpacer);
    ids.add(toolbarMidiPanic);
}

juce::String MainComponent::getToolbarIconGlyph(int itemId)
{
    switch (itemId)
    {
        case toolbarRoutingToggle:
            return juce::String::charToString((juce_wchar) 0xf003);

        case toolbarRefitWindow:
            return juce::String::charToString((juce_wchar) 0xe9a6);

        case toolbarSavePreset:
            return juce::String::charToString((juce_wchar) 0xe74e);

        case toolbarSavePresetAs:
            return juce::String::charToString((juce_wchar) 0xe792);

        case toolbarRevertPreset:
            return juce::String::charToString((juce_wchar) 0xe72c);

        case toolbarMidiPanic:
            return juce::String::charToString((juce_wchar) 0xe783);

        case toolbarSaveTabWindowSize:
            return juce::String::charToString((juce_wchar) 0xe78c);

        case toolbarClearTabWindowSize:
            return juce::String::charToString((juce_wchar) 0xea39);

        default:
            break;
    }

    return "?";
}

juce::ToolbarItemComponent* MainComponent::createItem(int itemId)
{
    if (itemId == toolbarRoutingToggle)
    {
    // Toolbar button sizing
        auto* button = new StyledToolbarButton(itemId,
                                               "Toggle Routing View",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               [this] { return showingRoutingView; });

        button->onClick = [this, button]
        {
            toggleRoutingView();
            button->setToggleState(showingRoutingView, juce::dontSendNotification);
            button->repaint();
        };

        button->setToggleState(showingRoutingView, juce::dontSendNotification);
        return button;
    }

    if (itemId == toolbarRefitWindow)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Fit window to current plugin",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30);

        button->onClick = [this]
        {
            resizeWindowToFitCurrentTab();
        };

        return button;
    }

    if (itemId == toolbarSaveTabWindowSize)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Save current window size for this tab",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               [this]
                                               {
                                                   if (auto* tc = getTabComponent(tabs.getCurrentTabIndex()))
                                                       return tc->hasSavedWindowBounds();

                                                   return false;
                                               });

        button->onClick = [this, button]
        {
            saveCurrentWindowSizeForCurrentTab();
            button->repaint();
            toolbar.repaint();
        };

        return button;
    }

    if (itemId == toolbarClearTabWindowSize)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Clear saved window size for this tab",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               [this]
                                               {
                                                   if (auto* tc = getTabComponent(tabs.getCurrentTabIndex()))
                                                       return tc->hasSavedWindowBounds();

                                                   return false;
                                               });

        button->onClick = [this, button]
        {
            clearSavedWindowSizeForCurrentTab();
            button->repaint();
            toolbar.repaint();
        };

        return button;
    }

    if (itemId == toolbarSavePreset)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Save Preset",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               [this] { return sessionDocument.isDirty(); });

        button->onClick = [this, button]
        {
            savePreset();
            button->repaint();
            toolbar.repaint();
        };

        return button;
    }

    if (itemId == toolbarSavePresetAs)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Save Preset As",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               [this] { return sessionDocument.isDirty(); });

        button->onClick = [this, button]
        {
            savePresetAs();
            button->repaint();
            toolbar.repaint();
        };

        return button;
    }

    if (itemId == toolbarSpacer)
        return new FixedWidthSpacer(itemId, 2);

    if (itemId == toolbarRevertPreset)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "Revert preset",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30);

        button->onClick = [this]
        {
            revertCurrentPreset();
        };

        return button;
    }

    if (itemId == toolbarMidiPanic)
    {
        auto* button = new StyledToolbarButton(itemId,
                                               "MIDI Panic",
                                               getToolbarIconGlyph(itemId),
                                               StyledToolbarButton::ContentType::IconGlyph,
                                               30,
                                               {},
                                               juce::Colour(0xFF8B2E2E));

        button->onClick = [this]
        {
            sendMidiPanic();
        };

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
    {
        item->setToggleState(showingRoutingView, juce::dontSendNotification);
        item->repaint();
    }

    toolbar.repaint();
    repaint();
    resized();

    if (!showingRoutingView)
        resizeWindowToFitCurrentTab();
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

void MainComponent::assignEnabledGlobalMidiDevicesToTab(int tabIndex)
{
    auto* midiState = getMidiRoutingStateForTab(tabIndex);
    if (midiState == nullptr)
        return;

    if (!midiState->assignedDeviceIdentifiers.isEmpty())
        return;

    auto enabledDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

    for (auto& identifier : enabledDeviceIdentifiers)
        midiState->assignedDeviceIdentifiers.addIfNotAlreadyThere(identifier);
}

void MainComponent::autoAssignGlobalMidiToTabIfAppropriate(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    if (getLoadedPluginTabCount() != 1)
        return;

    auto* tc = getTabComponent(tabIndex);
    if (tc == nullptr || !tc->hasPlugin())
        return;

    assignEnabledGlobalMidiDevicesToTab(tabIndex);
    refreshRoutingView();
    syncRoutingToAudioEngine();
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

void MainComponent::assignAllEnabledMidiDevicesToTab(int tabIndex)
{
    if (auto* state = getMidiRoutingStateForTab(tabIndex))
    {
        state->assignedDeviceIdentifiers.clear();

        auto enabledDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

        for (auto& identifier : enabledDeviceIdentifiers)
            state->assignedDeviceIdentifiers.addIfNotAlreadyThere(identifier);

        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::clearMidiAssignmentsForTab(int tabIndex)
{
    if (auto* state = getMidiRoutingStateForTab(tabIndex))
    {
        state->assignedDeviceIdentifiers.clear();
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
        },
        [this, tabIndex]
        {
            assignAllEnabledMidiDevicesToTab(tabIndex);
        },
        [this, tabIndex]
        {
            clearMidiAssignmentsForTab(tabIndex);
        },
        [this, tabIndex]() -> juce::StringArray
        {
            if (auto* currentState = getMidiRoutingStateForTab(tabIndex))
                return currentState->assignedDeviceIdentifiers;

            return {};
        });

    juce::CallOutBox::launchAsynchronously(std::move(content),
                                           anchorComponent->getScreenBounds(),
                                           nullptr);
}

void MainComponent::sendMidiPanic()
{
    juce::Array<juce::AudioProcessorGraph::NodeID> targetNodeIds;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
                continue;

            if (tc->isBypassed())
                continue;

            targetNodeIds.add(tc->getNodeID());
        }
    }

    if (targetNodeIds.isEmpty())
        return;

    audioEngine.sendMidiPanicToNodes(targetNodeIds);

    statusBar.setText("PolyHost 0.1  |  MIDI Panic sent",
                      juce::dontSendNotification);

    juce::Timer::callAfterDelay(1500, [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
            safe->updateStatusBarText();
    });
}

void MainComponent::updateStatusBarText()
{
    juce::String midiText = "No MIDI device selected";
    auto openNames = midiEngine.getOpenDeviceNames();

    if (!openNames.isEmpty())
        midiText = "MIDI: " + openNames.joinIntoString(", ");

    juce::String tempoModeText = "Internal Tempo";

    statusBar.setText("PolyHost 0.1  |  " + midiText + "  |  " + tempoModeText + "  |  Ready",
                      juce::dontSendNotification);
}

