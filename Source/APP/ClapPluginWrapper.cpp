#include "ClapPluginWrapper.h"
#include "DebugLog.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

class ClapPluginWrapper::PlaceholderEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PlaceholderEditor(ClapPluginWrapper& ownerIn)
        : juce::AudioProcessorEditor(ownerIn), owner(ownerIn)
    {
        setSize(520, 180);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1E1E2E));

        auto area = getLocalBounds().reduced(16);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        g.drawText(owner.getPluginName(), area.removeFromTop(30), juce::Justification::centredLeft);

        area.removeFromTop(8);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));

        juce::StringArray lines;
        lines.add("CLAP plugin loaded.");
        lines.add("Embedded GUI support detected: " + juce::String(owner.supportsEmbeddedGui() ? "YES" : "NO"));
        lines.add("Generic editor placeholder active.");
        lines.add("Real CLAP GUI embedding not implemented yet.");

        for (auto& line : lines)
            g.drawText(line, area.removeFromTop(22), juce::Justification::centredLeft);
    }

private:
    ClapPluginWrapper& owner;
};

uint32_t CLAP_ABI ClapPluginWrapper::inputEventsSize(const clap_input_events* list)
{
    if (list == nullptr || list->ctx == nullptr)
        return 0;

    auto* events = static_cast<InputEventList*>(list->ctx);
    return (uint32_t) events->eventPtrs.size();
}

const clap_event_header_t* CLAP_ABI ClapPluginWrapper::inputEventsGet(const clap_input_events* list, uint32_t index)
{
    if (list == nullptr || list->ctx == nullptr)
        return nullptr;

    auto* events = static_cast<InputEventList*>(list->ctx);

    if (index >= events->eventPtrs.size())
        return nullptr;

    return events->eventPtrs[index];
}

bool CLAP_ABI ClapPluginWrapper::outputEventsTryPush(const clap_output_events*, const clap_event_header_t*)
{
    return false;
}

