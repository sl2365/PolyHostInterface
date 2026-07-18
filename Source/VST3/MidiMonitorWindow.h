#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AppSettings.h"

struct VstMidiMonitorEntry
{
    juce::String time;
    juce::String device;
    juce::String type;
    juce::String channel;
    juce::String data1;
    juce::String data2;
    juce::String description;
    juce::String rawHex;
    juce::Colour colour;
    int rawStatus = -1;
    int filterFlags = 0;
};

class VstMidiMonitorComponent : public juce::Component,
                                private juce::Timer,
                                private juce::TableListBoxModel,
                                private juce::Button::Listener
{
public:
    explicit VstMidiMonitorComponent(PolyHostPluginProcessor& processorIn, AppSettings& settingsIn)
        : processor(processorIn),
          settings(settingsIn)
    {
        addAndMakeVisible(clearButton);
        clearButton.setButtonText("Clear");
        clearButton.addListener(this);

        addAndMakeVisible(pauseButton);
        pauseButton.setButtonText("Pause");
        pauseButton.setClickingTogglesState(true);
        pauseButton.addListener(this);

        addAndMakeVisible(freezeButton);
        freezeButton.setButtonText("Freeze");
        freezeButton.setClickingTogglesState(true);
        freezeButton.addListener(this);

        addAndMakeVisible(copyRowButton);
        copyRowButton.setButtonText("Copy Row");
        copyRowButton.addListener(this);

        addAndMakeVisible(copyAllButton);
        copyAllButton.setButtonText("Copy All");
        copyAllButton.addListener(this);

        addAndMakeVisible(exportButton);
        exportButton.setButtonText("Export");
        exportButton.addListener(this);

        addAndMakeVisible(hideClockButton);
        hideClockButton.setButtonText("Hide Clock");
        hideClockButton.setClickingTogglesState(true);
        hideClockButton.addListener(this);

        addAndMakeVisible(hideActiveSenseButton);
        hideActiveSenseButton.setButtonText("Hide Active Sense");
        hideActiveSenseButton.setClickingTogglesState(true);
        hideActiveSenseButton.addListener(this);

        addAndMakeVisible(showNoteButton);
        showNoteButton.setButtonText("Note");
        showNoteButton.setClickingTogglesState(true);
        showNoteButton.addListener(this);

        addAndMakeVisible(showPitchBendButton);
        showPitchBendButton.setButtonText("Pitch Bend");
        showPitchBendButton.setClickingTogglesState(true);
        showPitchBendButton.addListener(this);

        addAndMakeVisible(showCcButton);
        showCcButton.setButtonText("CC");
        showCcButton.setClickingTogglesState(true);
        showCcButton.addListener(this);

        addAndMakeVisible(showNrpnRpnButton);
        showNrpnRpnButton.setButtonText("NRPN/RPN");
        showNrpnRpnButton.setClickingTogglesState(true);
        showNrpnRpnButton.addListener(this);

        addAndMakeVisible(showProgramChangeButton);
        showProgramChangeButton.setButtonText("Program Change");
        showProgramChangeButton.setClickingTogglesState(true);
        showProgramChangeButton.addListener(this);

        addAndMakeVisible(showAftertouchButton);
        showAftertouchButton.setButtonText("Aftertouch / Ch Pressure");
        showAftertouchButton.setClickingTogglesState(true);
        showAftertouchButton.addListener(this);

        addAndMakeVisible(showSysExButton);
        showSysExButton.setButtonText("SysEx");
        showSysExButton.setClickingTogglesState(true);
        showSysExButton.addListener(this);

        addAndMakeVisible(showRealtimeButton);
        showRealtimeButton.setButtonText("Realtime / Transport");
        showRealtimeButton.setClickingTogglesState(true);
        showRealtimeButton.addListener(this);

        addAndMakeVisible(showSystemCommonButton);
        showSystemCommonButton.setButtonText("System Common");
        showSystemCommonButton.setClickingTogglesState(true);
        showSystemCommonButton.addListener(this);

        addAndMakeVisible(table);
        table.setModel(this);
        table.setMultipleSelectionEnabled(false);

        table.getHeader().addColumn("Time",        1, settings.getMidiMonitorColumnWidth(1, 80));
        table.getHeader().addColumn("Source",      2, settings.getMidiMonitorColumnWidth(2, 150));
        table.getHeader().addColumn("Type",        3, settings.getMidiMonitorColumnWidth(3, 110));
        table.getHeader().addColumn("Ch",          4, settings.getMidiMonitorColumnWidth(4, 40));
        table.getHeader().addColumn("Data 1",      5, settings.getMidiMonitorColumnWidth(5, 60));
        table.getHeader().addColumn("Data 2",      6, settings.getMidiMonitorColumnWidth(6, 60));
        table.getHeader().addColumn("Description", 7, settings.getMidiMonitorColumnWidth(7, 260));
        table.getHeader().addColumn("Raw Hex",     8, settings.getMidiMonitorColumnWidth(8, 145));

        table.setColour(juce::ListBox::backgroundColourId, juce::Colours::black);
        table.setRowHeight(22);

        loadSettings();
        updateFilterButtonColours();
        updateCaptureButtonColours();

        startTimerHz(20);
    }

    void saveSettings(int windowWidth, int windowHeight)
    {
        settings.setMidiMonitorWindowSize(windowWidth, windowHeight);

        settings.setMidiMonitorFilterSettings(hideClockButton.getToggleState(),
                                              hideActiveSenseButton.getToggleState(),
                                              showNoteButton.getToggleState(),
                                              showPitchBendButton.getToggleState(),
                                              showCcButton.getToggleState(),
                                              showNrpnRpnButton.getToggleState(),
                                              showProgramChangeButton.getToggleState(),
                                              showAftertouchButton.getToggleState(),
                                              showSysExButton.getToggleState(),
                                              showRealtimeButton.getToggleState(),
                                              showSystemCommonButton.getToggleState());

        settings.setMidiMonitorColumnWidths(table.getHeader().getColumnWidth(1),
                                            table.getHeader().getColumnWidth(2),
                                            table.getHeader().getColumnWidth(3),
                                            table.getHeader().getColumnWidth(4),
                                            table.getHeader().getColumnWidth(5),
                                            table.getHeader().getColumnWidth(6),
                                            table.getHeader().getColumnWidth(7),
                                            table.getHeader().getColumnWidth(8));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto topBar = area.removeFromTop(70);

        auto actionColumn = topBar.removeFromLeft(78);
        auto actionRow1 = actionColumn.removeFromTop(22);
        actionColumn.removeFromTop(2);
        auto actionRow2 = actionColumn.removeFromTop(22);
        actionColumn.removeFromTop(2);
        auto actionRow3 = actionColumn.removeFromTop(22);

        copyRowButton.setBounds(actionRow1);
        copyAllButton.setBounds(actionRow2);
        exportButton.setBounds(actionRow3);

        topBar.removeFromLeft(8);

        auto captureColumn = topBar.removeFromLeft(70);
        auto captureRow1 = captureColumn.removeFromTop(22);
        captureColumn.removeFromTop(2);
        auto captureRow2 = captureColumn.removeFromTop(22);
        captureColumn.removeFromTop(2);
        auto captureRow3 = captureColumn.removeFromTop(22);

        clearButton.setBounds(captureRow1);
        pauseButton.setBounds(captureRow2);
        freezeButton.setBounds(captureRow3);

        topBar.removeFromLeft(8);

        auto systemFilterColumn = topBar.removeFromLeft(150);
        auto systemUpperRow = systemFilterColumn.removeFromTop(22);
        systemFilterColumn.removeFromTop(2);
        auto systemLowerRow = systemFilterColumn.removeFromTop(22);

        hideClockButton.setBounds(systemUpperRow.removeFromLeft(130));
        hideActiveSenseButton.setBounds(systemLowerRow.removeFromLeft(150));

        topBar.removeFromLeft(8);

        auto typeFilterColumnA = topBar.removeFromLeft(125);
        auto typeAUpperRow = typeFilterColumnA.removeFromTop(22);
        typeFilterColumnA.removeFromTop(2);
        auto typeAMiddleRow = typeFilterColumnA.removeFromTop(22);
        typeFilterColumnA.removeFromTop(2);
        auto typeALowerRow = typeFilterColumnA.removeFromTop(22);

        showNoteButton.setBounds(typeAUpperRow.removeFromLeft(90));
        showPitchBendButton.setBounds(typeAMiddleRow.removeFromLeft(120));
        showProgramChangeButton.setBounds(typeALowerRow.removeFromLeft(125));

        topBar.removeFromLeft(8);

        auto typeFilterColumnB = topBar.removeFromLeft(160);
        auto typeBUpperRow = typeFilterColumnB.removeFromTop(22);
        typeFilterColumnB.removeFromTop(2);
        auto typeBMiddleRow = typeFilterColumnB.removeFromTop(22);
        typeFilterColumnB.removeFromTop(2);
        auto typeBLowerRow = typeFilterColumnB.removeFromTop(22);

        showCcButton.setBounds(typeBUpperRow.removeFromLeft(90));
        showNrpnRpnButton.setBounds(typeBMiddleRow.removeFromLeft(120));
        showAftertouchButton.setBounds(typeBLowerRow.removeFromLeft(160));

        topBar.removeFromLeft(8);

        auto typeFilterColumnC = topBar.removeFromLeft(160);
        auto typeCUpperRow = typeFilterColumnC.removeFromTop(22);
        typeFilterColumnC.removeFromTop(2);
        auto typeCMiddleRow = typeFilterColumnC.removeFromTop(22);
        typeFilterColumnC.removeFromTop(2);
        auto typeCLowerRow = typeFilterColumnC.removeFromTop(22);

        showSysExButton.setBounds(typeCUpperRow.removeFromLeft(90));
        showRealtimeButton.setBounds(typeCMiddleRow.removeFromLeft(160));
        showSystemCommonButton.setBounds(typeCLowerRow.removeFromLeft(140));

        area.removeFromTop(6);

        table.setBounds(area);
    }

private:
    enum MidiFilterFlags
    {
        filterNote = 1 << 0,
        filterPitchBend = 1 << 1,
        filterCc = 1 << 2,
        filterNrpnRpn = 1 << 3,
        filterProgramChange = 1 << 4,
        filterAftertouch = 1 << 5,
        filterSysEx = 1 << 6,
        filterRealtimeTransport = 1 << 7,
        filterSystemCommon = 1 << 8
    };

    int getNumRows() override
    {
        return entries.size();
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int, int, bool selected) override
    {
        if (selected)
        {
            g.fillAll(juce::Colours::darkblue);
            return;
        }

        if (juce::isPositiveAndBelow(rowNumber, entries.size()))
            g.fillAll(entries[rowNumber].colour.withAlpha(0.18f));
        else
            g.fillAll(juce::Colours::black);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override
    {
        if (!juce::isPositiveAndBelow(rowNumber, entries.size()))
            return;

        const auto& e = entries.getReference(rowNumber);

        juce::String text;
        switch (columnId)
        {
            case 1: text = e.time; break;
            case 2: text = e.device; break;
            case 3: text = e.type; break;
            case 4: text = e.channel; break;
            case 5: text = e.data1; break;
            case 6: text = e.data2; break;
            case 7: text = e.description; break;
            case 8: text = e.rawHex; break;
            default: break;
        }

        g.setColour(rowIsSelected ? juce::Colours::white
                                  : e.colour.brighter(0.2f));
        g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);

        g.setColour(juce::Colours::grey);
        g.fillRect(width - 1, 0, 1, height);
    }

    void timerCallback() override
    {
        auto messages = processor.popPendingMidiMonitorEvents();

        if (pauseButton.getToggleState())
            return;

        if (messages.isEmpty())
            return;

        bool addedAny = false;

        for (auto& incoming : messages)
        {
            if (! incoming.valid)
                continue;

            const auto& message = incoming.message;

            int rawStatus = -1;

            if (message.getRawDataSize() > 0)
            {
                rawStatus =
                    static_cast<int>(
                        static_cast<unsigned char>(
                            message.getRawData()[0]));
            }

            if (hideClockButton.getToggleState()
                && rawStatus == 0xF8)
            {
                continue;
            }

            if (hideActiveSenseButton.getToggleState()
                && rawStatus == 0xFE)
            {
                continue;
            }

            allEntries.add(makeEntry(incoming));
            addedAny = true;
        }

        if (! addedAny)
            return;

        if (allEntries.size() > 1000)
            allEntries.removeRange(0, allEntries.size() - 1000);

        if (freezeButton.getToggleState())
            return;

        autoFollow = isScrolledToBottom();

        rebuildVisibleEntries();

        table.updateContent();

        if (autoFollow && entries.size() > 0)
            table.scrollToEnsureRowIsOnscreen(entries.size() - 1);

        repaint();
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == &clearButton)
        {
            allEntries.clear();
            entries.clear();
            table.updateContent();
            repaint();
            return;
        }

        if (button == &copyRowButton)
        {
            copySelectedRow();
            return;
        }

        if (button == &copyAllButton)
        {
            copyAllVisibleRows();
            return;
        }

        if (button == &exportButton)
        {
            exportVisibleRows();
            return;
        }

        if (button == &pauseButton || button == &freezeButton)
        {
            updateCaptureButtonColours();

            if (button == &freezeButton && ! freezeButton.getToggleState())
            {
                autoFollow = isScrolledToBottom();

                rebuildVisibleEntries();
                table.updateContent();

                if (autoFollow && entries.size() > 0)
                    table.scrollToEnsureRowIsOnscreen(entries.size() - 1);
            }

            repaint();
            return;
        }

        updateFilterButtonColours();

        autoFollow = isScrolledToBottom();

        rebuildVisibleEntries();
        table.updateContent();

        if (autoFollow && entries.size() > 0)
            table.scrollToEnsureRowIsOnscreen(entries.size() - 1);

        repaint();
    }

    void loadSettings()
    {
        hideClockButton.setToggleState(settings.getMidiMonitorHideClock(), juce::dontSendNotification);
        hideActiveSenseButton.setToggleState(settings.getMidiMonitorHideActiveSense(), juce::dontSendNotification);

        showNoteButton.setToggleState(settings.getMidiMonitorShowNote(), juce::dontSendNotification);
        showPitchBendButton.setToggleState(settings.getMidiMonitorShowPitchBend(), juce::dontSendNotification);
        showCcButton.setToggleState(settings.getMidiMonitorShowCc(), juce::dontSendNotification);
        showNrpnRpnButton.setToggleState(settings.getMidiMonitorShowNrpnRpn(), juce::dontSendNotification);
        showProgramChangeButton.setToggleState(settings.getMidiMonitorShowProgramChange(), juce::dontSendNotification);
        showAftertouchButton.setToggleState(settings.getMidiMonitorShowAftertouch(), juce::dontSendNotification);
        showSysExButton.setToggleState(settings.getMidiMonitorShowSysEx(), juce::dontSendNotification);
        showRealtimeButton.setToggleState(settings.getMidiMonitorShowRealtime(), juce::dontSendNotification);
        showSystemCommonButton.setToggleState(settings.getMidiMonitorShowSystemCommon(), juce::dontSendNotification);
    }

    static void updateFilterButtonColour(juce::ToggleButton& button)
    {
        const auto activeColour = juce::Colour(0xFF56D364);
        const auto inactiveColour = juce::Colours::white;

        const auto colour = button.getToggleState() ? activeColour : inactiveColour;

        button.setColour(juce::ToggleButton::textColourId, colour);
        button.setColour(juce::ToggleButton::tickColourId, activeColour);
    }

    void updateFilterButtonColours()
    {
        updateFilterButtonColour(hideClockButton);
        updateFilterButtonColour(hideActiveSenseButton);

        updateFilterButtonColour(showNoteButton);
        updateFilterButtonColour(showPitchBendButton);
        updateFilterButtonColour(showCcButton);
        updateFilterButtonColour(showNrpnRpnButton);
        updateFilterButtonColour(showProgramChangeButton);
        updateFilterButtonColour(showAftertouchButton);
        updateFilterButtonColour(showSysExButton);
        updateFilterButtonColour(showRealtimeButton);
        updateFilterButtonColour(showSystemCommonButton);
    }

    void updateCaptureButtonColours()
    {
        const auto inactiveText = juce::Colours::white;
        const auto pauseActive = juce::Colour(0xFFFFA657);
        const auto freezeActive = juce::Colour(0xFF58A6FF);

        pauseButton.setColour(juce::ToggleButton::textColourId,
                              pauseButton.getToggleState() ? pauseActive : inactiveText);
        pauseButton.setColour(juce::ToggleButton::tickColourId, pauseActive);

        freezeButton.setColour(juce::ToggleButton::textColourId,
                               freezeButton.getToggleState() ? freezeActive : inactiveText);
        freezeButton.setColour(juce::ToggleButton::tickColourId, freezeActive);
    }

    static juce::String makeRawHexString(const juce::MidiMessage& msg)
    {
        static constexpr const char* hex = "0123456789ABCDEF";

        juce::String text;
        const auto* data = msg.getRawData();
        const int size = msg.getRawDataSize();

        for (int i = 0; i < size; ++i)
        {
            if (i > 0)
                text << " ";

            const int value = (int) (unsigned char) data[i];
            text << juce::String::charToString(hex[(value >> 4) & 0x0f]);
            text << juce::String::charToString(hex[value & 0x0f]);
        }

        return text;
    }

    static juce::String makeTsvLine(const VstMidiMonitorEntry& e)
    {
        juce::String text;
        text << e.time << "\t"
             << e.device << "\t"
             << e.type << "\t"
             << e.channel << "\t"
             << e.data1 << "\t"
             << e.data2 << "\t"
             << e.description << "\t"
             << e.rawHex;
        return text;
    }

    static juce::String csvEscape(juce::String text)
    {
        const bool needsQuotes = text.containsChar(',')
                              || text.containsChar('"')
                              || text.containsChar('\n')
                              || text.containsChar('\r');

        text = text.replace("\"", "\"\"");

        if (needsQuotes)
            return "\"" + text + "\"";

        return text;
    }

    static juce::String makeCsvLine(const VstMidiMonitorEntry& e)
    {
        juce::String text;
        text << csvEscape(e.time) << ","
             << csvEscape(e.device) << ","
             << csvEscape(e.type) << ","
             << csvEscape(e.channel) << ","
             << csvEscape(e.data1) << ","
             << csvEscape(e.data2) << ","
             << csvEscape(e.description) << ","
             << csvEscape(e.rawHex);
        return text;
    }

    static juce::String getTsvHeader()
    {
        return "Time\tSource\tType\tCh\tData 1\tData 2\tDescription\tRaw Hex";
    }

    static juce::String getCsvHeader()
    {
        return "Time,Source,Type,Ch,Data 1,Data 2,Description,Raw Hex";
    }

    juce::String buildVisibleRowsAsTsv(bool includeHeader) const
    {
        juce::String text;

        if (includeHeader)
            text << getTsvHeader() << "\n";

        for (const auto& entry : entries)
            text << makeTsvLine(entry) << "\n";

        return text.trimEnd();
    }

    juce::String buildVisibleRowsAsCsv() const
    {
        juce::String text;
        text << getCsvHeader() << "\n";

        for (const auto& entry : entries)
            text << makeCsvLine(entry) << "\n";

        return text.trimEnd();
    }

    void copySelectedRow()
    {
        const int row = table.getSelectedRow();

        if (! juce::isPositiveAndBelow(row, entries.size()))
            return;

        juce::SystemClipboard::copyTextToClipboard(makeTsvLine(entries.getReference(row)));
    }

    void copyAllVisibleRows()
    {
        juce::SystemClipboard::copyTextToClipboard(buildVisibleRowsAsTsv(true));
    }

    void exportVisibleRows()
    {
        auto defaultFile = AppSettings::getAppDirectory().getChildFile("midi-monitor-log.csv");

        juce::FileChooser chooser("Export visible MIDI monitor rows",
                                  defaultFile,
                                  "*.csv;*.txt",
                                  true);

        if (! chooser.browseForFileToSave(true))
            return;

        auto file = chooser.getResult();

        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension(".csv");

        const bool exportAsText = file.getFileExtension().equalsIgnoreCase(".txt");

        const auto text = exportAsText ? buildVisibleRowsAsTsv(true)
                                       : buildVisibleRowsAsCsv();

        file.replaceWithText(text);
    }

    bool anyTypeFilterEnabled() const
    {
        return showNoteButton.getToggleState()
            || showPitchBendButton.getToggleState()
            || showCcButton.getToggleState()
            || showNrpnRpnButton.getToggleState()
            || showProgramChangeButton.getToggleState()
            || showAftertouchButton.getToggleState()
            || showSysExButton.getToggleState()
            || showRealtimeButton.getToggleState()
            || showSystemCommonButton.getToggleState();
    }

    bool entryPassesCurrentFilters(const VstMidiMonitorEntry& entry) const
    {
        if (hideClockButton.getToggleState() && entry.rawStatus == 0xF8)
            return false;

        if (hideActiveSenseButton.getToggleState() && entry.rawStatus == 0xFE)
            return false;

        if (! anyTypeFilterEnabled())
            return true;

        if (showNoteButton.getToggleState()
            && ((entry.filterFlags & filterNote) != 0))
            return true;

        if (showPitchBendButton.getToggleState()
            && ((entry.filterFlags & filterPitchBend) != 0))
            return true;

        if (showCcButton.getToggleState()
            && ((entry.filterFlags & filterCc) != 0))
            return true;

        if (showNrpnRpnButton.getToggleState()
            && ((entry.filterFlags & filterNrpnRpn) != 0))
            return true;

        if (showProgramChangeButton.getToggleState()
            && ((entry.filterFlags & filterProgramChange) != 0))
            return true;

        if (showAftertouchButton.getToggleState()
            && ((entry.filterFlags & filterAftertouch) != 0))
            return true;

        if (showSysExButton.getToggleState()
            && ((entry.filterFlags & filterSysEx) != 0))
            return true;

        if (showRealtimeButton.getToggleState()
            && ((entry.filterFlags & filterRealtimeTransport) != 0))
            return true;

        if (showSystemCommonButton.getToggleState()
            && ((entry.filterFlags & filterSystemCommon) != 0))
            return true;

        return false;
    }

    void rebuildVisibleEntries()
    {
        entries.clear();

        for (const auto& entry : allEntries)
        {
            if (entryPassesCurrentFilters(entry))
                entries.add(entry);
        }
    }

    static bool isNrpnRpnController(int controllerNumber)
    {
        switch (controllerNumber)
        {
            case 6:   // Data Entry MSB
            case 38:  // Data Entry LSB
            case 96:  // Data Increment
            case 97:  // Data Decrement
            case 98:  // NRPN LSB
            case 99:  // NRPN MSB
            case 100: // RPN LSB
            case 101: // RPN MSB
                return true;

            default:
                return false;
        }
    }

    VstMidiMonitorEntry makeEntry(const PolyHostPluginProcessor::MidiMonitorEvent& incoming)
    {
        VstMidiMonitorEntry e;

        auto now = juce::Time::getCurrentTime();
        e.time = now.formatted("%H:%M:%S");
        e.device = incoming.sourceName;

        const auto& msg = incoming.message;
        e.rawHex = makeRawHexString(msg);

        if (msg.getRawDataSize() > 0)
            e.rawStatus = (int) (unsigned char) msg.getRawData()[0];

        if (msg.isNoteOn())
        {
            e.filterFlags = filterNote;
            e.type = "Note On";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String((int) msg.getVelocity());
            e.description = msg.getDescription();
            e.colour = juce::Colours::green;
        }
        else if (msg.isNoteOff())
        {
            e.filterFlags = filterNote;
            e.type = "Note Off";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String((int) msg.getVelocity());
            e.description = msg.getDescription();
            e.colour = juce::Colours::red;
        }
        else if (msg.isController())
        {
            const int controllerNumber = msg.getControllerNumber();

            e.filterFlags = isNrpnRpnController(controllerNumber) ? filterNrpnRpn : filterCc;
            e.type = isNrpnRpnController(controllerNumber) ? "NRPN/RPN" : "CC";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(controllerNumber);
            e.data2 = juce::String(msg.getControllerValue());
            e.description = msg.getDescription();
            e.colour = isNrpnRpnController(controllerNumber) ? juce::Colours::orange
                                                             : juce::Colours::yellow;
        }
        else if (msg.isPitchWheel())
        {
            e.filterFlags = filterPitchBend;
            e.type = "Pitch Bend";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getPitchWheelValue());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::cyan;
        }
        else if (msg.isAftertouch())
        {
            e.filterFlags = filterAftertouch;
            e.type = "Aftertouch";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String(msg.getAfterTouchValue());
            e.description = msg.getDescription();
            e.colour = juce::Colours::orange;
        }
        else if (msg.isChannelPressure())
        {
            e.filterFlags = filterAftertouch;
            e.type = "Ch Pressure";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getChannelPressureValue());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::orange;
        }
        else if (msg.isProgramChange())
        {
            e.filterFlags = filterProgramChange;
            e.type = "Program";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getProgramChangeNumber());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::purple;
        }
        else if (msg.isSysEx())
        {
            e.filterFlags = filterSysEx;
            e.type = "SysEx";
            e.channel = "-";
            e.data1 = juce::String((int) msg.getSysExDataSize()) + " bytes";
            e.data2 = "-";
            e.description = "System Exclusive";
            e.colour = juce::Colours::grey;
        }
        else if (msg.getRawDataSize() > 0)
        {
            const auto status = (unsigned char) msg.getRawData()[0];

            switch (status)
            {
                case 0xF1: e.filterFlags = filterSystemCommon;      e.type = "System";   e.description = "MIDI Time Code Quarter Frame"; e.colour = juce::Colours::lightblue; break;
                case 0xF2: e.filterFlags = filterSystemCommon;      e.type = "System";   e.description = "Song Position Pointer";        e.colour = juce::Colours::lightblue; break;
                case 0xF3: e.filterFlags = filterSystemCommon;      e.type = "System";   e.description = "Song Select";                  e.colour = juce::Colours::lightblue; break;
                case 0xF6: e.filterFlags = filterSystemCommon;      e.type = "System";   e.description = "Tune Request";                 e.colour = juce::Colours::lightblue; break;
                case 0xF8: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "MIDI Clock";                   e.colour = juce::Colours::orange; break;
                case 0xFA: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "Start";                        e.colour = juce::Colours::green; break;
                case 0xFB: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "Continue";                     e.colour = juce::Colours::greenyellow; break;
                case 0xFC: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "Stop";                         e.colour = juce::Colours::red; break;
                case 0xFE: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "Active Sensing";               e.colour = juce::Colours::yellow; break;
                case 0xFF: e.filterFlags = filterRealtimeTransport; e.type = "Realtime"; e.description = "System Reset";                 e.colour = juce::Colours::red; break;
                default:   e.type = "Other";                        e.description = msg.getDescription();                                e.colour = juce::Colours::white; break;
            }

            e.channel = "-";
            e.data1 = "-";
            e.data2 = "-";
        }
        else
        {
            e.type = "Other";
            e.channel = "-";
            e.data1 = "-";
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::white;
        }

        return e;
    }

    bool isScrolledToBottom() const
    {
        if (auto* viewport = table.getViewport())
        {
            if (!viewport->isVerticalScrollBarShown())
                return true;

            const int maxY = juce::jmax(0,
                                        viewport->getViewedComponent()->getHeight()
                                            - viewport->getHeight());

            return viewport->getViewPositionY() >= (maxY - 24);
        }

        return true;
    }

    PolyHostPluginProcessor& processor;
    AppSettings& settings;
    juce::TextButton clearButton;
    juce::ToggleButton pauseButton;
    juce::ToggleButton freezeButton;
    juce::TextButton copyRowButton;
    juce::TextButton copyAllButton;
    juce::TextButton exportButton;
    juce::ToggleButton hideClockButton;
    juce::ToggleButton hideActiveSenseButton;
    juce::ToggleButton showNoteButton;
    juce::ToggleButton showPitchBendButton;
    juce::ToggleButton showCcButton;
    juce::ToggleButton showNrpnRpnButton;
    juce::ToggleButton showProgramChangeButton;
    juce::ToggleButton showAftertouchButton;
    juce::ToggleButton showSysExButton;
    juce::ToggleButton showRealtimeButton;
    juce::ToggleButton showSystemCommonButton;
    juce::TableListBox table;
    juce::Array<VstMidiMonitorEntry> allEntries;
    juce::Array<VstMidiMonitorEntry> entries;
    bool autoFollow = true;
};

