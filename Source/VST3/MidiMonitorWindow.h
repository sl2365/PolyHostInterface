#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct VstMidiMonitorEntry
{
    juce::String time;
    juce::String device;
    juce::String type;
    juce::String channel;
    juce::String data1;
    juce::String data2;
    juce::String description;
    juce::Colour colour;
};

class VstMidiMonitorComponent : public juce::Component,
                                private juce::Timer,
                                private juce::TableListBoxModel,
                                private juce::Button::Listener
{
public:
    explicit VstMidiMonitorComponent(PolyHostPluginProcessor& processorIn)
        : processor(processorIn)
    {
        addAndMakeVisible(clearButton);
        clearButton.setButtonText("Clear");
        clearButton.addListener(this);

        addAndMakeVisible(pauseButton);
        pauseButton.setButtonText("Pause");
        pauseButton.setClickingTogglesState(true);
        pauseButton.addListener(this);

        addAndMakeVisible(hideClockButton);
        hideClockButton.setButtonText("Hide Clock");
        hideClockButton.setClickingTogglesState(true);

        addAndMakeVisible(hideActiveSenseButton);
        hideActiveSenseButton.setButtonText("Hide Active Sense");
        hideActiveSenseButton.setClickingTogglesState(true);

        addAndMakeVisible(table);
        table.setModel(this);

        table.getHeader().addColumn("Time",        1, 80);
        table.getHeader().addColumn("Source",      2, 150);
        table.getHeader().addColumn("Type",        3, 110);
        table.getHeader().addColumn("Ch",          4, 40);
        table.getHeader().addColumn("Data 1",      5, 60);
        table.getHeader().addColumn("Data 2",      6, 60);
        table.getHeader().addColumn("Description", 7, 260);

        table.setColour(juce::ListBox::backgroundColourId, juce::Colours::black);
        table.setRowHeight(22);

        startTimerHz(20);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto topBar = area.removeFromTop(32);

        clearButton.setBounds(topBar.removeFromLeft(80));
        topBar.removeFromLeft(8);
        pauseButton.setBounds(topBar.removeFromLeft(80));
        topBar.removeFromLeft(8);
        hideClockButton.setBounds(topBar.removeFromLeft(110));
        topBar.removeFromLeft(8);
        hideActiveSenseButton.setBounds(topBar.removeFromLeft(150));
        area.removeFromTop(8);

        table.setBounds(area);
    }

private:
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
        if (pauseButton.getToggleState())
            return;

        autoFollow = isScrolledToBottom();

        auto messages = processor.popPendingMidiMonitorEvents();
        if (messages.isEmpty())
            return;

        for (auto& incoming : messages)
        {
            if (!incoming.valid)
                continue;

            if (incoming.message.getRawDataSize() > 0)
            {
                const auto status = (unsigned char) incoming.message.getRawData()[0];

                if (hideClockButton.getToggleState() && status == 0xF8)
                    continue;

                if (hideActiveSenseButton.getToggleState() && status == 0xFE)
                    continue;
            }

            entries.add(makeEntry(incoming));
        }

        if (entries.size() > 1000)
            entries.removeRange(0, entries.size() - 1000);

        table.updateContent();

        if (autoFollow && entries.size() > 0)
            table.scrollToEnsureRowIsOnscreen(entries.size() - 1);

        repaint();
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == &clearButton)
        {
            entries.clear();
            table.updateContent();
            repaint();
        }
    }

    VstMidiMonitorEntry makeEntry(const PolyHostPluginProcessor::MidiMonitorEvent& incoming)
    {
        VstMidiMonitorEntry e;

        auto now = juce::Time::getCurrentTime();
        e.time = now.formatted("%H:%M:%S");
        e.device = incoming.sourceName;

        const auto& msg = incoming.message;

        if (msg.isNoteOn())
        {
            e.type = "Note On";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String((int) msg.getVelocity());
            e.description = msg.getDescription();
            e.colour = juce::Colours::green;
        }
        else if (msg.isNoteOff())
        {
            e.type = "Note Off";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String((int) msg.getVelocity());
            e.description = msg.getDescription();
            e.colour = juce::Colours::red;
        }
        else if (msg.isController())
        {
            e.type = "CC";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getControllerNumber());
            e.data2 = juce::String(msg.getControllerValue());
            e.description = msg.getDescription();
            e.colour = juce::Colours::yellow;
        }
        else if (msg.isPitchWheel())
        {
            e.type = "Pitch Bend";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getPitchWheelValue());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::cyan;
        }
        else if (msg.isAftertouch())
        {
            e.type = "Aftertouch";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getNoteNumber());
            e.data2 = juce::String(msg.getAfterTouchValue());
            e.description = msg.getDescription();
            e.colour = juce::Colours::orange;
        }
        else if (msg.isChannelPressure())
        {
            e.type = "Ch Pressure";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getChannelPressureValue());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::orange;
        }
        else if (msg.isProgramChange())
        {
            e.type = "Program";
            e.channel = juce::String(msg.getChannel());
            e.data1 = juce::String(msg.getProgramChangeNumber());
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::purple;
        }
        else if (msg.isSysEx())
        {
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
                case 0xF1: e.type = "System";   e.description = "MIDI Time Code Quarter Frame"; e.colour = juce::Colours::lightblue; break;
                case 0xF2: e.type = "System";   e.description = "Song Position Pointer";        e.colour = juce::Colours::lightblue; break;
                case 0xF3: e.type = "System";   e.description = "Song Select";                  e.colour = juce::Colours::lightblue; break;
                case 0xF6: e.type = "System";   e.description = "Tune Request";                 e.colour = juce::Colours::lightblue; break;
                case 0xF8: e.type = "Realtime"; e.description = "MIDI Clock";                   e.colour = juce::Colours::orange; break;
                case 0xFA: e.type = "Realtime"; e.description = "Start";                        e.colour = juce::Colours::green; break;
                case 0xFB: e.type = "Realtime"; e.description = "Continue";                     e.colour = juce::Colours::greenyellow; break;
                case 0xFC: e.type = "Realtime"; e.description = "Stop";                         e.colour = juce::Colours::red; break;
                case 0xFE: e.type = "Realtime"; e.description = "Active Sensing";               e.colour = juce::Colours::yellow; break;
                case 0xFF: e.type = "Realtime"; e.description = "System Reset";                 e.colour = juce::Colours::red; break;
                default:   e.type = "Other";    e.description = msg.getDescription();           e.colour = juce::Colours::white; break;
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
    juce::TextButton clearButton;
    juce::ToggleButton pauseButton;
    juce::ToggleButton hideClockButton;
    juce::ToggleButton hideActiveSenseButton;
    juce::TableListBox table;
    juce::Array<VstMidiMonitorEntry> entries;
    bool autoFollow = true;
};

class VstMidiMonitorWindow : public juce::DocumentWindow
{
public:
    explicit VstMidiMonitorWindow(PolyHostPluginProcessor& processor)
        : juce::DocumentWindow("MIDI Monitor",
                               juce::Colours::darkgrey,
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(new VstMidiMonitorComponent(processor), true);
        centreWithSize(800, 360);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};