ClapPluginWrapper::ClapPluginWrapper(const juce::File& pluginFile)
    : juce::AudioPluginInstance(BusesProperties()
                                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      moduleFile(pluginFile)
{
    DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - start: " + pluginFile.getFullPathName());

    emptyInputEvents.ctx = &currentInputEvents;
    emptyInputEvents.size = &ClapPluginWrapper::inputEventsSize;
    emptyInputEvents.get = &ClapPluginWrapper::inputEventsGet;

    emptyOutputEvents.ctx = nullptr;
    emptyOutputEvents.try_push = &ClapPluginWrapper::outputEventsTryPush;

    DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - before loadModule");

    if (!loadModule())
    {
        DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - loadModule failed: " + lastError);
        return;
    }

    DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - loadModule OK");
    DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - before createPluginInstance");

    if (!createPluginInstance())
    {
        DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - createPluginInstance failed: " + lastError);
        return;
    }

    DebugLog::write("ClapPluginWrapper::ClapPluginWrapper - createPluginInstance OK");
}

ClapPluginWrapper::~ClapPluginWrapper()
{
    releaseResources();

    if (plugin != nullptr && plugin->destroy != nullptr)
        plugin->destroy(plugin);

    plugin = nullptr;
    unloadModule();
}

bool ClapPluginWrapper::isLoaded() const noexcept
{
    return plugin != nullptr;
}

juce::String ClapPluginWrapper::getLastError() const noexcept
{
    return lastError;
}

juce::String ClapPluginWrapper::getPluginName() const noexcept
{
    return pluginName;
}

bool ClapPluginWrapper::hasAudioInputPort() const noexcept
{
    return hasAudioInput;
}

bool ClapPluginWrapper::hasAudioOutputPort() const noexcept
{
    return hasAudioOutput;
}

bool ClapPluginWrapper::isSynthLikePlugin() const noexcept
{
    return !hasAudioInput && hasAudioOutput;
}

bool ClapPluginWrapper::supportsGui() const noexcept
{
    return guiExtension != nullptr;
}

bool ClapPluginWrapper::supportsEmbeddedGui() const noexcept
{
    if (guiExtension == nullptr || guiExtension->is_api_supported == nullptr)
        return false;

   #if JUCE_WINDOWS
    return guiExtension->is_api_supported(plugin, CLAP_WINDOW_API_WIN32, false);
   #else
    return false;
   #endif
}

bool ClapPluginWrapper::createEmbeddedEditorIfNeeded()
{
    if (plugin == nullptr || guiExtension == nullptr)
        return false;

    if (guiCreated)
        return true;

    if (guiExtension->create == nullptr)
        return false;

   #if JUCE_WINDOWS
    if (!guiExtension->create(plugin, CLAP_WINDOW_API_WIN32, false))
        return false;
   #else
    return false;
   #endif

    guiCreated = true;

    if (guiExtension->get_size != nullptr)
    {
        uint32_t w = 0, h = 0;
        if (guiExtension->get_size(plugin, &w, &h))
        {
            if (w > 0) guiWidth = w;
            if (h > 0) guiHeight = h;
        }
    }

    return true;
}

void ClapPluginWrapper::destroyEmbeddedEditor()
{
    if (plugin == nullptr || guiExtension == nullptr)
        return;

    if (guiCreated && guiExtension->destroy != nullptr)
        guiExtension->destroy(plugin);

    guiCreated = false;
    guiAttached = false;
    guiVisible = false;
}

bool ClapPluginWrapper::attachGuiToHostWindow(void* nativeWindowHandle)
{
    DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - start for " + getPluginName());

    if (nativeWindowHandle == nullptr)
    {
        DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - nativeWindowHandle was null");
        return false;
    }

    DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - before createEmbeddedEditorIfNeeded");
    if (!createEmbeddedEditorIfNeeded())
    {
        DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - createEmbeddedEditorIfNeeded failed");
        return false;
    }

    if (guiExtension == nullptr)
    {
        DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - guiExtension was null");
        return false;
    }

    if (guiExtension->set_parent == nullptr)
    {
        DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - guiExtension->set_parent was null");
        return false;
    }

    clap_window window {};
    window.api = CLAP_WINDOW_API_WIN32;
    window.win32 = nativeWindowHandle;

    DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - before set_parent");
    if (!guiExtension->set_parent(plugin, &window))
    {
        DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - set_parent returned false");
        return false;
    }

    guiAttached = true;
    DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - set_parent succeeded");

    if (guiExtension->get_size != nullptr)
    {
        uint32_t w = 0, h = 0;
        if (guiExtension->get_size(plugin, &w, &h))
        {
            if (w > 0) guiWidth = w;
            if (h > 0) guiHeight = h;
        }
    }

    DebugLog::write("ClapPluginWrapper::attachGuiToHostWindow - success");
    return true;
}

void ClapPluginWrapper::detachGuiFromHostWindow()
{
    if (plugin == nullptr || guiExtension == nullptr)
        return;

    if (guiVisible && guiExtension->hide != nullptr)
        guiExtension->hide(plugin);

    guiVisible = false;
    guiAttached = false;
}

void ClapPluginWrapper::setEmbeddedGuiVisible(bool shouldBeVisible)
{
    if (!guiCreated || guiExtension == nullptr)
        return;

    if (shouldBeVisible)
    {
        if (!guiVisible && guiExtension->show != nullptr)
        {
            if (guiExtension->show(plugin))
                guiVisible = true;
        }
    }
    else
    {
        if (guiVisible && guiExtension->hide != nullptr)
        {
            if (guiExtension->hide(plugin))
                guiVisible = false;
        }
    }
}

juce::Rectangle<int> ClapPluginWrapper::getEmbeddedGuiSize() const noexcept
{
    return { 0, 0, (int) guiWidth, (int) guiHeight };
}

bool ClapPluginWrapper::refreshEmbeddedGuiSize()
{
    if (plugin == nullptr || guiExtension == nullptr || guiExtension->get_size == nullptr)
        return false;

    uint32_t w = 0, h = 0;
    if (!guiExtension->get_size(plugin, &w, &h))
        return false;

    if (w > 0) guiWidth = w;
    if (h > 0) guiHeight = h;
    return true;
}

bool ClapPluginWrapper::didLastStateLoadSucceed() const noexcept
{
    return lastStateLoadSucceeded;
}

void ClapPluginWrapper::clearLastStateLoadResult() noexcept
{
    lastStateLoadSucceeded = true;
}

void ClapPluginWrapper::buildInputEventsFromMidi(const juce::MidiBuffer& midiMessages)
{
    currentInputEvents.clear();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const auto sampleOffset = (uint32_t) juce::jmax(0, metadata.samplePosition);

        if (msg.isNoteOnOrOff() || msg.isController() || msg.isPitchWheel())
        {
            clap_event_midi_t ev {};
            ev.header.size = sizeof(clap_event_midi_t);
            ev.header.time = sampleOffset;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = CLAP_EVENT_MIDI;
            ev.header.flags = 0;
            ev.port_index = 0;

            const auto rawData = msg.getRawData();
            const auto rawSize = msg.getRawDataSize();

            ev.data[0] = rawSize > 0 ? rawData[0] : 0;
            ev.data[1] = rawSize > 1 ? rawData[1] : 0;
            ev.data[2] = rawSize > 2 ? rawData[2] : 0;

            currentInputEvents.midiEvents.push_back(ev);
        }
    }

    currentInputEvents.eventPtrs.reserve(currentInputEvents.midiEvents.size());

    for (auto& ev : currentInputEvents.midiEvents)
        currentInputEvents.eventPtrs.push_back(&ev.header);

    emptyInputEvents.ctx = &currentInputEvents;
}

