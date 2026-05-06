#include "MainComponent.h"
#include <cmath>
#include "ButtonStyling.h"
#include "DebugLog.h"

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
            titleLabel.setText("PolyHostInterface", juce::dontSendNotification);
            titleLabel.setJustificationType(juce::Justification::centred);
            titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(titleLabel);

            versionLabel.setText("Version " + juce::String(POLYHOST_VERSION_STRING), juce::dontSendNotification);
            versionLabel.setJustificationType(juce::Justification::centred);
            versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(versionLabel);

            infoLabel.setText("A lightweight tabbed plugin host for:\nVST2, VST3 and CLAP.\nBuilt with JUCE.\n\nVST2 32-bit bridging planned.",
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

    ButtonStyling::drawLabel(g,
                             ButtonStyling::Glyphs::add(),
                             getLocalBounds().reduced(2, 0),
                             false,
                             14.0f,
                             true);
}

MainComponent::MainComponent()
{
    DebugLog::setEnabled(settings.getDebugLoggingEnabled());
    DebugLog::setAdvancedEnabled(settings.getAdvancedDebugLoggingEnabled());

    if (settings.getClearDebugLogOnStartup())
        DebugLog::clear();

    DebugLog::write("MainComponent startup");

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

        DebugLog::write("onTempoChanged fired | suppressDirtyMarking="
                        + juce::String(suppressDirtyMarking ? "true" : "false")
                        + " | isLoadingPreset="
                        + juce::String(isLoadingPreset ? "true" : "false")
                        + " | nowMs="
                        + juce::String(nowMs)
                        + " | ignoreDirtyChangesUntilMs="
                        + juce::String(ignoreDirtyChangesUntilMs));

        if (!suppressDirtyMarking
            && !isLoadingPreset
            && nowMs >= ignoreDirtyChangesUntilMs)
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

        statusBar.setText("PolyHostInterface  |  Default BPM set to " + juce::String(bpm, 1),
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
    addTabButton.setTooltip(ButtonStyling::Tooltips::addTab());
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

    routingView.onCloseTab = [this](int tabIndex)
    {
        closeTabAt(tabIndex);
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

    routingView.onPointerUseTabSettingsChanged = [this](int tabIndex, bool shouldUse)
    {
        setPointerTabUseOverride(tabIndex, shouldUse);
    };

    routingView.onPointerJumpFilterChanged = [this](int tabIndex, int value)
    {
        setPointerTabJumpFilter(tabIndex, value);
    };

    routingView.onPointerMaxStepSizeChanged = [this](int tabIndex, int value)
    {
        setPointerTabMaxStepSize(tabIndex, value);
    };

    routingView.onPointerStepMultiplierChanged = [this](int tabIndex, int value)
    {
        setPointerTabStepMultiplier(tabIndex, value);
    };

    routingView.onPointerResetRequested = [this](int tabIndex)
    {
        resetPointerTabSettingsToDefaults(tabIndex);
    };

    addAndMakeVisible(pointerControlView);
    pointerControlView.setVisible(false);

    pointerControlView.setPointerControlEnabled(settings.getPointerControlEnabled());
    pointerControlView.setAdjustCcNumber(settings.getPointerControlAdjustCcNumber());
    pointerControlView.setDiscontinuityThreshold(settings.getPointerControlDiscontinuityThreshold());
    pointerControlView.setDeltaClamp(settings.getPointerControlDeltaClamp());

    pointerControl.setEnabled(settings.getPointerControlEnabled());
    pointerControl.setAdjustCcNumber(settings.getPointerControlAdjustCcNumber());
    pointerControl.setDiscontinuityThreshold(settings.getPointerControlDiscontinuityThreshold());
    pointerControl.setDeltaClamp(settings.getPointerControlDeltaClamp());

    pointerControlView.onEnabledChanged = [this](bool shouldEnable)
    {
        settings.setPointerControlEnabled(shouldEnable);
        pointerControl.setEnabled(shouldEnable);
    };

    pointerControlView.onAdjustCcChanged = [this](int ccNumber)
    {
        settings.setPointerControlAdjustCcNumber(ccNumber);
        pointerControl.setAdjustCcNumber(ccNumber);
        pointerControl.resetInteractionState();
    };

    pointerControlView.onDiscontinuityThresholdChanged = [this](int threshold)
    {
        settings.setPointerControlDiscontinuityThreshold(threshold);
        pointerControl.setDiscontinuityThreshold(threshold);
        pointerControl.resetInteractionState();
    };

    pointerControlView.onDeltaClampChanged = [this](int clampAmount)
    {
        settings.setPointerControlDeltaClamp(clampAmount);
        pointerControl.setDeltaClamp(clampAmount);
    };

    addEmptyTab();

    refreshRoutingView();

    ensurePointerSettingsForCurrentTabs();
    applyPointerSettingsToCurrentTab();

    auto enabledMidiDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

    for (auto& identifier : enabledMidiDeviceIdentifiers)
        midiEngine.openDevice(identifier);

    auto openNames = midiEngine.getOpenDeviceNames();

    if (!openNames.isEmpty())
    {
        statusBar.setText("PolyHostInterface  |  MIDI: " + openNames.joinIntoString(", ") + "  |  Ready",
                          juce::dontSendNotification);
    }
    else
    {
        auto legacySavedMidiDevice = settings.getMidiDeviceName();

        if (legacySavedMidiDevice.isNotEmpty())
        {
            midiEngine.openDevice(legacySavedMidiDevice);
            statusBar.setText("PolyHostInterface  |  MIDI: " + midiEngine.getCurrentDeviceName() + "  |  Ready",
                              juce::dontSendNotification);
        }
        else
        {
            statusBar.setText("PolyHostInterface  |  No MIDI device selected  |  Ready",
                              juce::dontSendNotification);
        }
    }

    pointerControl.setPointerDebugLoggingEnabledProvider([this]()
    {
        return settings.getDebugLoggingEnabled()
            && settings.getPointerControlDebugLoggingEnabled();
    });

    midiEngine.onMidiMessageReceived = [this](const MidiEngine::IncomingMidiMessage& incoming)
    {
        auto* currentTab = getTabComponent(tabs.getCurrentTabIndex());

        if (currentTab == nullptr || !currentTab->hasPlugin())
            pointerControl.resetInteractionState();

        if (currentTab != nullptr)
        {
            if (pointerControl.handleMidiMessageForCurrentTab(incoming.message, currentTab))
                return;
        }

        juce::Array<juce::AudioProcessorGraph::NodeID> targetNodeIds;

        for (int i = 0; i < pluginTabs.size(); ++i)
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

    for (int i = 0; i < pluginTabs.size(); ++i)
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

            if (auto* pointerState = getPointerSettingsForTab(i))
            {
                tab.pointerControlUseTabSettings = pointerState->useTabSettings;
                tab.pointerControlJumpFilter = pointerState->jumpFilter;
                tab.pointerControlMaxStepSize = pointerState->maxStepSize;
                tab.pointerControlStepMultiplier = pointerState->stepMultiplier;
            }

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
    resetAllTabsAndRouting();

    standaloneTempoSupport.setDefaultTempoBpm(session.hostTempoBpm);
    standaloneTempoSupport.setHostTempoBpm(session.hostTempoBpm);
    standaloneTempoSupport.refreshTempoStrip();

    for (int i = 0; i < session.tabs.size(); ++i)
    {
        auto* tc = pluginTabs.add(new PluginTabComponent(audioEngine, pluginTabs.size()));
        configurePluginTabComponent(*tc);
        tc->addChangeListener(this);
    }

    if (pluginTabs.isEmpty())
        addEmptyTab();
    else
        rebuildVisibleTabs();

    ensureMidiRoutingStateForCurrentTabs();
    ensurePointerSettingsForCurrentTabs();

    for (int i = 0; i < session.tabs.size(); ++i)
    {
        if (!juce::isPositiveAndBelow(i, tabs.getNumTabs()))
            break;

        const auto& tabData = session.tabs.getReference(i);

        if (auto* midiState = getMidiRoutingStateForTab(i))
            midiState->assignedDeviceIdentifiers = tabData.midiAssignedDeviceIdentifiers;

        if (auto* pointerState = getPointerSettingsForTab(i))
        {
            pointerState->useTabSettings = tabData.pointerControlUseTabSettings;
            pointerState->jumpFilter = tabData.pointerControlJumpFilter;
            pointerState->maxStepSize = tabData.pointerControlMaxStepSize;
            pointerState->stepMultiplier = tabData.pointerControlStepMultiplier;
        }

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
                    bool restoredState = true;

                    if (tabData.plugin.pluginStateBase64.isNotEmpty())
                    {
                        if (state.fromBase64Encoding(tabData.plugin.pluginStateBase64))
                            restoredState = tc->restorePluginState(state);
                        else
                            restoredState = false;
                    }

                    if (!restoredState)
                    {
                        loadErrors.add("Failed to restore plugin state: " + tabData.plugin.pluginName
                                       + "\n  Path: " + pluginFile.getFullPathName());
                    }

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

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (source == tc)
            {
                if (tc->hasPlugin())
                    autoAssignGlobalMidiToTabIfAppropriate(i);

                refreshTabAppearance(i);
                syncRoutingToAudioEngine();
                refreshRoutingView();

                DebugLog::writeAdvanced("PluginTab changeListenerCallback fired for tab "
                                        + juce::String(i)
                                        + " | plugin="
                                        + tc->getPluginName()
                                        + " | suppressDirtyMarking="
                                        + juce::String(suppressDirtyMarking ? "true" : "false")
                                        + " | isLoadingPreset="
                                        + juce::String(isLoadingPreset ? "true" : "false")
                                        + " | stateDiffersFromBaseline="
                                        + juce::String(tc->doesCurrentPluginStateDifferFromBaseline() ? "true" : "false"));

                if (!suppressDirtyMarking && !isLoadingPreset)
                {
                    if (tc->doesCurrentPluginStateDifferFromBaseline())
                    {
                        if (!shouldSuppressDirtyForSinglePluginQuickOpen())
                            markSessionDirty();
                    }
                    else if (sessionDocument.isDirty())
                    {
                        bool anyTabDiffers = false;

                        for (int tabIndex = 0; tabIndex < pluginTabs.size(); ++tabIndex)
                        {
                            if (auto* otherTab = getTabComponent(tabIndex))
                            {
                                if (otherTab->doesCurrentPluginStateDifferFromBaseline())
                                {
                                    anyTabDiffers = true;
                                    break;
                                }
                            }
                        }

                        if (!anyTabDiffers)
                            markSessionClean();
                    }
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

void MainComponent::handleSuccessfulPluginLoadIntoTab(int tabIndex, bool markDirtyAfterLoad)
{
    if (!juce::isPositiveAndBelow(tabIndex, pluginTabs.size()))
        return;

    tabs.setCurrentTabIndex(tabIndex);
    autoAssignGlobalMidiToTabIfAppropriate(tabIndex);
    refreshTabAppearance(tabIndex);
    syncRoutingToAudioEngine();
    refreshRoutingView();

    if (auto* tc = getTabComponent(tabIndex))
    {
        tc->resized();
        tc->repaint();
    }

    resized();
    handleCurrentTabChanged();
    resizeWindowToFitCurrentTab();

    if (markDirtyAfterLoad)
        markSessionDirty();
}

void MainComponent::loadPluginFromFile(const juce::File& file)
{
    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
            {
                if (tc->loadPlugin(file))
                {
                    handleSuccessfulPluginLoadIntoTab(i, false);

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
            handleSuccessfulPluginLoadIntoTab(tabs.getNumTabs() - 1, false);

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
            handleSuccessfulPluginLoadIntoTab(currentIndex, true);
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
            handleSuccessfulPluginLoadIntoTab(newTabIndex, true);
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
            handleSuccessfulPluginLoadIntoTab(targetTabIndex, true);
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
            handleSuccessfulPluginLoadIntoTab(targetTabIndex, true);
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
        for (int i = 0; i < pluginTabs.size(); ++i)
        {
            if (getTabComponent(i) == &tabComponent)
            {
                handleDroppedPluginFile(file, i);
                return;
            }
        }
    };

    tabComponent.onPluginLoadedDirectly = [this, &tabComponent]
    {
        for (int i = 0; i < pluginTabs.size(); ++i)
        {
            if (getTabComponent(i) == &tabComponent)
            {
                handleSuccessfulPluginLoadIntoTab(i, true);
                return;
            }
        }
    };
}

void MainComponent::addEmptyTab()
{
    auto* tc = pluginTabs.add(new PluginTabComponent(audioEngine, pluginTabs.size()));
    configurePluginTabComponent(*tc);
    tc->addChangeListener(this);

    rebuildVisibleTabs();
    ensurePointerSettingsForCurrentTabs();

    if (!shouldSuppressDirtyForSinglePluginQuickOpen())
        markSessionDirty();
}

void MainComponent::rebuildVisibleTabs()
{
    const auto* previouslySelected = getTabComponent(tabs.getCurrentTabIndex());

    while (tabs.getNumTabs() > 0)
        tabs.removeTab(tabs.getNumTabs() - 1);

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        auto* tc = pluginTabs[i];
        if (tc == nullptr)
            continue;

        tc->setSlotIndex(i);

        juce::String name = "Empty";
        if (tc->hasPlugin())
            name = tc->getPluginName();

        auto colour = PluginTabComponent::colourForType(tc->getType());

        tabs.addTab(name, colour, tc, false);
    }

    int selectedIndex = 0;

    if (previouslySelected != nullptr)
    {
        const int found = getTabIndexForComponent(previouslySelected);
        if (found >= 0)
            selectedIndex = found;
    }

    if (tabs.getNumTabs() > 0)
        tabs.setCurrentTabIndex(juce::jlimit(0, tabs.getNumTabs() - 1, selectedIndex));

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);
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
    for (int i = 0; i < pluginTabs.size(); ++i)
        if (auto* tc = getTabComponent(i)) if (tc->getType() == type) ++count;
    return count;
}

int MainComponent::getLoadedPluginTabCount() const
{
    int count = 0;

    for (int i = 0; i < pluginTabs.size(); ++i)
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
    if (!juce::isPositiveAndBelow(index, pluginTabs.size()))
        return nullptr;

    return pluginTabs[index];
}

void MainComponent::handleCurrentTabChanged()
{
    pointerControl.resetInteractionState();
    ensurePointerSettingsForCurrentTabs();
    applyPointerSettingsToCurrentTab();

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    refreshRoutingView();

    if (!showingRoutingView)
        resizeWindowToFitCurrentTab();
}

int MainComponent::getTabIndexForComponent(const PluginTabComponent* component) const
{
    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (pluginTabs[i] == component)
            return i;
    }

    return -1;
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
    pointerControlView.setBounds(area);

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
            menu.addItem(203, "Reset Routing Window Size");
            break;
        case 2:
        {
            refreshMidiDevices();

            juce::PopupMenu midiInputDeviceMenu;
            auto devices = midiEngine.getAvailableDevices();

            if (devices.isEmpty())
            {
                midiInputDeviceMenu.addItem(1300, "(No MIDI input devices found)", false, false);
            }
            else
            {
                for (int i = 0; i < devices.size(); ++i)
                {
                    const auto& device = devices.getReference(i);
                    midiInputDeviceMenu.addItem(1000 + i,
                                                device.name,
                                                true,
                                                midiEngine.isDeviceOpen(device.identifier));
                }
            }

            menu.addSubMenu("Select MIDI Input Device", midiInputDeviceMenu);
            menu.addItem(302, "MIDI Monitor");
            menu.addItem(304, "Refresh MIDI Devices");
            menu.addSeparator();

            juce::PopupMenu autoAssignMenu;
            auto mode = settings.getMidiAutoAssignMode().trim();
            if (mode.isEmpty())
                mode = "firstTabOnly";

            autoAssignMenu.addItem(305, "First tab only", true, mode == "firstTabOnly");
            autoAssignMenu.addItem(306, "All new tabs", true, mode == "allNewTabs");
            autoAssignMenu.addItem(307, "None", true, mode == "none");

            menu.addSubMenu("Auto-Assign MIDI to New Tabs", autoAssignMenu);
            menu.addSeparator();

            menu.addItem(303, "Panic");
            break;
        }
        case 3: menu.addItem(401, "Record Audio...  [TODO]"); menu.addItem(402, "Record MIDI...  [TODO]"); break;
        case 4:
        {
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

            juce::PopupMenu debugMenu;
             debugMenu.addItem(505,
                               "Enable Debug Logging",
                               true,
                               settings.getDebugLoggingEnabled());
             debugMenu.addItem(507,
                               "Enable Advanced Logging",
                               settings.getDebugLoggingEnabled(),
                               settings.getAdvancedDebugLoggingEnabled());
             debugMenu.addItem(509,
                               "Enable Pointer Control Logging",
                               settings.getDebugLoggingEnabled(),
                               settings.getPointerControlDebugLoggingEnabled());
             debugMenu.addSeparator();
             debugMenu.addItem(506,
                               "Clear Log On Startup",
                               true,
                               settings.getClearDebugLogOnStartup());
             debugMenu.addItem(508, "Clear Log Now");

            menu.addSubMenu("Debug", debugMenu);
            break;
        }
        case 5: menu.addItem(601, "About PolyHostInterface"); break;
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

    if (itemId >= 1000 && itemId < 1300)
    {
        auto devices = midiEngine.getAvailableDevices();
        const int deviceIndex = itemId - 1000;

        if (juce::isPositiveAndBelow(deviceIndex, devices.size()))
        {
            const auto& device = devices.getReference(deviceIndex);

            if (midiEngine.isDeviceOpen(device.identifier))
                midiEngine.closeDevice(device.identifier);
            else
                midiEngine.openDevice(device.identifier);

            auto openNames = midiEngine.getOpenDeviceNames();

            if (openNames.isEmpty())
            {
                statusBar.setText(
                    "PolyHostInterface  |  No MIDI device selected  |  Ready",
                    juce::dontSendNotification);
                settings.setMidiDeviceName({});
                settings.setEnabledMidiDeviceIdentifiers({});
            }
            else
            {
                statusBar.setText(
                    "PolyHostInterface  |  MIDI: " + openNames.joinIntoString(", ") + "  |  Ready",
                    juce::dontSendNotification);

                settings.setMidiDeviceName(openNames[0]); // legacy compatibility
                settings.setEnabledMidiDeviceIdentifiers(midiEngine.getOpenDeviceIdentifiers());
            }

            refreshRoutingView();
            return;
        }
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
        case 203:
            resetRoutingWindowSizeToDefault();
            break;

        case 302:
        {
            if (midiMonitorWindow == nullptr)
                midiMonitorWindow = std::make_unique<MidiMonitorWindow>(midiEngine);

            midiMonitorWindow->setVisible(true);
            midiMonitorWindow->toFront(true);
            break;
        }
        case 304:
            refreshMidiDevices();
            break;
        case 305:
        case 306:
        case 307:
        {
            juce::String newMode = "firstTabOnly";

            if (itemId == 306)
                newMode = "allNewTabs";
            else if (itemId == 307)
                newMode = "none";

            settings.setMidiAutoAssignMode(newMode);
            refreshRoutingView();
            updateStatusBarText();

            if (newMode != "none")
            {
                const bool applyNow = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::QuestionIcon,
                    "Apply MIDI Auto-Assign Mode",
                    "Apply this MIDI auto-assign mode to existing loaded tabs that currently have no MIDI devices assigned?",
                    "Apply Now",
                    "Later");

                if (applyNow)
                    applyMidiAutoAssignModeToExistingTabs();
            }

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
        case 505:
        {
            auto enabled = !settings.getDebugLoggingEnabled();
            settings.setDebugLoggingEnabled(enabled);
            DebugLog::setEnabled(enabled);

            if (!enabled)
            {
                settings.setAdvancedDebugLoggingEnabled(false);
                DebugLog::setAdvancedEnabled(false);
            }

            DebugLog::write("Debug logging " + juce::String(enabled ? "enabled" : "disabled"));
            break;
        }
        case 507:
        {
            if (!settings.getDebugLoggingEnabled())
                break;

            auto advancedEnabled = !settings.getAdvancedDebugLoggingEnabled();
            settings.setAdvancedDebugLoggingEnabled(advancedEnabled);
            DebugLog::setAdvancedEnabled(advancedEnabled);
            DebugLog::write("Advanced debug logging " + juce::String(advancedEnabled ? "enabled" : "disabled"));
            break;
        }
        case 506:
        {
            auto shouldClear = !settings.getClearDebugLogOnStartup();
            settings.setClearDebugLogOnStartup(shouldClear);
            break;
        }
        case 508:
            DebugLog::clear();
            DebugLog::write("Debug log cleared");
            break;
        case 509:
        {
            if (!settings.getDebugLoggingEnabled())
                break;

            auto enabled = !settings.getPointerControlDebugLoggingEnabled();
            settings.setPointerControlDebugLoggingEnabled(enabled);
            DebugLog::write("Pointer Control debug logging " + juce::String(enabled ? "enabled" : "disabled"));
            break;
        }

        case 601:
            showAboutDialog();
            break;
        default: break;
    }
}

void MainComponent::resetAllTabsAndRouting()
{
    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = pluginTabs[i])
            tc->removeChangeListener(this);
    }

    while (tabs.getNumTabs() > 0)
        tabs.removeTab(tabs.getNumTabs() - 1);

    pluginTabs.clear();
    midiRoutingStates.clear();
    pointerTabSettings.clear();
}

void MainComponent::clearAllPlugins()
{
    resetAllTabsAndRouting();
    refreshRoutingView();
    syncRoutingToAudioEngine();
    markSessionDirty();
}

void MainComponent::showAboutDialog()
{
    auto content = std::make_unique<AboutDialogContent>();

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = "About PolyHostInterface";
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

    resetAllTabsAndRouting();
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

    if (!SessionManager::saveSessionToFile(session, file))
        return false;

    for (int tabIndex = 0; tabIndex < tabs.getNumTabs(); ++tabIndex)
    {
        if (auto* tc = getTabComponent(tabIndex))
            tc->capturePluginStateBaseline();
    }

    markSessionClean();
    return true;
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

    resetAllTabsAndRouting();
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
    suppressDirtyMarking = true;
    ignoreDirtyChangesUntilMs = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    DebugLog::write("loadPresetFromFile begin | file=" + file.getFullPathName());
    unresolvedMissingPlugins.clear();
    suppressDirtyForSinglePluginQuickOpen = false;

    juce::StringArray loadErrors = parseWarnings;
    juce::Array<MissingPluginEntry> missingPlugins;

    applySessionData(session, loadErrors, missingPlugins);
    DebugLog::write("loadPresetFromFile after applySessionData");

    unresolvedMissingPlugins = missingPlugins;
    presetSessionController.rememberLoadedFile(file);
    refreshPresetDropdown();
    showPresetLoadErrors(loadErrors);

    promptToLocateMissingPlugins(unresolvedMissingPlugins);

    DebugLog::write("loadPresetFromFile markSessionClean immediate");
    markSessionClean();

    juce::Timer::callAfterDelay(5000, [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
        {
            DebugLog::write("loadPresetFromFile delayed cleanup fired");
            safe->isLoadingPreset = false;
            safe->ignoreDirtyChangesUntilMs = 0.0;
            safe->suppressDirtyMarking = false;

            for (int tabIndex = 0; tabIndex < safe->tabs.getNumTabs(); ++tabIndex)
            {
                if (auto* tc = safe->getTabComponent(tabIndex))
                    tc->capturePluginStateBaseline();
            }

            safe->markSessionClean();
        }
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

    if (pluginFile.getFileExtension().equalsIgnoreCase(".clap"))
    {
        auto clapWrapper = std::make_unique<ClapPluginWrapper>(pluginFile);
        if (!clapWrapper->isLoaded())
            return false;

        clapWrapper->fillInPluginDescription(desc);
        desc.fileOrIdentifier = pluginFile.getFullPathName();
        desc.name = clapWrapper->getPluginName();
        desc.descriptiveName = desc.name;
        desc.pluginFormatName = "CLAP";
        return true;
    }

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
    if (suppressDirtyMarking)
    {
        DebugLog::write("markSessionDirty suppressed");
        return;
    }

    if (getLoadedPluginTabCount() > 1)
        suppressDirtyForSinglePluginQuickOpen = false;

    DebugLog::write("markSessionDirty applied");
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

    juce::StringArray candidateLabels;

    for (int i = 0; i < candidates.size(); ++i)
    {
        const auto& candidate = candidates.getReference(i);

        juce::String label = candidate.desc.name;

        if (candidate.desc.manufacturerName.isNotEmpty())
            label += " - " + candidate.desc.manufacturerName;

        if (candidate.desc.pluginFormatName.isNotEmpty())
            label += " [" + candidate.desc.pluginFormatName + "]";

        label += " {" + candidate.file.getFileName() + "}";

        candidateLabels.add(label);
    }

    juce::AlertWindow chooser("Choose Replacement Plugin",
                              "Select a replacement for:\n" + entry.pluginName,
                              juce::AlertWindow::QuestionIcon);

    chooser.addComboBox("replacementChoice", candidateLabels, "Replacement Plugin");
    chooser.addButton("Use Selected", 1, juce::KeyPress(juce::KeyPress::returnKey));
    chooser.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* combo = chooser.getComboBoxComponent("replacementChoice"))
        combo->setSelectedItemIndex(0);

    if (chooser.runModalLoop() != 1)
        return {};

    auto* combo = chooser.getComboBoxComponent("replacementChoice");
    if (combo == nullptr)
        return {};

    const int selectedIndex = combo->getSelectedItemIndex();

    if (!juce::isPositiveAndBelow(selectedIndex, candidates.size()))
        return {};

    return candidates[selectedIndex].file;
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
    if (!juce::isPositiveAndBelow(entry.tabIndex, pluginTabs.size()))
        return false;

    if (auto* tc = getTabComponent(entry.tabIndex))
    {
        if (!tc->loadPlugin(replacementFile))
            return false;

        lastPluginRepairDirectory = replacementFile.getParentDirectory();

        juce::MemoryBlock state;
        if (state.fromBase64Encoding(entry.pluginStateBase64))
            tc->restorePluginState(state);

        handleSuccessfulPluginLoadIntoTab(entry.tabIndex, true);
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
bool MainComponent::clearTabAt(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return false;

    if (!confirmClearTab(tabIndex))
        return false;

    if (auto* tc = getTabComponent(tabIndex))
    {
        tc->clearSavedWindowBounds();
        tc->clearPlugin();
        refreshTabAppearance(tabIndex);
        refreshRoutingView();
        unresolvedMissingPlugins.clear();
        markSessionDirty();
        return true;
    }

    return false;
}

bool MainComponent::closeTabAt(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return false;

    if (!confirmCloseTab(tabIndex))
        return false;

    if (tabs.getNumTabs() == 1)
        return clearTabAt(tabIndex);

    auto currentIndex = tabs.getCurrentTabIndex();
    const bool wasCurrentTab = (tabIndex == currentIndex);

    removeTabAt(tabIndex);

    if (tabs.getNumTabs() > 0)
    {
        if (wasCurrentTab)
        {
            currentIndex = juce::jlimit(0, tabs.getNumTabs() - 1, tabIndex);
        }
        else
        {
            if (tabIndex < currentIndex)
                --currentIndex;

            currentIndex = juce::jlimit(0, tabs.getNumTabs() - 1, currentIndex);
        }

        tabs.setCurrentTabIndex(currentIndex);
    }

    handleCurrentTabChanged();
    unresolvedMissingPlugins.clear();
    markSessionDirty();
    return true;
}

void MainComponent::removeTabAt(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, pluginTabs.size()))
        return;

    if (auto* tc = getTabComponent(tabIndex))
        tc->removeChangeListener(this);

    pluginTabs.remove(tabIndex, true);

    for (int i = midiRoutingStates.size(); --i >= 0;)
    {
        if (midiRoutingStates.getReference(i).tabIndex == tabIndex)
            midiRoutingStates.remove(i);
    }

    for (int i = 0; i < midiRoutingStates.size(); ++i)
    {
        if (midiRoutingStates.getReference(i).tabIndex > tabIndex)
            --midiRoutingStates.getReference(i).tabIndex;
    }

    for (int i = pointerTabSettings.size(); --i >= 0;)
    {
        if (pointerTabSettings.getReference(i).tabIndex == tabIndex)
            pointerTabSettings.remove(i);
    }

    for (int i = 0; i < pointerTabSettings.size(); ++i)
    {
        if (pointerTabSettings.getReference(i).tabIndex > tabIndex)
            --pointerTabSettings.getReference(i).tabIndex;
    }

    if (pluginTabs.isEmpty())
    {
        addEmptyTab();
        return;
    }

    rebuildVisibleTabs();
    ensureMidiRoutingStateForCurrentTabs();
    refreshRoutingView();
    syncRoutingToAudioEngine();
}

void MainComponent::closeCurrentTab()
{
    auto currentIndex = tabs.getCurrentTabIndex();

    if (!juce::isPositiveAndBelow(currentIndex, tabs.getNumTabs()))
        return;

    closeTabAt(currentIndex);
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
                               clearTabAt(tabIndex);
                               return;
                           }

                           if (result != 3)
                               return;

                           closeTabAt(tabIndex);
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

    reorderLiveTabs(tabIndex, targetIndex);
}

void MainComponent::moveTabLater(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    int targetIndex = -1;

    for (int i = tabIndex + 1; i < pluginTabs.size(); ++i)
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

    reorderLiveTabs(tabIndex, targetIndex);
}

// ===================================
// ROUTING - AUDIO / MIDI
// ===================================
void MainComponent::getAllToolbarItemIds(juce::Array<int>& ids)
{
    ids.add(toolbarRoutingToggle);
    ids.add(toolbarSpacer);
    ids.add(toolbarPointerControl);
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
    ids.add(toolbarPointerControl);
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
        case toolbarPointerControl:     return ButtonStyling::Glyphs::pointerControl();
        case toolbarRoutingToggle:      return ButtonStyling::Glyphs::routing();
        case toolbarRefitWindow:        return ButtonStyling::Glyphs::fitWindow();
        case toolbarSavePreset:         return ButtonStyling::Glyphs::save();
        case toolbarSavePresetAs:       return ButtonStyling::Glyphs::saveAs();
        case toolbarRevertPreset:       return ButtonStyling::Glyphs::revert();
        case toolbarMidiPanic:          return ButtonStyling::Glyphs::panic();
        case toolbarSaveTabWindowSize:  return ButtonStyling::Glyphs::saveWindowSize();
        case toolbarClearTabWindowSize: return ButtonStyling::Glyphs::clearWindowSize();
        default:                        break;
    }

    return "?";
}

juce::ToolbarItemComponent* MainComponent::createItem(int itemId)
{
    if (itemId == toolbarPointerControl)
    {
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                                            ButtonStyling::Tooltips::pointerControl(),
                                                            getToolbarIconGlyph(itemId),
                                                            ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                                            ButtonStyling::defaultButtonWidth(),
                                                            [this] { return showingPointerControlView; });

        button->onClick = [this, button]
        {
            togglePointerControlView();
            button->setToggleState(showingPointerControlView, juce::dontSendNotification);
            button->repaint();
        };

        button->setToggleState(showingPointerControlView, juce::dontSendNotification);
        return button;
    }

    if (itemId == toolbarRoutingToggle)
    {
    // Toolbar button sizing
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::routing(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
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
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::fitWindow(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth());

        button->onClick = [this]
        {
            resizeWindowToFitCurrentTab();
        };

        return button;
    }

    if (itemId == toolbarSaveTabWindowSize)
    {
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::saveWindowSize(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
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
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::clearWindowSize(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
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
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::savePreset(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
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
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::savePresetAs(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
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
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::revertPreset(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth());

        button->onClick = [this]
        {
            revertCurrentPreset();
        };

        return button;
    }

    if (itemId == toolbarMidiPanic)
    {
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::midiPanic(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
                                               {},
                                               ButtonStyling::destructiveBackground(),
                                               1);

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
    auto* top = findParentComponentOfClass<juce::DocumentWindow>();

    if (showingRoutingView && !shouldShow && top != nullptr)
        settings.setRoutingWindowSize(top->getWidth(), top->getHeight());

    showingRoutingView = shouldShow;
    pointerControl.resetInteractionState();

    if (showingRoutingView)
        showingPointerControlView = false;

    if (showingRoutingView)
        refreshMidiDevices();

    tabs.setVisible(!showingRoutingView && !showingPointerControlView);
    routingView.setVisible(showingRoutingView);
    pointerControlView.setVisible(showingPointerControlView);

    if (auto* item = toolbar.getItemComponent(toolbarRoutingToggle))
    {
        item->setToggleState(showingRoutingView, juce::dontSendNotification);
        item->repaint();
    }

    toolbar.repaint();
    repaint();
    resized();

    if (showingRoutingView)
        resizeWindowForRoutingView();
    else
        resizeWindowToFitCurrentTab();
}

void MainComponent::toggleRoutingView()
{
    setRoutingViewVisible(!showingRoutingView);
}

void MainComponent::resizeWindowForRoutingView()
{
    auto* top = findParentComponentOfClass<juce::DocumentWindow>();
    if (top == nullptr)
        return;
    // Set default Routing page window size
    const int routingWidth = juce::jmax(850, settings.getRoutingWindowWidth());
    const int routingHeight = juce::jmax(600, settings.getRoutingWindowHeight());

    top->setSize(routingWidth, routingHeight);
}

void MainComponent::resetRoutingWindowSizeToDefault()
{
    settings.clearRoutingWindowSize();

    if (showingRoutingView)
        resizeWindowForRoutingView();
}

void MainComponent::refreshRoutingView()
{
    juce::Array<RoutingView::ModuleEntry> modules;

    for (int tabIndex = 0; tabIndex < pluginTabs.size(); ++tabIndex)
    {
        auto* tc = getTabComponent(tabIndex);
        if (tc == nullptr || !tc->hasPlugin())
            continue;

        RoutingView::ModuleEntry entry;
        entry.tabIndex = tabIndex;

        auto pluginName = tc->getPluginName().trim();
        if (pluginName.isEmpty())
        {
            if (tc->getType() == PluginTabComponent::SlotType::Synth)
                pluginName = "Synth Tab " + juce::String(tabIndex + 1);
            else if (tc->getType() == PluginTabComponent::SlotType::FX)
                pluginName = "FX Tab " + juce::String(tabIndex + 1);
            else
                pluginName = "Tab " + juce::String(tabIndex + 1);
        }

        entry.name = pluginName;
        entry.type = tc->getType();
        entry.isBypassed = tc->isBypassed();
        entry.canMoveUp = (tabIndex > 0);
        entry.canMoveDown = (tabIndex < pluginTabs.size() - 1);
        if (const auto* midiState = getMidiRoutingStateForTab(tabIndex))
            entry.midiAssignmentCount = midiState->assignedDeviceIdentifiers.size();
        else
            entry.midiAssignmentCount = 0;

        if (const auto* pointerState = getPointerSettingsForTab(tabIndex))
        {
            entry.pointerUseTabSettings = pointerState->useTabSettings;
            entry.pointerJumpFilter = pointerState->jumpFilter;
            entry.pointerMaxStepSize = pointerState->maxStepSize;
            entry.pointerStepMultiplier = pointerState->stepMultiplier;
        }

        juce::StringArray relatedNames;

        if (tc->getType() == PluginTabComponent::SlotType::Synth && !tc->isBypassed())
        {
            for (int j = tabIndex + 1; j < pluginTabs.size(); ++j)
            {
                if (auto* downstream = getTabComponent(j))
                {
                    if (!downstream->hasPlugin() || downstream->isBypassed())
                        continue;

                    if (downstream->getType() == PluginTabComponent::SlotType::FX)
                    {
                        auto fxName = downstream->getPluginName().trim();
                        if (fxName.isEmpty())
                            fxName = "FX Tab " + juce::String(j + 1);

                        relatedNames.add(fxName);
                    }
                }
            }

            if (relatedNames.isEmpty())
            {
                entry.routingTooltip = "Output:\nDirect to output";
            }
            else
            {
                entry.routingTooltip = "Output:\n" + relatedNames.joinIntoString("\n");
            }
        }
        else if (tc->getType() == PluginTabComponent::SlotType::FX && !tc->isBypassed())
        {
            for (int j = 0; j < tabIndex; ++j)
            {
                if (auto* upstream = getTabComponent(j))
                {
                    if (!upstream->hasPlugin() || upstream->isBypassed())
                        continue;

                    if (upstream->getType() == PluginTabComponent::SlotType::Synth)
                    {
                        auto synthName = upstream->getPluginName().trim();
                        if (synthName.isEmpty())
                            synthName = "Synth Tab " + juce::String(j + 1);

                        relatedNames.add(synthName);
                    }
                }
            }

            if (relatedNames.isEmpty())
            {
                entry.routingTooltip = "Input:\nNo synth inputs";
            }
            else
            {
                entry.routingTooltip = "Input:\n" + relatedNames.joinIntoString("\n");
            }
        }
        else
        {
            entry.routingTooltip.clear();
        }

        modules.add(entry);
    }

    routingView.setModules(modules);
}

void MainComponent::setPointerControlViewVisible(bool shouldShow)
{
    showingPointerControlView = shouldShow;

    if (showingPointerControlView)
        showingRoutingView = false;

    pointerControl.resetInteractionState();

    tabs.setVisible(!showingPointerControlView && !showingRoutingView);
    routingView.setVisible(showingRoutingView);
    pointerControlView.setVisible(showingPointerControlView);

    if (auto* item = toolbar.getItemComponent(toolbarPointerControl))
    {
        item->setToggleState(showingPointerControlView, juce::dontSendNotification);
        item->repaint();
    }

    if (auto* item = toolbar.getItemComponent(toolbarRoutingToggle))
    {
        item->setToggleState(showingRoutingView, juce::dontSendNotification);
        item->repaint();
    }

    toolbar.repaint();
    repaint();
    resized();
}

void MainComponent::togglePointerControlView()
{
    setPointerControlViewVisible(!showingPointerControlView);
}

bool MainComponent::shouldAutoAssignMidiToNewTabs() const
{
    auto mode = settings.getMidiAutoAssignMode().trim();

    if (mode.isEmpty())
        mode = "firstTabOnly";

    return mode == "firstTabOnly" || mode == "allNewTabs";
}

bool MainComponent::shouldAutoAssignMidiOnlyToFirstTab() const
{
    auto mode = settings.getMidiAutoAssignMode().trim();

    if (mode.isEmpty())
        mode = "firstTabOnly";

    return mode == "firstTabOnly";
}

void MainComponent::reorderLiveTabs(int fromIndex, int toIndex)
{
    if (!juce::isPositiveAndBelow(fromIndex, pluginTabs.size()))
        return;

    if (!juce::isPositiveAndBelow(toIndex, pluginTabs.size()))
        return;

    if (fromIndex == toIndex)
        return;

    auto* movedComponent = pluginTabs[fromIndex];

    MidiTabRoutingState movedMidiState;
    bool foundMovedMidiState = false;

    if (auto* state = getMidiRoutingStateForTab(fromIndex))
    {
        movedMidiState = *state;
        foundMovedMidiState = true;
    }

    pluginTabs.remove(fromIndex, false);
    pluginTabs.insert(toIndex, movedComponent);

    if (juce::isPositiveAndBelow(fromIndex, midiRoutingStates.size()))
        midiRoutingStates.remove(fromIndex);

    if (foundMovedMidiState)
    {
        movedMidiState.tabIndex = toIndex;
        midiRoutingStates.insert(toIndex, movedMidiState);
    }

    for (int i = 0; i < midiRoutingStates.size(); ++i)
        midiRoutingStates.getReference(i).tabIndex = i;

    rebuildVisibleTabs();
    refreshRoutingView();
    syncRoutingToAudioEngine();
    handleCurrentTabChanged();
    markSessionDirty();
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
    ensureMidiRoutingStateForCurrentTabs();

    auto* midiState = getMidiRoutingStateForTab(tabIndex);
    if (midiState == nullptr)
        return;

    if (!midiState->assignedDeviceIdentifiers.isEmpty())
        return;

    auto enabledDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

    if (enabledDeviceIdentifiers.isEmpty())
        enabledDeviceIdentifiers = midiEngine.getOpenDeviceIdentifiers();

    for (auto& identifier : enabledDeviceIdentifiers)
        midiState->assignedDeviceIdentifiers.addIfNotAlreadyThere(identifier);
}

void MainComponent::autoAssignGlobalMidiToTabIfAppropriate(int tabIndex)
{
    if (!juce::isPositiveAndBelow(tabIndex, tabs.getNumTabs()))
        return;

    ensureMidiRoutingStateForCurrentTabs();

    if (!shouldAutoAssignMidiToNewTabs())
        return;

    if (shouldAutoAssignMidiOnlyToFirstTab() && getLoadedPluginTabCount() != 1)
        return;

    auto* tc = getTabComponent(tabIndex);
    if (tc == nullptr || !tc->hasPlugin())
        return;

    assignEnabledGlobalMidiDevicesToTab(tabIndex);
    refreshRoutingView();
    syncRoutingToAudioEngine();
    markSessionDirty();
}

void MainComponent::syncRoutingToAudioEngine()
{
    juce::Array<AudioEngine::RoutingEntry> orderedEntries;

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (!tc->hasPlugin())
                continue;

            if (tc->isBypassed())
                continue;

            AudioEngine::RoutingEntry entry;
            entry.nodeId = tc->getNodeID();
            entry.isSynth = (tc->getType() == PluginTabComponent::SlotType::Synth);
            orderedEntries.add(entry);
        }
    }

    audioEngine.setRoutingState(orderedEntries);
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
    auto enabledDeviceIdentifiers = settings.getEnabledMidiDeviceIdentifiers();

    if (enabledDeviceIdentifiers.isEmpty())
    {
        auto currentlyOpen = midiEngine.getOpenDeviceIdentifiers();

        for (auto& identifier : currentlyOpen)
            enabledDeviceIdentifiers.addIfNotAlreadyThere(identifier);
    }

    midiEngine.refreshDevices(enabledDeviceIdentifiers);

    refreshRoutingView();
    updateStatusBarText();

    auto availableDevices = midiEngine.getAvailableDevices();
    auto openNames = midiEngine.getOpenDeviceNames();

    juce::String message = "PolyHostInterface  |  MIDI refreshed";

    if (!openNames.isEmpty())
        message += "  |  Active: " + openNames.joinIntoString(", ");

    message += "  |  Available devices: " + juce::String(availableDevices.size());

    statusBar.setText(message, juce::dontSendNotification);

    juce::Timer::callAfterDelay(1500, [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
            safe->updateStatusBarText();
    });
}

void MainComponent::applyMidiAutoAssignModeToExistingTabs()
{
    ensureMidiRoutingStateForCurrentTabs();

    if (!shouldAutoAssignMidiToNewTabs())
        return;

    int loadedPluginCountSeen = 0;

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        auto* tc = getTabComponent(i);
        if (tc == nullptr || !tc->hasPlugin())
            continue;

        ++loadedPluginCountSeen;

        if (shouldAutoAssignMidiOnlyToFirstTab() && loadedPluginCountSeen != 1)
            continue;

        assignEnabledGlobalMidiDevicesToTab(i);
    }

    refreshRoutingView();
    syncRoutingToAudioEngine();
    markSessionDirty();
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

    for (int i = 0; i < pluginTabs.size(); ++i)
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

    statusBar.setText("PolyHostInterface  |  MIDI Panic sent",
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

    statusBar.setText("PolyHostInterface  |  " + midiText + "  |  " + tempoModeText + "  |  Ready",
                      juce::dontSendNotification);
}

MainComponent::PointerTabSettings* MainComponent::getPointerSettingsForTab(int tabIndex)
{
    for (auto& state : pointerTabSettings)
        if (state.tabIndex == tabIndex)
            return &state;

    return nullptr;
}

const MainComponent::PointerTabSettings* MainComponent::getPointerSettingsForTab(int tabIndex) const
{
    for (auto& state : pointerTabSettings)
        if (state.tabIndex == tabIndex)
            return &state;

    return nullptr;
}

void MainComponent::ensurePointerSettingsForCurrentTabs()
{
    juce::Array<int> existingTabs;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        existingTabs.add(i);

    for (int i = pointerTabSettings.size(); --i >= 0;)
    {
        if (!existingTabs.contains(pointerTabSettings.getReference(i).tabIndex))
            pointerTabSettings.remove(i);
    }

    for (int i = 0; i < tabs.getNumTabs(); ++i)
    {
        if (getPointerSettingsForTab(i) == nullptr)
        {
            PointerTabSettings state;
            state.tabIndex = i;
            state.useTabSettings = false;
            state.jumpFilter = settings.getPointerControlDiscontinuityThreshold();
            state.maxStepSize = settings.getPointerControlDeltaClamp();
            state.stepMultiplier = 1;
            pointerTabSettings.add(state);
        }
    }
}

void MainComponent::applyPointerSettingsToCurrentTab()
{
    auto currentIndex = tabs.getCurrentTabIndex();
    auto* tabState = getPointerSettingsForTab(currentIndex);

    if (tabState != nullptr && tabState->useTabSettings)
    {
        pointerControl.setDiscontinuityThreshold(tabState->jumpFilter);
        pointerControl.setDeltaClamp(tabState->maxStepSize);
        pointerControl.setStepMultiplier(tabState->stepMultiplier);
    }
    else
    {
        pointerControl.setDiscontinuityThreshold(settings.getPointerControlDiscontinuityThreshold());
        pointerControl.setDeltaClamp(settings.getPointerControlDeltaClamp());
        pointerControl.setStepMultiplier(1);
    }

    pointerControl.resetInteractionState();
}

void MainComponent::setPointerTabUseOverride(int tabIndex, bool shouldUse)
{
    ensurePointerSettingsForCurrentTabs();

    if (auto* state = getPointerSettingsForTab(tabIndex))
    {
        state->useTabSettings = shouldUse;
        applyPointerSettingsToCurrentTab();
        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::setPointerTabJumpFilter(int tabIndex, int value)
{
    ensurePointerSettingsForCurrentTabs();

    if (auto* state = getPointerSettingsForTab(tabIndex))
    {
        state->jumpFilter = juce::jlimit(1, 127, value);
        applyPointerSettingsToCurrentTab();
        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::setPointerTabMaxStepSize(int tabIndex, int value)
{
    ensurePointerSettingsForCurrentTabs();

    if (auto* state = getPointerSettingsForTab(tabIndex))
    {
        state->maxStepSize = juce::jlimit(1, 32, value);
        applyPointerSettingsToCurrentTab();
        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::setPointerTabStepMultiplier(int tabIndex, int value)
{
    ensurePointerSettingsForCurrentTabs();

    if (auto* state = getPointerSettingsForTab(tabIndex))
    {
        state->stepMultiplier = juce::jlimit(1, 8, value);
        applyPointerSettingsToCurrentTab();
        refreshRoutingView();
        markSessionDirty();
    }
}

void MainComponent::resetPointerTabSettingsToDefaults(int tabIndex)
{
    ensurePointerSettingsForCurrentTabs();

    if (auto* state = getPointerSettingsForTab(tabIndex))
    {
        state->useTabSettings = true;
        state->jumpFilter = 12;
        state->maxStepSize = 4;
        state->stepMultiplier = 1;
        applyPointerSettingsToCurrentTab();
        refreshRoutingView();
        markSessionDirty();
    }
}