class VstMidiMonitorWindow : public juce::DocumentWindow
{
public:
    explicit VstMidiMonitorWindow(PolyHostPluginProcessor& processor,
                                  AppSettings& settingsIn,
                                  juce::Component* componentToCentreAround = nullptr)
        : juce::DocumentWindow("MIDI Monitor",
                               juce::Colours::darkgrey,
                               juce::DocumentWindow::closeButton),
          settings(settingsIn)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(minimumWindowWidth, minimumWindowHeight, 2000, 1400);

        content = new VstMidiMonitorComponent(processor, settings);
        setContentOwned(content, true);

        const int windowWidth = juce::jmax(minimumWindowWidth, settings.getMidiMonitorWindowWidth());
        const int windowHeight = juce::jmax(minimumWindowHeight, settings.getMidiMonitorWindowHeight());

        if (componentToCentreAround != nullptr)
            centreAroundComponent(componentToCentreAround, windowWidth, windowHeight);
        else
            centreWithSize(windowWidth, windowHeight);

        setVisible(true);
    }

    ~VstMidiMonitorWindow() override = default;

    void closeButtonPressed() override
    {
        saveSettings();
        setVisible(false);
    }

private:
    static constexpr int minimumWindowWidth = 940;
    static constexpr int minimumWindowHeight = 384;

    void saveSettings()
    {
        if (content != nullptr)
            content->saveSettings(juce::jmax(minimumWindowWidth, getWidth()),
                                  juce::jmax(minimumWindowHeight, getHeight()));
    }

    AppSettings& settings;
    VstMidiMonitorComponent* content = nullptr;
};