const juce::String ClapPluginWrapper::getName() const
{
    return pluginName;
}

bool ClapPluginWrapper::loadModule()
{
    if (!moduleFile.existsAsFile())
    {
        lastError = "CLAP file does not exist.";
        return false;
    }

   #if JUCE_WINDOWS
    libraryHandle = (void*) LoadLibraryW(moduleFile.getFullPathName().toWideCharPointer());

    if (libraryHandle == nullptr)
    {
        lastError = "Failed to load CLAP module.";
        return false;
    }

    entry = (const clap_plugin_entry*) GetProcAddress((HMODULE) libraryHandle, "clap_entry");

    if (entry == nullptr)
    {
        lastError = "clap_entry not found.";
        unloadModule();
        return false;
    }
   #else
    lastError = "Only Windows loading implemented so far.";
    return false;
   #endif

    if (entry == nullptr)
    {
        lastError = "Invalid CLAP entry.";
        unloadModule();
        return false;
    }

    if (!entry->init || !entry->init(moduleFile.getFullPathName().toRawUTF8()))
    {
        lastError = "CLAP entry init failed.";
        unloadModule();
        return false;
    }

    factory = (const clap_plugin_factory*) entry->get_factory(CLAP_PLUGIN_FACTORY_ID);

    if (factory == nullptr)
    {
        lastError = "CLAP plugin factory not found.";
        unloadModule();
        return false;
    }

    return true;
}

void ClapPluginWrapper::unloadModule()
{
    if (entry != nullptr && entry->deinit != nullptr)
        entry->deinit();

    entry = nullptr;
    factory = nullptr;
    descriptor = nullptr;

   #if JUCE_WINDOWS
    if (libraryHandle != nullptr)
    {
        FreeLibrary((HMODULE) libraryHandle);
        libraryHandle = nullptr;
    }
   #endif
}

