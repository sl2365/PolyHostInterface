#pragma once
#include <JuceHeader.h>
#include "MidiEngine.h"

struct MidiMonitorEntry
{
    juce::String time;
    juce::String type;
    juce::String channel;
    juce::String data1;
    juce::String data2;
    juce::String description;
    juce::Colour colour;
};

class MidiMonitorComponent : public juce::Component,
                             private juce::Timer,
                             private juce::TableListBoxModel,
                             private juce::Button::Listener
{
public:
    explicit MidiMonitorComponent(MidiEngine& engine) : midiEngine(engine)
    {
        addAndMakeVisible(clearButton);
        clearButton.setButtonText("Clear");
        clearButton.addListener(this);

        addAndMakeVisible(pauseButton);
        pauseButton.setButtonText("Pause");
        pauseButton.setClickingTogglesState(true);
        pauseButton.addListener(this);

        addAndMakeVisible(table);
        table.setModel(this);

        table.getHeader().addColumn("Time",        1, 100);
        table.getHeader().addColumn("Type",        2, 110);
        table.getHeader().addColumn("Ch",          3, 50);
        table.getHeader().addColumn("Data 1",      4, 70);
        table.getHeader().addColumn("Data 2",      5, 70);
        table.getHeader().addColumn("Description", 6, 300);

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
        area.removeFromTop(8);

        table.setBounds(area);
    }

private:
    int getNumRows() override
    {
        return entries.size();
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool selected) override
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

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool) override
    {
        if (!juce::isPositiveAndBelow(rowNumber, entries.size()))
            return;

        const auto& e = entries.getReference(rowNumber);

        juce::String text;
        switch (columnId)
        {
            case 1: text = e.time; break;
            case 2: text = e.type; break;
            case 3: text = e.channel; break;
            case 4: text = e.data1; break;
            case 5: text = e.data2; break;
            case 6: text = e.description; break;
            default: break;
        }

        g.setColour(juce::Colours::white);
        g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);

        g.setColour(juce::Colours::grey);
        g.fillRect(width - 1, 0, 1, height);
    }

    void timerCallback() override
    {
        if (pauseButton.getToggleState())
            return;

        auto messages = midiEngine.popPendingMessages();
        if (messages.isEmpty())
            return;

        for (auto& incoming : messages)
            entries.add(makeEntry(incoming.message));

        if (entries.size() > 1000)
            entries.removeRange(0, entries.size() - 1000);

        table.updateContent();
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

    MidiMonitorEntry makeEntry(const juce::MidiMessage& msg)
    {
        MidiMonitorEntry e;

        auto now = juce::Time::getCurrentTime();
        e.time = now.formatted("%H:%M:%S");

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
        else
        {
            e.type = "Other";
            e.channel = juce::String(msg.getChannel());
            e.data1 = "-";
            e.data2 = "-";
            e.description = msg.getDescription();
            e.colour = juce::Colours::white;
        }

        return e;
    }

    MidiEngine& midiEngine;
    juce::TextButton clearButton;
    juce::ToggleButton pauseButton;
    juce::TableListBox table;
    juce::Array<MidiMonitorEntry> entries;
};

class MidiMonitorWindow : public juce::DocumentWindow
{
public:
    explicit MidiMonitorWindow(MidiEngine& engine)
        : juce::DocumentWindow("MIDI Monitor",
                               juce::Colours::darkgrey,
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(new MidiMonitorComponent(engine), true);
        centreWithSize(760, 360);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};