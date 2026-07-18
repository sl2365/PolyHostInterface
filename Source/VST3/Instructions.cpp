#include "Instructions.h"
#include <vector>

namespace
{
    struct InstructionTopic
    {
        juce::String title;
        juce::String body;
    };

    juce::String makeHeader(const juce::String& title)
    {
        return title.toUpperCase()
             + "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    }

    juce::String makeBody(const juce::String& title, const juce::String& body)
    {
        return makeHeader(title) + body;
    }

    std::vector<InstructionTopic> buildTopics()
    {
        std::vector<InstructionTopic> topics;

        topics.push_back({ "Overview", makeBody("Overview",
            "PolyHostInterface is a tabbed plugin host designed for loading and managing multiple hosted instruments and effects inside one DAW plugin instance.\n\n"
            "The main workflow is:\n\n"
            "1. Load a plugin into the current tab.\n"
            "2. Add more tabs for more instruments or effects.\n"
            "3. Assign MIDI input/channel routing per tab.\n"
            "4. Use presets to save the full PHI session.\n"
            "5. Use external pointer maps for plugins that need MIDI-controlled mouse/pointer operation.\n\n"
            "Audio/MIDI Routing\n"
            "_________________________________________________________________\n\n"
            " MIDI Device(s)        Audio In from DAW/Host\n"
            "   |                     |\n"
            "   |                     |\n"
            "   +-- [Synth Tab 1] ----+\n"
            "   +-- [Synth Tab 2] ----+  (Audio summed, added to all FX below)\n"
            "   |                     |\n"
            "   |                [FX Tab 1]\n"
            "   |                     |\n"
            "   +----------------[FX Tab 2]\n"
            "   |                     |\n"
            "   +-- [Synth Tab 3] ----+  (only passes through FX below, bypasses any above)\n"
            "                         |\n"
            "                    [FX Tab 3]\n"
            "                         |\n"
            "                         |\n"
            "                    [Audio Out]  ->  Output meter\n\n"
            "_________________________________________________________________\n\n"
            "PHI stores its presets as XML files in the Presets folder. External pointer maps are stored separately in the PluginMaps folder." ) });

        topics.push_back({ "Loading Plugins", makeBody("Loading Plugins",
            "To load a plugin, click the empty plugin area or use File > Replace Plugin.\n\n"
            "If a plugin file exposes multiple shell plugins, PHI shows a chooser so you can select the exact plugin contained inside that shell.\n\n"
            "Use File > New Plugin to create a new tab and immediately load a plugin into it.\n\n"
            "Use File > Replace Plugin to replace the plugin in the current tab while keeping the tab position.\n\n"
            "If a plugin fails to load during preset restore, PHI marks the tab red and keeps the rest of the preset usable." ) });

        topics.push_back({ "Tabs", makeBody("Tabs",
            "Each tab represents one hosted plugin slot. Tabs can be synths, effects, or empty slots.\n\n"
            "Use File > New Tab or the + tab button to create a new empty tab.\n\n"
            "Right-click a tab to access tab actions such as close, clear, replace, and reload.\n\n"
            "Tabs that need attention are shown in red. This usually means the plugin was missing, failed to load, or was quarantined after a restore problem.\n\n"
            "The selected tab determines which plugin GUI, pointer map, MIDI assignments, and routing details are currently visible." ) });

        topics.push_back({ "Presets", makeBody("Presets",
            "PHI presets save the current tab layout, loaded plugins, plugin states, MIDI assignments, routing-related tab state, selected external pointer maps, and macro mappings.\n\n"
            "Use File > Save Preset to save over the current preset.\n\n"
            "Use File > Save Preset As to save a new preset file.\n\n"
            "Use File > Load Preset to restore a saved PHI preset.\n\n"
            "Use File > Recent Presets to reopen recently used presets.\n\n"
            "Use File > Open Presets Folder to open the preset location in Explorer.\n\n"
            "Use File > Presets Backup to create a ZIP backup of the Presets folder. The backup ZIP is saved directly into the Presets folder." ) });

        topics.push_back({ "External Pointer Maps", makeBody("External Pointer Maps",
            "External pointer maps store pointer jump points and pointer free zones for a specific plugin GUI layout. They are useful for plugins with different skins, resizable GUIs, or alternate editor layouts.\n\n"
            "The pointer-map dropdown in the toolbar lists only maps that match the currently loaded plugin identity. The displayed names come from the map filenames without the .xml extension.\n\n"
            "PHI searches the PluginMaps folder recursively, so users can organise maps into subfolders.\n\n"
            "If two matching maps have the same filename, PHI displays them as Name, Name (2), Name (3), and so on.\n\n"
            "The selected map name and relative path are saved into the PHI preset. On restore, PHI first tries the saved relative path, then falls back to the same filename if the map was moved." ) });

        topics.push_back({ "Pointer Edit Mode", makeBody("Pointer Edit Mode",
            "Pointer Edit Mode lets you create and edit pointer jump points directly over the hosted plugin GUI.\n\n"
            "Use the pointer edit toolbar button to enter or leave Pointer Edit Mode.\n\n"
            "Left-click or left-drag in the overlay to add pointer points. Clicking an existing point removes it.\n\n"
            "Snap X aligns new points to the Y positions of nearby saved points.\n"
            "Snap Y aligns new points to the X positions of nearby saved points.\n\n"
            "The mouse Back button toggles Snap Y.\n"
            "The mouse Forward button toggles Snap X.\n\n"
            "The Save icon overwrites the selected external pointer map.\n"
            "The Save As icon creates a new external pointer map file.\n"
            "The Delete icon deletes the selected external pointer map after confirmation." ) });

        topics.push_back({ "Pointer Free Zones", makeBody("Pointer Free Zones",
            "Pointer Free Zones are areas where jump-point snapping is intentionally disabled. They are useful over display panels, menus, XY pads, or plugin regions where normal pointer movement should remain free.\n\n"
            "In Pointer Edit Mode, right-drag over the plugin GUI to add a free zone.\n\n"
            "Right-click inside an existing free zone to delete it. If zones overlap, PHI deletes the smallest zone under the cursor. If sizes match, it deletes the newest one.\n\n"
            "A tab can contain multiple free zones. The bottom overlay bar shows the current free-zone count.\n\n"
            "The free-zone colour can be changed in Pointer Control Settings." ) });

        topics.push_back({ "Pointer Control Settings", makeBody("Pointer Control Settings",
            "Use Options > Pointer Control Settings to configure MIDI pointer control behaviour.\n\n"
            "The settings include pointer movement mode, sensitivity, tolerance control, tab switching CC, mouse button CCs, cursor key CCs, overlay transparency, point colour, preview colour, crosshair colour, and free-zone colour.\n\n"
            "The tolerance CC changes pointer lane tolerance while editing or using pointer maps. The current tolerance value is shown on the pointer overlay bottom bar.\n\n"
            "Mouse-button CCs can trigger left, middle, and right mouse button states at the physical cursor position.\n\n"
            "Cursor Up, Cursor Down, and Enter CCs can send keyboard actions for plugins that require keyboard navigation." ) });

        topics.push_back({ "MIDI Assignments", makeBody("MIDI Assignments",
            "Each tab can have its own MIDI input/channel assignment. This lets different MIDI sources or channels control different hosted plugins.\n\n"
            "Use the MIDI assignment control for a tab to choose which MIDI input/channel entries should feed that tab.\n\n"
            "MIDI Ch: All routes all matching host MIDI into that tab. When MIDI Ch: All is enabled, individual channel entries are disabled to avoid conflicting assignments.\n\n"
            "DAW-routed MIDI may appear as Host MIDI because the DAW does not always pass through the original physical device identity to the plugin." ) });

        topics.push_back({ "MIDI Monitor", makeBody("MIDI Monitor",
            "Use MIDI > MIDI Monitor to inspect incoming MIDI events.\n\n"
            "The monitor shows event time, source, channel, type, data, and raw hex bytes.\n\n"
            "Filters let you show or hide note events, CC, pitch bend, NRPN/RPN, program change, aftertouch, SysEx, realtime/transport, and system common messages.\n\n"
            "Hide Clock and Hide Active Sense suppress high-frequency messages that can otherwise flood the display.\n\n"
            "Pause discards incoming monitor events while active. Freeze keeps capturing internally but stops visual updates until unfrozen.\n\n"
            "Copy Row copies the selected visible event. Copy All copies all visible events. Export saves visible events to a text or CSV file." ) });

        topics.push_back({ "Routing View", makeBody("Routing View",
            "Routing View provides a compact overview of the hosted tabs and their routing state.\n\n"
            "Use the routing toolbar button to switch between the normal plugin editor view and Routing View.\n\n"
            "From Routing View you can select tabs, close tabs, move tabs, inspect routing-related state, and see warnings for missing or failed plugins.\n\n"
            "Routing View is also useful when a plugin GUI is very small or when you need to manage the session structure rather than edit the plugin itself." ) });

        topics.push_back({ "Macro Mapping", makeBody("Macro Mapping",
            "Macro Mapping lets PHI assign the last touched hosted-plugin parameter to macro controls.\n\n"
            "Touch or move a plugin parameter, then use the Map Last Touched control to assign it.\n\n"
            "Use the Macro Mappings view to review, reorder, enable, disable, or clear mappings.\n\n"
            "Mappings are saved with the PHI preset." ) });

        topics.push_back({ "Plugin Diagnostics", makeBody("Plugin Diagnostics",
            "Plugin Diagnostics shows detailed information about the selected tab and hosted plugin.\n\n"
            "Click the routing/info area for a plugin to open diagnostics.\n\n"
            "Diagnostics can include plugin name, manufacturer, format, unique ID, path, channel layout, latency, tail length, parameter count, state status, MIDI assignments, pointer-map status, and restore warnings.\n\n"
            "Use this when diagnosing plugin identity, pointer-map matching, missing plugin restore problems, or unusual hosted-plugin behaviour." ) });

        topics.push_back({ "Preset Load Report", makeBody("Preset Load Report",
            "The Preset Load Report summarises what happened during the most recent preset load.\n\n"
            "If a preset loads cleanly, PHI does not automatically show the report. You can still open it from Help > Preset Load Report.\n\n"
            "If a preset has issues, PHI shows the report automatically.\n\n"
            "The report can include missing plugins, failed plugin restores, quarantined plugins, selected external pointer maps, moved maps, ambiguous duplicate map filenames, and other restore warnings.\n\n"
            "The report remains available until another preset is loaded or a new preset is created." ) });

        topics.push_back({ "Missing / Failed Plugins", makeBody("Missing / Failed Plugins",
            "If a plugin in a preset cannot be found, PHI marks the tab red and keeps the preset open so other tabs remain usable.\n\n"
            "Use Locate Missing Plugins to point PHI to missing plugin files.\n\n"
            "If a plugin fails to load, PHI may quarantine it for the current session to avoid repeated restore failures. Quarantine is session-only and can be cleared by restarting or using the relevant repair options.\n\n"
            "Manual loading or replacing remains available even when automatic preset restore skipped a failed plugin." ) });

        topics.push_back({ "Toolbar Reference", makeBody("Toolbar Reference",
            "The top toolbar contains common session and plugin actions.\n\n"
            "Routing toggles Routing View.\n"
            "Pointer Control enters Pointer Edit Mode.\n"
            "Map Last Touched assigns the last touched hosted-plugin parameter.\n"
            "Macro Mappings opens the macro mapping view.\n"
            "Clear Solos clears solo states.\n"
            "Save and Save As save PHI presets.\n"
            "Revert restores the current preset from disk.\n"
            "Panic sends MIDI panic/all-notes-off style messages.\n"
            "The pointer-map dropdown selects the external pointer map for the current plugin." ) });

        topics.push_back({ "Keyboard / Mouse Controls", makeBody("Keyboard / Mouse Controls",
            "Ctrl+Shift+P sends MIDI Panic.\n\n"
            "In Pointer Edit Mode:\n\n"
            "Left-click or left-drag adds pointer jump points.\n"
            "Clicking an existing point removes it.\n"
            "Right-drag adds a pointer free zone.\n"
            "Right-clicking inside a free zone deletes it.\n"
            "Mouse Back toggles Snap Y.\n"
            "Mouse Forward toggles Snap X.\n\n"
            "Pointer Control Settings can also map MIDI CCs to mouse buttons, cursor up/down, Enter, tab switching, tolerance, and pointer movement." ) });

        topics.push_back({ "Settings / Debug", makeBody("Settings / Debug",
            "Options > Pointer Control Settings opens pointer-specific configuration.\n\n"
            "Options > Plugin Repairs contains tools for plugin scan folders and missing-plugin repair workflows.\n\n"
            "Options > Debug contains debug logging controls. Debug logging can be useful when diagnosing plugin loading, preset restore, pointer-map matching, MIDI routing, or UI behaviour.\n\n"
            "Clear Debug Log Now clears the current debug log. Clear Debug Log On Startup resets it automatically when PHI starts." ) });

        return topics;
    }

    class InstructionsContent final : public juce::Component,
                                      private juce::ListBoxModel
    {
    public:
        InstructionsContent()
            : topics(buildTopics()),
              topicList("Instructions Topics", this)
        {
            titleLabel.setText("PolyHostInterface Instructions", juce::dontSendNotification);
            titleLabel.setJustificationType(juce::Justification::centredLeft);
            titleLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(titleLabel);

            topicList.setRowHeight(26);
            topicList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF171B24));
            topicList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
            topicList.setOutlineThickness(0);
            addAndMakeVisible(topicList);

            contentEditor.setMultiLine(true);
            contentEditor.setReadOnly(true);
            contentEditor.setScrollbarsShown(true);
            contentEditor.setCaretVisible(false);
            contentEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF11151D));
            contentEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            contentEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.18f));
            contentEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::white.withAlpha(0.25f));
            contentEditor.applyFontToAllText(juce::Font(juce::FontOptions("Consolas", 14.0f, juce::Font::plain)));
            addAndMakeVisible(contentEditor);

            copyButton.setButtonText("Copy");
            copyButton.onClick = [this]
            {
                juce::SystemClipboard::copyTextToClipboard(contentEditor.getText());
            };
            addAndMakeVisible(copyButton);

            closeButton.setButtonText("Close");
            closeButton.onClick = [this]
            {
                if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
                    parent->exitModalState(0);
            };
            addAndMakeVisible(closeButton);

            setSize(900, 620);
            topicList.selectRow(0);
            setSelectedTopic(0);
        }

        int getNumRows() override
        {
            return (int) topics.size();
        }

        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override
        {
            if (! juce::isPositiveAndBelow(rowNumber, (int) topics.size()))
                return;

            auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

            if (rowIsSelected)
            {
                g.setColour(juce::Colour(0xFF3A6EA5));
                g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
            }
            else
            {
                g.setColour(juce::Colours::transparentBlack);
                g.fillRect(bounds);
            }

            g.setColour(rowIsSelected ? juce::Colours::white
                                      : juce::Colours::lightgrey.withAlpha(0.92f));
            g.setFont(juce::Font(juce::FontOptions(13.0f, rowIsSelected ? juce::Font::bold
                                                                         : juce::Font::plain)));
            g.drawFittedText(topics[(size_t) rowNumber].title,
                             bounds.reduced(8, 0),
                             juce::Justification::centredLeft,
                             1);
        }

        void selectedRowsChanged(int lastRowSelected) override
        {
            setSelectedTopic(lastRowSelected);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF1D2230));

            auto area = getLocalBounds().reduced(14);
            area.removeFromTop(34);
            area.removeFromTop(10);

            auto left = area.removeFromLeft(230);
            g.setColour(juce::Colour(0xFF171B24));
            g.fillRoundedRectangle(left.toFloat(), 8.0f);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawRoundedRectangle(left.toFloat().reduced(0.5f), 8.0f, 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(14);

            titleLabel.setBounds(area.removeFromTop(34));
            area.removeFromTop(10);

            auto buttonRow = area.removeFromBottom(30);
            closeButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            copyButton.setBounds(buttonRow.removeFromRight(90));
            area.removeFromBottom(10);

            auto left = area.removeFromLeft(230).reduced(8);
            area.removeFromLeft(12);

            topicList.setBounds(left);
            contentEditor.setBounds(area);
        }

    private:
        void setSelectedTopic(int index)
        {
            if (! juce::isPositiveAndBelow(index, (int) topics.size()))
                return;

            contentEditor.setText(topics[(size_t) index].body, juce::dontSendNotification);
            contentEditor.setCaretPosition(0);
        }

        std::vector<InstructionTopic> topics;
        juce::Label titleLabel;
        juce::ListBox topicList;
        juce::TextEditor contentEditor;
        juce::TextButton copyButton;
        juce::TextButton closeButton;
    };
}

void PolyHostInstructions::show(juce::Component* centreAround)
{
    auto content = std::make_unique<InstructionsContent>();
    const int dialogWidth = content->getWidth();
    const int dialogHeight = content->getHeight();

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = "PolyHostInterface Instructions";
    options.dialogBackgroundColour = juce::Colour(0xFF1D2230);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = centreAround;

    if (auto* window = options.launchAsync())
    {
        window->setResizeLimits(720, 460, 1400, 1000);
        window->centreAroundComponent(centreAround, dialogWidth, dialogHeight);
    }
}