bool ClapPluginWrapper::createPluginInstance()
{
    if (factory == nullptr)
    {
        lastError = "CLAP factory unavailable.";
        return false;
    }

    const auto count = factory->get_plugin_count(factory);

    if (count == 0)
    {
        lastError = "No CLAP plugins found in file.";
        return false;
    }

    descriptor = factory->get_plugin_descriptor(factory, 0);

    if (descriptor == nullptr || descriptor->id == nullptr)
    {
        lastError = "Invalid CLAP descriptor.";
        return false;
    }

    pluginName = juce::String(descriptor->name != nullptr ? descriptor->name : "CLAP Plugin");

    clap_host host
    {
        CLAP_VERSION,
        this,
        "PolyHost",
        "PolyHost",
        "https://example.com",
        "1.0.0",
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    plugin = factory->create_plugin(factory, &host, descriptor->id);

    if (plugin == nullptr)
    {
        lastError = "Failed to create CLAP plugin instance.";
        return false;
    }

    if (plugin->init != nullptr && !plugin->init(plugin))
    {
        lastError = "CLAP plugin init failed.";
        plugin->destroy(plugin);
        plugin = nullptr;
        return false;
    }

    inspectAudioPorts();

    guiExtension = static_cast<const clap_plugin_gui_t*>(
        plugin->get_extension(plugin, CLAP_EXT_GUI));

    stateExtension = static_cast<const clap_plugin_state_t*>(
        plugin->get_extension(plugin, CLAP_EXT_STATE));

    return true;
}

void ClapPluginWrapper::inspectAudioPorts()
{
    audioPorts = nullptr;
    mainInputChannelCount = 0;
    mainOutputChannelCount = 0;
    hasAudioInput = false;
    hasAudioOutput = false;
    isSimpleStereoProcessor = false;
    allowProcessing = false;

    if (plugin == nullptr || plugin->get_extension == nullptr)
        return;

    audioPorts = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));

    if (audioPorts == nullptr || audioPorts->count == nullptr || audioPorts->get == nullptr)
        return;

    const auto inputCount = audioPorts->count(plugin, true);
    const auto outputCount = audioPorts->count(plugin, false);

    hasAudioInput = inputCount > 0;
    hasAudioOutput = outputCount > 0;

    if (inputCount > 0)
    {
        clap_audio_port_info_t info {};
        if (audioPorts->get(plugin, 0, true, &info))
            mainInputChannelCount = info.channel_count;
    }

    if (outputCount > 0)
    {
        clap_audio_port_info_t info {};
        if (audioPorts->get(plugin, 0, false, &info))
            mainOutputChannelCount = info.channel_count;
    }

    const bool supportedEffect =
        (mainInputChannelCount == 1 || mainInputChannelCount == 2) &&
        (mainOutputChannelCount == 1 || mainOutputChannelCount == 2);

    const bool supportedGenerator =
        !hasAudioInput &&
        (mainOutputChannelCount == 1 || mainOutputChannelCount == 2);

    isSimpleStereoProcessor = supportedEffect || supportedGenerator;
    allowProcessing = supportedEffect || supportedGenerator;
}

void ClapPluginWrapper::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    steadyTime = 0;

    if (plugin == nullptr)
        return;

    if (plugin->activate != nullptr && !activated)
        activated = plugin->activate(plugin, sampleRate, (uint32_t) samplesPerBlock, (uint32_t) samplesPerBlock);

    if (activated && plugin->start_processing != nullptr)
        plugin->start_processing(plugin);
}

void ClapPluginWrapper::releaseResources()
{
    if (plugin == nullptr)
        return;

    if (plugin->stop_processing != nullptr && activated)
        plugin->stop_processing(plugin);

    if (plugin->deactivate != nullptr && activated)
        plugin->deactivate(plugin);

    activated = false;
    steadyTime = 0;
}

bool ClapPluginWrapper::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputSet = layouts.getMainInputChannelSet();
    const auto outputSet = layouts.getMainOutputChannelSet();

    const int inChannels = inputSet.size();
    const int outChannels = outputSet.size();

    const bool inputOk = (inChannels == 0 || inChannels == 1 || inChannels == 2);
    const bool outputOk = (outChannels == 0 || outChannels == 1 || outChannels == 2);

    return inputOk && outputOk;
}

