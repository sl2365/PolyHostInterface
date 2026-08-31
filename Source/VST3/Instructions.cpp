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
            "PolyHostInterface (PHI) is a tabbed plugin host for loading and managing multiple hosted instruments and effects. PHI can run as a standalone Windows application or as a VST3 plugin inside a DAW or other VST3 host.\n\n"
            "The main workflow is:\n\n"
            "1. Load a plugin into the current tab.\n"
            "2. Add more tabs for more instruments or effects.\n"
            "3. Assign MIDI channels and adjust the routing order.\n"
            "4. Use presets to save the full PHI session.\n"
            "5. Use external pointer maps for plugins that need MIDI-controlled mouse/pointer operation.\n\n"
            "The standalone version also provides audio and MIDI device settings, tempo and metronome controls, and audio or MIDI recording. These standalone-only controls are not shown when PHI is loaded as a VST3 plugin.\n\n"
            "Audio/MIDI Routing\n"
            "_________________________________________________________________\n\n"
            " Incoming MIDI         Audio Input / Host Audio\n"
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
            "To load a plugin into an empty tab, click the empty plugin area and select its .vst3 or .dll file. VST2 .dll loading is available only when PHI was built with the VST2 SDK.\n\n"
            "If a plugin file exposes multiple shell plugins, PHI shows a chooser so you can select the exact plugin contained inside that shell.\n\n"
            "Use File > New Plugin to create a new tab and immediately load a plugin into it.\n\n"
            "Use File > Replace Plugin to replace the plugin in the current tab while keeping the tab position.\n\n"
            "You can also drag a .vst3 or supported .dll file from Explorer onto PHI. Dropping onto an empty tab loads it directly. If the selected tab already contains a plugin, PHI lets you open the dropped plugin in a new tab, replace the current plugin, or cancel.\n\n"
            "In standalone mode, Windows can be associated with PHI for supported plugin files. Opening a plugin file from Explorer starts PHI with that plugin. If PHI is already open, the plugin is opened in a new tab.\n\n"
            "If a plugin fails to load during preset restore, PHI marks the tab red and keeps the rest of the preset usable." ) });

        topics.push_back({ "Tabs", makeBody("Tabs",
            "Each tab represents one hosted plugin slot. Tabs can be synths, effects, or empty slots.\n\n"
            "Use File > New Tab or the + tab button to create a new empty tab.\n\n"
            "Right-click a tab to create a new tab, replace or reload its plugin, clear the tab, or close it.\n\n"
            "Tabs that need attention are shown in red. This usually means the plugin was missing, failed to load, or was quarantined after a restore problem.\n\n"
            "The selected tab determines which plugin GUI, pointer map, MIDI-channel assignments, and plugin details are currently active. Use the tab scroll buttons when the window is too narrow to show every tab." ) });

        topics.push_back({ "Presets", makeBody("Presets",
            "PHI presets save the current tab layout, loaded plugins, plugin states, MIDI assignments, routing-related tab state, selected external pointer maps, and macro mappings.\n\n"
            "The Presets menu is rebuilt from the current contents of the Presets folder whenever it is opened. Preset subfolders appear as submenus, so presets can be organised into folders without losing direct menu access.\n\n"
            "Use File > Save Preset to save over the current preset.\n\n"
            "Use File > Save Preset As to save a new preset file.\n\n"
            "Use File > Load Preset to restore a saved PHI preset.\n\n"
            "Use File > Recent Presets or the preset dropdown to reopen recently used presets. The dropdown contains New Preset followed by available recent presets; it is not a complete list of the Presets folder.\n\n"
            "Use File > Delete Current Preset to delete the preset currently associated with the session.\n\n"
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
            "Snap X aligns points into horizontal rows for X-axis movement by matching the Y position of nearby saved points.\n"
            "Snap Y aligns points into vertical columns for Y-axis movement by matching the X position of nearby saved points.\n\n"
            "The mouse Back button toggles Snap Y.\n"
            "The mouse Forward button toggles Snap X.\n\n"
            "Middle-click closes Pointer Edit Mode. The mouse wheel cycles through matching external pointer maps.\n\n"
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
            "The settings include X and Y movement CCs, parameter-adjust CC and mode, tab-switching CC and delay, mouse-button CCs, cursor-key CCs, tolerance and sensitivity controls, adjustment method, drag return delay, snap weights, and overlay appearance.\n\n"
            "The tolerance CC changes pointer lane tolerance while editing or using pointer maps. The current tolerance value is shown on the pointer overlay bottom bar.\n\n"
            "Mouse-button CCs can trigger left, middle, and right mouse button states at the physical cursor position.\n\n"
            "Cursor Up, Cursor Down, and Enter CCs can send keyboard actions for plugins that require keyboard navigation. Point, preview, crosshair and free-zone colours can also be adjusted." ) });

        topics.push_back({ "MIDI Assignments", makeBody("MIDI Assignments",
            "Each tab has its own MIDI-channel assignment. This allows different MIDI channels to control different hosted plugins.\n\n"
            "Use the MIDI Ch control on a tab or in Routing View to select MIDI Ch: All or one or more individual channels from 1 to 16.\n\n"
            "MIDI Ch: All routes every channel into that tab. When MIDI Ch: All is enabled, the individual channel entries are disabled to avoid conflicting assignments.\n\n"
            "In standalone mode, physical MIDI input devices are enabled globally in MIDI > MIDI Settings. The per-tab MIDI Ch control then filters the combined incoming stream by channel. In the VST3 version, MIDI arrives from the DAW or VST3 host." ) });

        topics.push_back({ "MIDI Settings / Output", makeBody("MIDI Settings / Output",
            "Use MIDI > MIDI Settings to configure MIDI input and output behaviour. The available controls differ between standalone and VST3 versions.\n\n"
            "Standalone:\n\n"
            "Enable the physical MIDI input devices PHI should receive. Choose a hardware or virtual MIDI output, then optionally enable Send generated MIDI to output and MIDI Thru. Generated MIDI includes events produced by hosted arpeggiators, sequencers and MIDI effects. MIDI Thru also forwards the original incoming MIDI.\n\n"
            "VST3:\n\n"
            "Send generated MIDI to host returns MIDI created or changed by hosted plugins to the DAW. MIDI Thru also returns the original MIDI received from the DAW.\n\n"
            "Do not route PHI's MIDI output back into its own input, because MIDI Thru can create duplicate notes or a feedback loop. Moving an on-screen plugin parameter normally produces plugin automation, not MIDI; it is recorded or sent only if that hosted plugin actually generates MIDI events." ) });

        topics.push_back({ "MIDI Monitor", makeBody("MIDI Monitor",
            "Use MIDI > MIDI Monitor to inspect incoming MIDI events.\n\n"
            "The monitor shows event time, source, channel, type, data, and raw hex bytes.\n\n"
            "Filters let you show or hide note events, CC, pitch bend, NRPN/RPN, program change, aftertouch, SysEx, realtime/transport, and system common messages.\n\n"
            "Hide Clock and Hide Active Sense suppress high-frequency messages that can otherwise flood the display.\n\n"
            "Pause discards incoming monitor events while active. Freeze keeps capturing internally but stops visual updates until unfrozen.\n\n"
            "Copy Row copies the selected visible event. Copy All copies all visible events. Export saves visible events to a text or CSV file.\n\n"
            "PHI currently receives MIDI as a combined stream and labels it Host MIDI. The monitor does not retain the original physical-device identity." ) });

        topics.push_back({ "Routing View", makeBody("Routing View",
            "Routing View provides a compact overview of loaded tabs in their processing order.\n\n"
            "Use the routing toolbar button to switch between the normal plugin editor view and Routing View.\n\n"
            "From Routing View you can select or close tabs, drag them into a new order, change their MIDI-channel assignments and pointer adjustment method, toggle bypass or solo, and open Plugin Diagnostics. Changes apply immediately.\n\n"
            "Synth outputs join the current audio signal. Effects process the signal reaching their position, so changing the tab order changes which later effects process each synth.\n\n"
            "Routing View is also useful when a plugin GUI is very small or when you need to manage the session structure rather than edit the plugin itself." ) });

        topics.push_back({ "Macro Mapping", makeBody("Macro Mapping",
            "Macro Mapping lets PHI assign the last touched hosted-plugin parameter to macro controls.\n\n"
            "Touch or move a plugin parameter, then use the Map Last Touched control to assign it.\n\n"
            "Click the highlighted Tab, Plugin, or Parameter title in the Macro Mappings view to sort ascending, descending, then return to Macro order. Use the drag handle at the left of each row while Macro order is active to move mappings between Macro 001-128 slots. You can also filter mappings, replace a target with the current last touched parameter, delete mappings, undo the last mapping edit, or clear all mappings.\n\n"
            "In the PHI VST3 version, automate Macro 001-128 from the DAW to control the mapped hosted-plugin parameters. The additional MIDI CC parameter entries exposed by some hosts are not the macro controls.\n\n"
            "Mappings are saved with the PHI preset. Standalone PHI has no DAW automation source, although the mappings remain part of a preset that can also be loaded in PHI VST3." ) });

        topics.push_back({ "Plugin Diagnostics", makeBody("Plugin Diagnostics",
            "Plugin Diagnostics shows detailed information about the selected tab and hosted plugin.\n\n"
            "Open Routing View and click the information button for a plugin to open its diagnostics.\n\n"
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
            "Use File > Locate Missing Plugins to point PHI to replacement plugin files. After a successful repair, save the preset to retain the corrected path.\n\n"
            "Options > Plugin Repairs contains the configured plugin scan folders and the option to auto-save a preset after repair.\n\n"
            "If a plugin fails to load, PHI may quarantine it for the current session to avoid repeated restore failures. Restarting PHI clears the session quarantine. Manual loading or replacing remains available even when automatic preset restore skipped a failed plugin." ) });

        topics.push_back({ "Toolbar Reference", makeBody("Toolbar Reference",
            "The main toolbar contains common session and plugin actions.\n\n"
            "Routing toggles Routing View.\n"
            "Pointer Control toggles Pointer Edit Mode.\n"
            "Map Last Touched assigns the last touched hosted-plugin parameter.\n"
            "Macro Mappings opens the macro mapping view.\n"
            "Clear Solos clears solo states.\n"
            "Save and Save As save PHI presets.\n"
            "Revert restores the current preset from disk.\n"
            "Panic sends MIDI panic/all-notes-off style messages.\n"
            "The pointer-map dropdown selects the external pointer map for the current plugin.\n\n"
            "Standalone PHI also places tempo, metronome, tap-tempo and Record controls at the right of the menu bar. These controls are not displayed in PHI VST3." ) });

        topics.push_back({ "Standalone Tempo", makeBody("Standalone Tempo",
            "Standalone PHI provides tempo controls at the right of the menu bar. The four beat LEDs show the current position in a 4/4 bar, with the first beat shown in red.\n\n"
            "Type a BPM value into the tempo field, use the mouse wheel to adjust it by 1.0 BPM, or hold Shift while scrolling to adjust it by 0.1 BPM. Double-click the field or use the Reset button to restore the saved default tempo. Right-click the tempo field and choose Set as Default to store the current value.\n\n"
            "Use Tap Tempo repeatedly to calculate a tempo from your clicks.\n\n"
            "The metronome button cycles through three saved modes:\n\n"
            "Off - default button colour; no metronome.\n"
            "On - blue; metronome plays continuously.\n"
            "Record Only - red; metronome plays while recording is armed, during count-in or Wait Note, and while recording.\n\n"
            "The metronome is added after PHI's recording tap, so it is heard at the output but is never written into audio recordings." ) });

        topics.push_back({ "Recording", makeBody("Recording",
            "Audio and MIDI recording are available only in standalone PHI. Open the Recording view with Options > Recording or by right-clicking the Record button. Recording continues if the view is closed or another tab is selected.\n\n"
            "Use the Audio/MIDI switch to choose the recording type, then left-click the Record button to arm recording. The button is orange while waiting for count-in or Wait Note, and red once recording is being captured. Left-click it again to cancel an armed recording or stop an active recording.\n\n"
            "Count-in can be set to 0, 1, 2, 4 or 8 bars, or Wait Note. A bar count-in begins on a newly synchronised first beat. Wait Note preserves the existing metronome timing and starts recording at the first MIDI note-on. The metronome sounds while armed when its mode permits it, but is not recorded.\n\n"
            "Audio mode records the stereo PHI output as a 24-bit WAV file. MIDI mode creates a standard Type 1 MIDI file at 960 PPQ in 4/4. MIDI recording can include external notes, external controllers, and MIDI generated by hosted plugins. At least one MIDI source must remain selected.\n\n"
            "Completed files are stored in the Recordings folder beside the standalone executable. PHI creates the folder when required. The file list switches between WAV and MIDI files; double-click a completed file to open it in the Windows default application." ) });

        topics.push_back({ "Keyboard / Mouse Controls", makeBody("Keyboard / Mouse Controls",
            "Ctrl+Shift+P sends MIDI Panic.\n\n"
            "In Pointer Edit Mode:\n\n"
            "Left-click or left-drag adds pointer jump points.\n"
            "Clicking an existing point removes it.\n"
            "Right-drag adds a pointer free zone.\n"
            "Right-clicking inside a free zone deletes it.\n"
            "Middle-click closes Pointer Edit Mode.\n"
            "Mouse wheel selects the previous or next matching pointer map.\n"
            "Mouse Back toggles Snap Y.\n"
            "Mouse Forward toggles Snap X.\n\n"
            "In the standalone tempo field:\n\n"
            "Mouse wheel adjusts tempo by 1.0 BPM.\n"
            "Shift+Mouse wheel adjusts tempo by 0.1 BPM.\n"
            "Double-click resets tempo.\n"
            "Right-click provides Set as Default.\n\n"
            "Right-clicking the standalone Record button opens or closes Recording View without arming recording.\n\n"
            "Pointer Control Settings can also map MIDI CCs to mouse buttons, cursor up/down, Enter, tab switching, tolerance, sensitivity, parameter adjustment, and pointer movement." ) });

        topics.push_back({ "Settings / Debug", makeBody("Settings / Debug",
            "Options > Pointer Control Settings opens pointer-specific configuration.\n\n"
            "In standalone PHI, Options > Audio Settings configures the audio driver, input and output devices, sample rate, buffer size and active channels. Options > Recording opens or closes Recording View. These two items are not present in PHI VST3.\n\n"
            "Options > Plugin Repairs configures plugin scan folders and whether repaired presets are saved automatically. Use File > Locate Missing Plugins to perform the actual missing-plugin repair.\n\n"
            "Options > Debug contains debug logging controls. Debug logging can be useful when diagnosing plugin loading, preset restore, pointer-map matching, MIDI routing, or UI behaviour.\n\n"
            "Enable Advanced Debug Logging adds more detailed messages and is available only while normal debug logging is enabled. Clear Debug Log Now clears the current debug log. Clear Debug Log On Startup resets it automatically when PHI starts." ) });

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

            auto left = area.removeFromLeft(230).reduced(8);
            area.removeFromLeft(12);

            auto buttonRow = area.removeFromBottom(30);
            closeButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            copyButton.setBounds(buttonRow.removeFromRight(90));
            area.removeFromBottom(10);

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