void ClapPluginWrapper::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (!allowProcessing)
        return;

    if (plugin == nullptr || plugin->process == nullptr || !activated)
        return;

    const auto numSamples = (uint32_t) buffer.getNumSamples();
    const int hostChannels = buffer.getNumChannels();

    if (hostChannels <= 0)
        return;

    inputChannelPointers.clear();
    outputChannelPointers.clear();
    scratchBuffers.clear();

    auto makeScratchChannel = [this, numSamples]() -> float*
    {
        scratchBuffers.emplace_back(1, (int) numSamples);
        scratchBuffers.back().clear();
        return scratchBuffers.back().getWritePointer(0);
    };

    // -------------------------
    // Build input channel array
    // -------------------------
    if (hasAudioInput && mainInputChannelCount > 0)
    {
        inputChannelPointers.resize(mainInputChannelCount, nullptr);

        for (uint32_t ch = 0; ch < mainInputChannelCount; ++ch)
        {
            if ((int) ch < hostChannels)
            {
                inputChannelPointers[ch] = buffer.getWritePointer((int) ch);
            }
            else if (hostChannels == 1)
            {
                inputChannelPointers[ch] = buffer.getWritePointer(0); // duplicate mono
            }
            else
            {
                inputChannelPointers[ch] = makeScratchChannel(); // silence
            }
        }

        clapInputBuffer.data32 = inputChannelPointers.data();
        clapInputBuffer.channel_count = mainInputChannelCount;
    }
    else
    {
        clapInputBuffer.data32 = nullptr;
        clapInputBuffer.channel_count = 0;
    }

    clapInputBuffer.data64 = nullptr;
    clapInputBuffer.latency = 0;
    clapInputBuffer.constant_mask = 0;

    // --------------------------
    // Build output channel array
    // --------------------------
    if (hasAudioOutput && mainOutputChannelCount > 0)
    {
        outputChannelPointers.resize(mainOutputChannelCount, nullptr);

        for (uint32_t ch = 0; ch < mainOutputChannelCount; ++ch)
        {
            if ((int) ch < hostChannels)
            {
                outputChannelPointers[ch] = buffer.getWritePointer((int) ch);
            }
            else
            {
                outputChannelPointers[ch] = makeScratchChannel(); // discard extra outputs
            }
        }

        clapOutputBuffer.data32 = outputChannelPointers.data();
        clapOutputBuffer.channel_count = mainOutputChannelCount;
    }
    else
    {
        clapOutputBuffer.data32 = nullptr;
        clapOutputBuffer.channel_count = 0;
    }

    clapOutputBuffer.data64 = nullptr;
    clapOutputBuffer.latency = 0;
    clapOutputBuffer.constant_mask = 0;

    buildInputEventsFromMidi(midiMessages);

    clap_process_t process {};
    process.steady_time = steadyTime;
    process.frames_count = numSamples;
    process.transport = nullptr;
    process.audio_inputs = hasAudioInput ? &clapInputBuffer : nullptr;
    process.audio_outputs = hasAudioOutput ? &clapOutputBuffer : nullptr;
    process.audio_inputs_count = hasAudioInput ? 1u : 0u;
    process.audio_outputs_count = hasAudioOutput ? 1u : 0u;
    process.in_events = &emptyInputEvents;
    process.out_events = &emptyOutputEvents;

    plugin->process(plugin, &process);
    steadyTime += numSamples;

    // If plugin is mono out and host is stereo, copy left to right
    if (hasAudioOutput && mainOutputChannelCount == 1 && hostChannels > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, (int) numSamples);
}

void ClapPluginWrapper::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

juce::AudioProcessorEditor* ClapPluginWrapper::createEditor()
{
    class ClapEmbeddedEditor final : public juce::AudioProcessorEditor,
                                     private juce::Timer
    {
    public:
        explicit ClapEmbeddedEditor(ClapPluginWrapper& ownerIn)
            : juce::AudioProcessorEditor(ownerIn), owner(ownerIn)
        {
            setOpaque(true);

            owner.createEmbeddedEditorIfNeeded();
            owner.refreshEmbeddedGuiSize();

            auto size = owner.getEmbeddedGuiSize();
            const int w = juce::jmax(320, size.getWidth());
            const int h = juce::jmax(140, size.getHeight());
            setSize(w, h);

            DebugLog::write("ClapEmbeddedEditor - constructed for " + owner.getPluginName());
            startTimerHz(10);
        }

        ~ClapEmbeddedEditor() override
        {
            DebugLog::write("ClapEmbeddedEditor - destructor for " + owner.getPluginName());

            stopTimer();

            owner.setEmbeddedGuiVisible(false);

           #if JUCE_WINDOWS
            destroyHostWindow();
           #endif

            owner.destroyEmbeddedEditor();
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF1E1E2E));

            if (embedFailed)
            {
                auto area = getLocalBounds().reduced(16);

                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
                g.drawText(owner.getPluginName(), area.removeFromTop(28), juce::Justification::centredLeft);

                area.removeFromTop(8);

                g.setColour(juce::Colours::lightgrey);
                g.setFont(juce::Font(juce::FontOptions(14.0f)));
                g.drawFittedText("CLAP GUI embedding failed.\nPlugin loaded and audio is running.\nSee polyhost.log for details.",
                                 area,
                                 juce::Justification::centredLeft,
                                 4);
            }
        }

        void resized() override
        {
           #if JUCE_WINDOWS
            updateHostWindowBounds();
           #endif
        }

        void moved() override
        {
           #if JUCE_WINDOWS
            updateHostWindowBounds();
           #endif
        }

        void visibilityChanged() override
        {
            DebugLog::write("ClapEmbeddedEditor - visibilityChanged for " + owner.getPluginName()
                            + " isShowing=" + juce::String(isShowing() ? "true" : "false"));

            syncVisibility();
        }

        void parentHierarchyChanged() override
        {
        }

    private:
        void timerCallback() override
        {
            DebugLog::writeAdvanced("ClapEmbeddedEditor - timerCallback for " + owner.getPluginName()
                                    + " isShowing=" + juce::String(isShowing() ? "true" : "false")
                                    + " isAttached=" + juce::String(isAttached ? "true" : "false")
                                    + " embedFailed=" + juce::String(embedFailed ? "true" : "false"));

            if (!isAttached && !embedFailed)
            {
                auto* peer = getPeer();

                if (peer != nullptr && isShowing())
                {
                    DebugLog::write("ClapEmbeddedEditor - timer trying attach for " + owner.getPluginName());
                    tryInitialAttach();
                }
            }

            if (isAttached)
            {
                updateHostWindowBounds();
                syncVisibility();
            }
        }

        void syncVisibility()
        {
           #if JUCE_WINDOWS
            if (hostWindow != nullptr)
                ShowWindow(hostWindow, isShowing() ? SW_SHOW : SW_HIDE);
           #endif

            if (!isAttached)
                return;

            owner.setEmbeddedGuiVisible(isShowing());
        }

       #if JUCE_WINDOWS
        static LRESULT CALLBACK HostWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            switch (msg)
            {
                case WM_ERASEBKGND:
                    return 1;

                case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    auto hdc = BeginPaint(hwnd, &ps);

                    RECT rc {};
                    GetClientRect(hwnd, &rc);

                    HBRUSH brush = CreateSolidBrush(RGB(30, 30, 46));
                    FillRect(hdc, &rc, brush);
                    DeleteObject(brush);

                    EndPaint(hwnd, &ps);
                    return 0;
                }
            }

            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        void ensureHostWindow()
        {
            if (hostWindow != nullptr)
                return;

            auto* peer = getPeer();
            if (peer == nullptr)
            {
                DebugLog::write("ClapEmbeddedEditor - ensureHostWindow: peer was null");
                return;
            }

            auto parentHandle = (HWND) peer->getNativeHandle();
            if (parentHandle == nullptr)
            {
                DebugLog::write("ClapEmbeddedEditor - ensureHostWindow: parent handle was null");
                return;
            }

            static bool classRegistered = false;

            if (!classRegistered)
            {
                WNDCLASSW wc {};
                wc.lpfnWndProc = HostWindowProc;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = L"PolyHostClapEmbedWindow";
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                wc.hbrBackground = nullptr;

                RegisterClassW(&wc);
                classRegistered = true;
            }

            hostWindow = CreateWindowExW(
                0,
                L"PolyHostClapEmbedWindow",
                L"",
                WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0, 1, 1,
                parentHandle,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);

            if (hostWindow == nullptr)
                DebugLog::write("ClapEmbeddedEditor - CreateWindowExW failed");
            else
                ShowWindow(hostWindow, SW_HIDE);
        }

        void updateHostWindowBounds()
        {
            if (hostWindow == nullptr)
                return;

            auto* peer = getPeer();
            if (peer == nullptr)
                return;

            const juce::Point<int> topLeftOnScreen = localPointToGlobal(juce::Point<int>(0, 0));
            const juce::Point<int> peerTopLeftOnScreen = peer->localToGlobal(juce::Point<int>(0, 0));
            const juce::Point<int> offset = topLeftOnScreen - peerTopLeftOnScreen;

            SetWindowPos(hostWindow,
                         nullptr,
                         offset.x,
                         offset.y,
                         getWidth(),
                         getHeight(),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        void destroyHostWindow()
        {
            if (hostWindow != nullptr)
            {
                DestroyWindow(hostWindow);
                hostWindow = nullptr;
            }
        }
       #endif

        void tryInitialAttach()
        {
           #if JUCE_WINDOWS
            if (isAttached || embedFailed)
                return;

            auto* peer = getPeer();
            if (peer == nullptr)
                return;

            ensureHostWindow();

            if (hostWindow == nullptr)
                return;

            owner.refreshEmbeddedGuiSize();

            auto guiSize = owner.getEmbeddedGuiSize();
            if (guiSize.getWidth() > 0 && guiSize.getHeight() > 0)
                setSize(guiSize.getWidth(), guiSize.getHeight());

            updateHostWindowBounds();

            DebugLog::write("ClapEmbeddedEditor - trying attach for " + owner.getPluginName());

            if (!owner.attachGuiToHostWindow(hostWindow))
            {
                embedFailed = true;
                DebugLog::write("ClapEmbeddedEditor - attachGuiToHostWindow returned false for " + owner.getPluginName());
                repaint();
                stopTimer();
                return;
            }

            isAttached = true;
            DebugLog::write("ClapEmbeddedEditor - attach succeeded for " + owner.getPluginName());
            syncVisibility();
           #endif
        }

        ClapPluginWrapper& owner;
        bool isAttached = false;
        bool embedFailed = false;

       #if JUCE_WINDOWS
        HWND hostWindow = nullptr;
       #endif
    };

    return new ClapEmbeddedEditor(*this);
}

bool ClapPluginWrapper::hasEditor() const
{
    return supportsEmbeddedGui();
}

const juce::String ClapPluginWrapper::getInputChannelName(int channelIndex) const
{
    return juce::String(channelIndex + 1);
}

const juce::String ClapPluginWrapper::getOutputChannelName(int channelIndex) const
{
    return juce::String(channelIndex + 1);
}

bool ClapPluginWrapper::isInputChannelStereoPair(int) const
{
    return true;
}

bool ClapPluginWrapper::isOutputChannelStereoPair(int) const
{
    return true;
}

bool ClapPluginWrapper::acceptsMidi() const
{
    return true;
}

bool ClapPluginWrapper::producesMidi() const
{
    return false;
}

bool ClapPluginWrapper::isMidiEffect() const
{
    return false;
}

double ClapPluginWrapper::getTailLengthSeconds() const
{
    return 0.0;
}

int ClapPluginWrapper::getNumPrograms()
{
    return 1;
}

int ClapPluginWrapper::getCurrentProgram()
{
    return 0;
}

void ClapPluginWrapper::setCurrentProgram(int)
{
}

const juce::String ClapPluginWrapper::getProgramName(int)
{
    return {};
}

void ClapPluginWrapper::changeProgramName(int, const juce::String&)
{
}

namespace
{
    struct MemoryOutputStreamContext
    {
        juce::MemoryBlock* block = nullptr;
    };

    struct MemoryInputStreamContext
    {
        const char* data = nullptr;
        uint64_t size = 0;
        uint64_t position = 0;
    };

    int64_t CLAP_ABI writeToMemoryStream(const clap_ostream* stream, const void* buffer, uint64_t size)
    {
        if (stream == nullptr || stream->ctx == nullptr || buffer == nullptr)
            return -1;

        auto* ctx = static_cast<MemoryOutputStreamContext*>(stream->ctx);
        if (ctx->block == nullptr)
            return -1;

        const auto oldSize = ctx->block->getSize();
        ctx->block->append(buffer, (size_t) size);
        return (int64_t) (ctx->block->getSize() - oldSize);
    }

    int64_t CLAP_ABI readFromMemoryStream(const clap_istream* stream, void* buffer, uint64_t size)
    {
        if (stream == nullptr || stream->ctx == nullptr || buffer == nullptr)
            return -1;

        auto* ctx = static_cast<MemoryInputStreamContext*>(stream->ctx);

        if (ctx->position >= ctx->size)
            return 0;

        const auto remaining = ctx->size - ctx->position;
        const auto bytesToRead = juce::jmin<uint64_t>(size, remaining);

        std::memcpy(buffer, ctx->data + ctx->position, (size_t) bytesToRead);
        ctx->position += bytesToRead;
        return (int64_t) bytesToRead;
    }
}

void ClapPluginWrapper::getStateInformation(juce::MemoryBlock& destData)
{
    destData.reset();

    if (plugin == nullptr || stateExtension == nullptr || stateExtension->save == nullptr)
        return;

    MemoryOutputStreamContext ctx;
    ctx.block = &destData;

    clap_ostream stream {};
    stream.ctx = &ctx;
    stream.write = &writeToMemoryStream;

    if (!stateExtension->save(plugin, &stream))
        destData.reset();
}

void ClapPluginWrapper::setStateInformation(const void* data, int sizeInBytes)
{
    lastStateLoadSucceeded = false;

    if (plugin == nullptr || stateExtension == nullptr || stateExtension->load == nullptr)
        return;

    if (data == nullptr || sizeInBytes <= 0)
        return;

    const bool wasActivated = activated;

    if (wasActivated)
    {
        if (plugin->stop_processing != nullptr)
            plugin->stop_processing(plugin);

        if (plugin->deactivate != nullptr)
            plugin->deactivate(plugin);

        activated = false;
    }

    MemoryInputStreamContext ctx;
    ctx.data = static_cast<const char*>(data);
    ctx.size = (uint64_t) sizeInBytes;
    ctx.position = 0;

    clap_istream stream {};
    stream.ctx = &ctx;
    stream.read = &readFromMemoryStream;

    const bool loaded = stateExtension->load(plugin, &stream);
    lastStateLoadSucceeded = loaded;

    if (wasActivated)
    {
        if (plugin->activate != nullptr)
            activated = plugin->activate(plugin,
                                         currentSampleRate,
                                         (uint32_t) currentBlockSize,
                                         (uint32_t) currentBlockSize);

        if (activated && plugin->start_processing != nullptr)
            plugin->start_processing(plugin);
    }

    if (!loaded)
        lastError = "CLAP state load failed.";
}

void ClapPluginWrapper::fillInPluginDescription(juce::PluginDescription& description) const
{
    description = {};

    description.name = pluginName;
    description.descriptiveName = pluginName;
    description.pluginFormatName = "CLAP";
    description.fileOrIdentifier = moduleFile.getFullPathName();
    description.uniqueId = 0;
    description.isInstrument = isSynthLikePlugin();
    description.numInputChannels = (int) mainInputChannelCount;
    description.numOutputChannels = (int) mainOutputChannelCount;
}