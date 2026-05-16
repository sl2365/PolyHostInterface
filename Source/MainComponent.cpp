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

            const int width = 280;
            const int height = 20
                             + (deviceButtons.size() * 28)
                             + 13   // space between device list and buttons
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
        std::function<void(const juce::String& deviceIdentifier)> toggleAssignment;
        std::function<void()> assignAllEnabled;
        std::function<void()> clearAllAssignments;
        std::function<juce::StringArray()> getAssignedIdentifiers;
    };

    class PointerControlSettingsComponent final : public juce::Component
    {
    public:
        PointerControlSettingsComponent(AppSettings& settingsIn, float currentToleranceIn)
                                : settings(settingsIn), currentTolerance(currentToleranceIn)
        {
            auto configureLabel = [this](juce::Label& label, const juce::String& text, juce::Justification justification)
            {
                addAndMakeVisible(label);
                label.setText(text, juce::dontSendNotification);
                label.setJustificationType(justification);
                label.setColour(juce::Label::textColourId, juce::Colours::white);
            };

            configureLabel(xCcLabel, "X CC:", juce::Justification::centredRight);
            configureLabel(yCcLabel, "Y CC:", juce::Justification::centredRight);
            configureLabel(adjustCcLabel, "Adjust CC:", juce::Justification::centredRight);
            configureLabel(adjustModeLabel, "Adjust Mode:", juce::Justification::centredRight);
            configureLabel(toleranceCcLabel, "Tolerance CC:", juce::Justification::centredRight);
            configureLabel(sensitivityCcLabel, "Sensitivity CC:", juce::Justification::centredRight);
            configureLabel(adjustSensitivityLabel, "Adjust Sens:", juce::Justification::centredRight);
            configureLabel(adjustMethodLabel, "Adjust Method:", juce::Justification::centredRight);
            configureLabel(xWeightLabel, "X Weight:", juce::Justification::centredRight);
            configureLabel(yWeightLabel, "Y Weight:", juce::Justification::centredRight);
            configureLabel(overlayTransparencyLabel, "Overlay\nTransparency:", juce::Justification::centredRight);
            configureLabel(pointSizeLabel, "Point Size:", juce::Justification::centredRight);
            configureLabel(showCrosshairLabel, "Show Crosshair:", juce::Justification::centredRight);
            configureLabel(crosshairRgbaLabel, "Crosshair RGBA:", juce::Justification::centredRight);
            configureLabel(pointRgbLabel, "Point RGB:", juce::Justification::centredRight);
            configureLabel(previewRgbLabel, "Preview RGB:", juce::Justification::centredRight);

            configureTipLabel(currentToleranceInfoLabel, "Adjusts X/Y tolerance. Higher values = wider snapping.");
            configureTipLabel(currentToleranceLineLabel,
                              "Current Tab Setting:    " + juce::String(currentTolerance, 1) + " px");

            configureTipLabel(tipXcc, "Horizontal pointer control movement.");
            configureTipLabel(tipYcc, "Vertical pointer control movement.");
            configureTipLabel(tipAdjustCc, "Used to adjust software parameters.");
            configureTipLabel(tipAdjustMode, "");
            configureTipLabel(tipAdjustSensitivity, "Repeats adjust steps per encoder movement. Higher values help stubborn plugin controls.");
            configureTipLabel(tipSensitivityCc, "MIDI CC used to control Adjust Sensitivity live.");
            configureTipLabel(tipAdjustMethod, "");
            configureTipLabel(tipWeights, "Fallback snap weighting. (Legacy?)\nDefaults: X=1.0  Y=0.2");
            configureTipLabel(tipOverlay, "Overlay darkness while editing.");
            configureTipLabel(tipPointSize, "Size of saved jump-point colour markers.");
            configureTipLabel(tipShowCrosshair, "Shows full-width and full-height guide lines.");
            configureTipLabel(tipRgb, "Point RGB: Saved point colour.\nPreview RGB: Point colour on mouse down.");
            configureTipLabel(tipCrosshairRgba, "Crosshair colour and alpha.");

            addAndMakeVisible(separatorAfterAdjustMethod);
            addAndMakeVisible(separatorAfterAdjustSensitivity);
            addAndMakeVisible(separatorAfterWeights);

            configureCcEditor(xCcEditor, settings.getPointerControlXccNumber());
            configureCcEditor(yCcEditor, settings.getPointerControlYccNumber());
            configureCcEditor(adjustCcEditor, settings.getPointerControlAdjustCcNumber());
            configureCcEditor(xCcEditor, settings.getPointerControlXccNumber());
            configureCcEditor(yCcEditor, settings.getPointerControlYccNumber());
            configureCcEditor(adjustCcEditor, settings.getPointerControlAdjustCcNumber());
            configureIntegerEditor(adjustModeEditor,
                                   settings.getPointerControlAdjustCcMode(),
                                   1, 4);

            configureCcEditor(toleranceCcEditor, settings.getPointerControlToleranceCcNumber());
            configureCcEditor(sensitivityCcEditor, settings.getPointerControlSensitivityCcNumber());
            configureIntegerEditor(adjustSensitivityEditor,
                                   settings.getPointerControlAdjustSensitivity(),
                                   1, 20);
            configureIntegerEditor(adjustMethodEditor,
                                   settings.getPointerControlAdjustMethod(),
                                   1, 2);

            updateAdjustModeTip();
            updateAdjustMethodTip();

            configureWeightEditor(xWeightEditor, settings.getPointerControlXSnapWeight());
            configureWeightEditor(yWeightEditor, settings.getPointerControlYSnapWeight());
            configureWeightEditor(xWeightEditor, settings.getPointerControlXSnapWeight());
            configureWeightEditor(yWeightEditor, settings.getPointerControlYSnapWeight());

            configureIntegerEditor(overlayTransparencyEditor, settings.getPointerControlOverlayTransparency(), 0, 150);
            configureIntegerEditor(pointSizeEditor, settings.getPointerControlPointSize(), 3, 15);

            addAndMakeVisible(showCrosshairToggle);
            showCrosshairToggle.setButtonText({});
            showCrosshairToggle.setToggleState(settings.getPointerControlShowCrosshair(),
                                               juce::dontSendNotification);
            showCrosshairToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);

            auto pointColour = settings.getPointerControlPointColour();
            auto previewColour = settings.getPointerControlPreviewColour();
            auto crosshairColour = settings.getPointerControlCrosshairColour();

            configureIntegerEditor(pointColourREditor, (int) pointColour.getRed());
            configureIntegerEditor(pointColourGEditor, (int) pointColour.getGreen());
            configureIntegerEditor(pointColourBEditor, (int) pointColour.getBlue());

            configureIntegerEditor(previewColourREditor, (int) previewColour.getRed());
            configureIntegerEditor(previewColourGEditor, (int) previewColour.getGreen());
            configureIntegerEditor(previewColourBEditor, (int) previewColour.getBlue());

            configureIntegerEditor(crosshairColourREditor, (int) crosshairColour.getRed());
            configureIntegerEditor(crosshairColourGEditor, (int) crosshairColour.getGreen());
            configureIntegerEditor(crosshairColourBEditor, (int) crosshairColour.getBlue());
            configureIntegerEditor(crosshairColourAEditor, (int) crosshairColour.getAlpha());

            setSize(430, 360);
        }

        void apply()
        {
            settings.setPointerControlXccNumber(parseInt(xCcEditor.getText(), 24, 0, 127));
            settings.setPointerControlYccNumber(parseInt(yCcEditor.getText(), 25, 0, 127));
            settings.setPointerControlAdjustCcNumber(parseInt(adjustCcEditor.getText(), 26, 0, 127));
            settings.setPointerControlAdjustCcMode(parseInt(adjustModeEditor.getText(), 1, 1, 4));
            settings.setPointerControlToleranceCcNumber(parseInt(toleranceCcEditor.getText(), 22, 0, 127));
            settings.setPointerControlSensitivityCcNumber(parseInt(sensitivityCcEditor.getText(), 23, 0, 127));
            settings.setPointerControlAdjustSensitivity(parseInt(adjustSensitivityEditor.getText(), 1, 1, 20));
            settings.setPointerControlAdjustMethod(parseInt(adjustMethodEditor.getText(), 1, 1, 2));

            settings.setPointerControlXSnapWeight(parseFloat(xWeightEditor.getText(), 1.0f, 0.001f, 100.0f));
            settings.setPointerControlYSnapWeight(parseFloat(yWeightEditor.getText(), 0.20f, 0.001f, 100.0f));

            settings.setPointerControlOverlayTransparency(parseInt(overlayTransparencyEditor.getText(), 40, 0, 150));
            settings.setPointerControlPointSize(parseInt(pointSizeEditor.getText(), 4, 3, 15));
            settings.setPointerControlShowCrosshair(showCrosshairToggle.getToggleState());

            settings.setPointerControlPointColour(juce::Colour::fromRGB(
                (juce::uint8) parseInt(pointColourREditor.getText(), 80, 0, 255),
                (juce::uint8) parseInt(pointColourGEditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(pointColourBEditor.getText(), 120, 0, 255)));

            settings.setPointerControlPreviewColour(juce::Colour::fromRGB(
                (juce::uint8) parseInt(previewColourREditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(previewColourGEditor.getText(), 210, 0, 255),
                (juce::uint8) parseInt(previewColourBEditor.getText(), 0, 0, 255)));

            settings.setPointerControlCrosshairColour(juce::Colour::fromRGBA(
                (juce::uint8) parseInt(crosshairColourREditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(crosshairColourGEditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(crosshairColourBEditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(crosshairColourAEditor.getText(), 70, 0, 255)));
        }

        int getSelectedAdjustMethod() const
        {
            return parseInt(adjustMethodEditor.getText(), 1, 1, 2);
        }

        void resetToDefaults()
        {
            xCcEditor.setText("24", juce::dontSendNotification);
            yCcEditor.setText("25", juce::dontSendNotification);
            adjustCcEditor.setText("26", juce::dontSendNotification);
            adjustModeEditor.setText("1", juce::dontSendNotification);
            updateAdjustModeTip();
            toleranceCcEditor.setText("22", juce::dontSendNotification);
            sensitivityCcEditor.setText("23", juce::dontSendNotification);
            adjustSensitivityEditor.setText("1", juce::dontSendNotification);
            adjustMethodEditor.setText("1", juce::dontSendNotification);
            updateAdjustMethodTip();

            xWeightEditor.setText("1.0", juce::dontSendNotification);
            yWeightEditor.setText("0.2", juce::dontSendNotification);

            overlayTransparencyEditor.setText("30", juce::dontSendNotification);
            pointSizeEditor.setText("4", juce::dontSendNotification);
            showCrosshairToggle.setToggleState(true, juce::dontSendNotification);

            pointColourREditor.setText("80", juce::dontSendNotification);
            pointColourGEditor.setText("255", juce::dontSendNotification);
            pointColourBEditor.setText("120", juce::dontSendNotification);

            previewColourREditor.setText("255", juce::dontSendNotification);
            previewColourGEditor.setText("210", juce::dontSendNotification);
            previewColourBEditor.setText("0", juce::dontSendNotification);

            crosshairColourREditor.setText("255", juce::dontSendNotification);
            crosshairColourGEditor.setText("255", juce::dontSendNotification);
            crosshairColourBEditor.setText("255", juce::dontSendNotification);
            crosshairColourAEditor.setText("70", juce::dontSendNotification);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(12);
            area.removeFromTop(2); // move everything up slightly overall

            constexpr int rowHeight = 28;
            constexpr int rowGap = 6;
            constexpr int labelWidth = 100;
            constexpr int fieldWidth = 60;
            constexpr int infoGap = 8;
            constexpr int infoWidth = 220;
            constexpr int rgbFieldWidth = 40;
            constexpr int separatorHeight = 8;

            auto layoutStandardRow = [&](juce::Label& label,
                                         juce::TextEditor& editor,
                                         juce::Label* tip = nullptr)
            {
                auto row = area.removeFromTop(rowHeight);
                label.setBounds(row.removeFromLeft(labelWidth));
                editor.setBounds(row.removeFromLeft(fieldWidth));

                if (tip != nullptr)
                {
                    row.removeFromLeft(infoGap);
                    tip->setBounds(row.removeFromLeft(infoWidth));
                }

                area.removeFromTop(rowGap);
            };

            auto layoutSeparator = [&](juce::Component& separator)
            {
                auto row = area.removeFromTop(separatorHeight);
                separator.setBounds(row.withTrimmedLeft(labelWidth)
                                       .withTrimmedRight(8)
                                       .withSizeKeepingCentre(row.getWidth() - labelWidth - 8, 1));
                area.removeFromTop(rowGap);
            };

            layoutStandardRow(xCcLabel, xCcEditor, &tipXcc);
            layoutStandardRow(yCcLabel, yCcEditor, &tipYcc);
            layoutStandardRow(adjustCcLabel, adjustCcEditor, &tipAdjustCc);
            layoutStandardRow(adjustModeLabel, adjustModeEditor, &tipAdjustMode);
            layoutStandardRow(adjustMethodLabel, adjustMethodEditor, &tipAdjustMethod);
            layoutSeparator(separatorAfterAdjustMethod);

            {
                auto row = area.removeFromTop(rowHeight);
                toleranceCcLabel.setBounds(row.removeFromLeft(labelWidth));
                toleranceCcEditor.setBounds(row.removeFromLeft(fieldWidth));
                row.removeFromLeft(infoGap);

                auto infoArea = row.removeFromLeft(infoWidth);
                auto topHalf = infoArea.removeFromTop(14);
                auto bottomHalf = infoArea.removeFromTop(14);

                currentToleranceInfoLabel.setBounds(topHalf);
                currentToleranceLineLabel.setBounds(bottomHalf);

                area.removeFromTop(rowGap);
            }

            layoutStandardRow(sensitivityCcLabel, sensitivityCcEditor, &tipSensitivityCc);
            layoutStandardRow(adjustSensitivityLabel, adjustSensitivityEditor, &tipAdjustSensitivity);
            layoutSeparator(separatorAfterAdjustSensitivity);

            {
                auto combinedArea = area.removeFromTop(rowHeight * 2 + rowGap);
                auto combinedBounds = combinedArea;

                auto topRow = combinedArea.removeFromTop(rowHeight);
                xWeightLabel.setBounds(topRow.removeFromLeft(labelWidth));
                xWeightEditor.setBounds(topRow.removeFromLeft(fieldWidth));

                combinedArea.removeFromTop(rowGap);

                auto secondRow = combinedArea.removeFromTop(rowHeight);
                yWeightLabel.setBounds(secondRow.removeFromLeft(labelWidth));
                yWeightEditor.setBounds(secondRow.removeFromLeft(fieldWidth));

                tipWeights.setBounds(combinedBounds.getX() + labelWidth + fieldWidth + infoGap,
                                     combinedBounds.getY(),
                                     infoWidth,
                                     rowHeight * 2 + rowGap);

                area.removeFromTop(rowGap);
            }

            layoutSeparator(separatorAfterWeights);
            layoutStandardRow(overlayTransparencyLabel, overlayTransparencyEditor, &tipOverlay);
            layoutStandardRow(pointSizeLabel, pointSizeEditor, &tipPointSize);

            {
                auto row = area.removeFromTop(rowHeight);
                showCrosshairLabel.setBounds(row.removeFromLeft(labelWidth));
                showCrosshairToggle.setBounds(row.removeFromLeft(24));
                row.removeFromLeft(infoGap);
                tipShowCrosshair.setBounds(row.removeFromLeft(infoWidth));
                area.removeFromTop(rowGap);
            }

            {
                auto row = area.removeFromTop(rowHeight);
                crosshairRgbaLabel.setBounds(row.removeFromLeft(labelWidth));
                crosshairColourREditor.setBounds(row.removeFromLeft(rgbFieldWidth));
                row.removeFromLeft(4);
                crosshairColourGEditor.setBounds(row.removeFromLeft(rgbFieldWidth));
                row.removeFromLeft(4);
                crosshairColourBEditor.setBounds(row.removeFromLeft(rgbFieldWidth));
                row.removeFromLeft(4);
                crosshairColourAEditor.setBounds(row.removeFromLeft(rgbFieldWidth));
                row.removeFromLeft(infoGap);
                tipCrosshairRgba.setBounds(row.removeFromLeft(infoWidth));
                area.removeFromTop(rowGap);
            }

            {
                auto combinedArea = area.removeFromTop(rowHeight * 2 + rowGap);
                auto combinedBounds = combinedArea;

                auto topRow = combinedArea.removeFromTop(rowHeight);
                pointRgbLabel.setBounds(topRow.removeFromLeft(labelWidth));
                pointColourREditor.setBounds(topRow.removeFromLeft(rgbFieldWidth));
                topRow.removeFromLeft(4);
                pointColourGEditor.setBounds(topRow.removeFromLeft(rgbFieldWidth));
                topRow.removeFromLeft(4);
                pointColourBEditor.setBounds(topRow.removeFromLeft(rgbFieldWidth));

                combinedArea.removeFromTop(rowGap);

                auto secondRow = combinedArea.removeFromTop(rowHeight);
                previewRgbLabel.setBounds(secondRow.removeFromLeft(labelWidth));
                previewColourREditor.setBounds(secondRow.removeFromLeft(rgbFieldWidth));
                secondRow.removeFromLeft(4);
                previewColourGEditor.setBounds(secondRow.removeFromLeft(rgbFieldWidth));
                secondRow.removeFromLeft(4);
                previewColourBEditor.setBounds(secondRow.removeFromLeft(rgbFieldWidth));

                tipRgb.setBounds(combinedBounds.getX() + labelWidth + (rgbFieldWidth * 3) + 8 + 8,
                                 combinedBounds.getY(),
                                 infoWidth,
                                 rowHeight * 2 + rowGap);

                area.removeFromTop(rowGap);
            }
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF4A4A4A));

            auto panelBounds = getLocalBounds().withTrimmedLeft(6)
                                               .withTrimmedTop(6)
                                               .withTrimmedRight(6)
                                               .withTrimmedBottom(10);
            panelBounds.removeFromBottom(0);

            g.setColour(juce::Colour(0xFF575757));
            g.fillRect(panelBounds);

            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawRect(panelBounds, 1);

            g.setColour(juce::Colours::white.withAlpha(0.16f));

            for (auto* separator : { &separatorAfterAdjustMethod,
                                     &separatorAfterAdjustSensitivity,
                                     &separatorAfterWeights })
            {
                auto bounds = separator->getBounds();

                if (!bounds.isEmpty())
                    g.fillRect(bounds.withHeight(1));
            }
        }

    private:
        class ScrollableTextEditor : public juce::TextEditor
        {
        public:
            using juce::TextEditor::TextEditor;

            void configureAsInteger(int defaultValueIn, int minValueIn, int maxValueIn)
            {
                isFloat = false;
                defaultIntValue = defaultValueIn;
                minIntValue = minValueIn;
                maxIntValue = maxValueIn;
            }

            void configureAsFloat(float defaultValueIn, float minValueIn, float maxValueIn, int decimalsIn = 3)
            {
                isFloat = true;
                defaultFloatValue = defaultValueIn;
                minFloatValue = minValueIn;
                maxFloatValue = maxValueIn;
                floatDecimals = decimalsIn;
            }

            void clampCurrentText()
            {
                auto text = getText().trim();

                if (isFloat)
                {
                    auto value = text.isEmpty() ? defaultFloatValue : text.getFloatValue();
                    value = juce::jlimit(minFloatValue, maxFloatValue, value);
                    setText(juce::String(value, floatDecimals), juce::dontSendNotification);
                }
                else
                {
                    auto value = text.isEmpty() ? defaultIntValue : text.getIntValue();
                    value = juce::jlimit(minIntValue, maxIntValue, value);
                    setText(juce::String(value), juce::dontSendNotification);
                }
            }

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override
            {
                juce::ignoreUnused(event);

                if (wheel.deltaY == 0.0f)
                {
                    juce::TextEditor::mouseWheelMove(event, wheel);
                    return;
                }

                auto text = getText().trim();

                if (isFloat)
                {
                    auto value = text.isEmpty() ? defaultFloatValue : text.getFloatValue();
                    value += (wheel.deltaY > 0.0f ? 0.1f : -0.1f);
                    value = juce::jlimit(minFloatValue, maxFloatValue, value);
                    setText(juce::String(value, floatDecimals), juce::dontSendNotification);
                }
                else
                {
                    auto value = text.isEmpty() ? defaultIntValue : text.getIntValue();
                    value += (wheel.deltaY > 0.0f ? 1 : -1);
                    value = juce::jlimit(minIntValue, maxIntValue, value);
                    setText(juce::String(value), juce::dontSendNotification);
                }
            }

            void focusLost(FocusChangeType cause) override
            {
                clampCurrentText();
                juce::TextEditor::focusLost(cause);
            }

            bool keyPressed(const juce::KeyPress& key) override
            {
                const bool handled = juce::TextEditor::keyPressed(key);

                if (key == juce::KeyPress::returnKey)
                    clampCurrentText();

                return handled;
            }

        private:
            bool isFloat = false;

            int defaultIntValue = 0;
            int minIntValue = 0;
            int maxIntValue = 999;

            float defaultFloatValue = 0.0f;
            float minFloatValue = 0.0f;
            float maxFloatValue = 999.0f;
            int floatDecimals = 3;
        };

        void configureTipLabel(juce::Label& label, const juce::String& text)
        {
            addAndMakeVisible(label);
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centredLeft);
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            label.setFont(juce::Font(juce::FontOptions(12.0f)));
        }

        void configureBaseEditor(ScrollableTextEditor& ed)
        {
            addAndMakeVisible(ed);
            ed.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF33404A));
            ed.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::lightgrey.withAlpha(0.35f));
        }

        void configureCcEditor(ScrollableTextEditor& ed, int value)
        {
            configureBaseEditor(ed);
            ed.configureAsInteger(value, 0, 127);
            ed.setText(juce::String(juce::jlimit(0, 127, value)), juce::dontSendNotification);
            ed.setInputRestrictions(3, "0123456789");
        }

        void configureWeightEditor(ScrollableTextEditor& ed, float value)
        {
            configureBaseEditor(ed);
            ed.configureAsFloat(value, 0.001f, 100.0f, 3);
            ed.setText(juce::String(juce::jlimit(0.001f, 100.0f, value), 3), juce::dontSendNotification);
            ed.setInputRestrictions(0, "0123456789.");
        }

        void configureIntegerEditor(ScrollableTextEditor& ed, int value, int minValue = 0, int maxValue = 255)
        {
            configureBaseEditor(ed);
            ed.configureAsInteger(value, minValue, maxValue);
            ed.setText(juce::String(juce::jlimit(minValue, maxValue, value)), juce::dontSendNotification);

            const int maxDigits = juce::jmax(3, juce::String(maxValue).length());
            ed.setInputRestrictions(maxDigits, "0123456789");
        }

        static int parseInt(const juce::String& text, int fallback, int minValue, int maxValue)
        {
            auto value = text.getIntValue();
            if (text.trim().isEmpty())
                value = fallback;

            return juce::jlimit(minValue, maxValue, value);
        }

        static float parseFloat(const juce::String& text, float fallback, float minValue, float maxValue)
        {
            auto trimmed = text.trim();
            auto value = trimmed.isEmpty() ? fallback : trimmed.getFloatValue();
            return juce::jlimit(minValue, maxValue, value);
        }

        static juce::String getAdjustModeDescription(int mode)
        {
            switch (mode)
            {
                case 2:
                    return "Relative Mode 1:\n 127 = increment, 1 = decrement.";

                case 3:
                    return "Relative Mode 2:  65 = increment,\n 64 = no change, 63 = decrement.";

                case 4:
                    return "Relative Mode 3:  17 = increment,\n 16 = no change, 15 = decrement.";

                case 1:
                default:
                    return "Absolute Mode:  Uses delta between\n  successive CC values.";
            }
        }

        static juce::String getAdjustMethodDescription(int method)
        {
            switch (method)
            {
                case 2:
                    return "Method 2:  Uses vertical mouse drag\n to adjust parameters.";

                case 1:
                default:
                    return "Method 1:  Uses the mouse wheel\n to adjust parameters.";
            }
        }

        void updateAdjustModeTip()
        {
            tipAdjustMode.setText(getAdjustModeDescription(parseInt(adjustModeEditor.getText(), 1, 1, 4)),
                                  juce::dontSendNotification);
            tipAdjustMode.repaint();
        }

        void updateAdjustMethodTip()
        {
            const int method = parseInt(adjustMethodEditor.getText(), 1, 1, 2);
            adjustMethodEditor.setText(juce::String(method), juce::dontSendNotification);
            tipAdjustMethod.setText(getAdjustMethodDescription(method),
                                    juce::dontSendNotification);
            tipAdjustMethod.repaint();
        }

        AppSettings& settings;
        float currentTolerance = 30.0f;

        juce::Label xCcLabel, yCcLabel, adjustCcLabel, adjustModeLabel, toleranceCcLabel, sensitivityCcLabel, adjustMethodLabel;
        juce::Label xWeightLabel, yWeightLabel, adjustSensitivityLabel;
        juce::Label overlayTransparencyLabel, pointSizeLabel, showCrosshairLabel;
        juce::Label pointRgbLabel, previewRgbLabel, crosshairRgbaLabel;

        juce::Label currentToleranceInfoLabel, currentToleranceLineLabel;

        juce::Label tipXcc, tipYcc, tipAdjustCc, tipAdjustMode, tipAdjustSensitivity, tipSensitivityCc, tipAdjustMethod;
        juce::Label tipWeights, tipOverlay, tipPointSize, tipShowCrosshair, tipRgb, tipCrosshairRgba;

        ScrollableTextEditor xCcEditor, yCcEditor, adjustCcEditor, toleranceCcEditor, sensitivityCcEditor;
        class AdjustModeEditor final : public ScrollableTextEditor
        {
        public:
            explicit AdjustModeEditor(PointerControlSettingsComponent& ownerIn)
                : owner(ownerIn)
            {
            }

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override
            {
                ScrollableTextEditor::mouseWheelMove(event, wheel);
                owner.updateAdjustModeTip();
            }

            void focusLost(FocusChangeType cause) override
            {
                ScrollableTextEditor::focusLost(cause);
                owner.updateAdjustModeTip();
            }

            bool keyPressed(const juce::KeyPress& key) override
            {
                const bool handled = ScrollableTextEditor::keyPressed(key);
                owner.updateAdjustModeTip();
                return handled;
            }

        private:
            PointerControlSettingsComponent& owner;
        };

        AdjustModeEditor adjustModeEditor { *this };
        
        class AdjustMethodEditor final : public ScrollableTextEditor
        {
        public:
            explicit AdjustMethodEditor(PointerControlSettingsComponent& ownerIn)
                : owner(ownerIn)
            {
            }

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override
            {
                ScrollableTextEditor::mouseWheelMove(event, wheel);
                owner.updateAdjustMethodTip();
            }

            void focusLost(FocusChangeType cause) override
            {
                ScrollableTextEditor::focusLost(cause);
                owner.updateAdjustMethodTip();
            }

            bool keyPressed(const juce::KeyPress& key) override
            {
                const bool handled = ScrollableTextEditor::keyPressed(key);
                owner.updateAdjustMethodTip();
                return handled;
            }

        private:
            PointerControlSettingsComponent& owner;
        };

        AdjustMethodEditor adjustMethodEditor { *this };
        ScrollableTextEditor xWeightEditor, yWeightEditor, adjustSensitivityEditor;
        ScrollableTextEditor overlayTransparencyEditor, pointSizeEditor;
        juce::ToggleButton showCrosshairToggle;

        juce::Component separatorAfterAdjustMethod;
        juce::Component separatorAfterAdjustSensitivity;
        juce::Component separatorAfterWeights;

        ScrollableTextEditor pointColourREditor, pointColourGEditor, pointColourBEditor;
        ScrollableTextEditor previewColourREditor, previewColourGEditor, previewColourBEditor;
        ScrollableTextEditor crosshairColourREditor, crosshairColourGEditor, crosshairColourBEditor, crosshairColourAEditor;
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

    class PointerEditOverlayContent final : public juce::Component
    {
    public:
        explicit PointerEditOverlayContent(MainComponent& ownerIn)
            : owner(ownerIn)
        {
            setInterceptsMouseClicks(true, true);
            setWantsKeyboardFocus(false);
            setMouseClickGrabsKeyboardFocus(false);

            snapRowEnabled = owner.getSettings().getPointerControlSnapXEnabled();
            snapColumnEnabled = owner.getSettings().getPointerControlSnapYEnabled();
        }

        void paint(juce::Graphics& g) override
        {
            const auto overlayAlpha = (juce::uint8) juce::jlimit(0, 150,
                owner.getSettings().getPointerControlOverlayTransparency());
            g.fillAll(juce::Colour::fromRGBA(0, 0, 0, overlayAlpha));

            auto bounds = getLocalBounds();
            auto banner = bounds.removeFromTop(24);

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(banner);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawText("Pointer Control Edit Mode Enabled",
                       banner.reduced(8, 0),
                       juce::Justification::centred);

            if (owner.getSettings().getPointerControlShowCrosshair() && hasHoverPosition)
            {
                const auto crosshairPos = previewActive ? previewPosition : hoverPosition;

                const int crossX = juce::jlimit(0, getWidth()  - 1, (int) std::round(crosshairPos.x));
                const int crossY = juce::jlimit(0, getHeight() - 1, (int) std::round(crosshairPos.y));

                g.setColour(owner.getSettings().getPointerControlCrosshairColour());
                g.fillRect(0, crossY, getWidth(), 1);
                g.fillRect(crossX, 0, 1, getHeight());
            }

            if (auto* tc = owner.getSettingsSafeCurrentTabForOverlay())
            {
                const auto& points = tc->getPointerJumpPoints();

                g.setColour(owner.getSettings().getPointerControlPointColour());

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);
                    const float pointSize = (float) owner.getSettings().getPointerControlPointSize();
                    const float halfSize = pointSize * 0.5f;

                    auto r = juce::Rectangle<float>(point.x - halfSize,
                                                    point.y - halfSize,
                                                    pointSize,
                                                    pointSize);
                    g.fillRect(r);
                }
            }

            if (previewActive)
            {
                const float pointSize = (float) owner.getSettings().getPointerControlPointSize();
                const float previewSize = pointSize + 2.0f;
                const float previewHalfSize = previewSize * 0.5f;

                g.setColour(owner.getSettings().getPointerControlPreviewColour().withAlpha(0.9f));

                auto previewRect = juce::Rectangle<float>(previewPosition.x - previewHalfSize,
                                                          previewPosition.y - previewHalfSize,
                                                          previewSize,
                                                          previewSize);

                g.fillRect(previewRect);
            }

            auto bottomBar = getBottomBarBounds();

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(bottomBar);

            drawSnapButton(g, getSnapXButtonBounds(), "Snap X", snapRowEnabled);
            drawSnapButton(g, getSnapYButtonBounds(), "Snap Y", snapColumnEnabled);

            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
            g.drawText("Placement: " + getSnapModeText(),
                       bottomBar.removeFromRight(160),
                       juce::Justification::centredRight);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            auto* tc = owner.getSettingsSafeCurrentTabForOverlay();

            if (tc == nullptr)
                return;

            constexpr float hitRadius = 6.0f;

            mouseDownPosition = event.position;
            previewPosition = event.position;
            hoverPosition = event.position;
            hasHoverPosition = true;
            previewActive = false;
            pendingDeletePointIndex = -1;
            owner.showPointerOverlayCoordinates(event.position);

            if (getSnapXButtonBounds().contains(event.position.toInt()))
            {
                snapRowEnabled = !snapRowEnabled;
                owner.getSettings().setPointerControlSnapXEnabled(snapRowEnabled);
                repaint();
                return;
            }

            if (getSnapYButtonBounds().contains(event.position.toInt()))
            {
                snapColumnEnabled = !snapColumnEnabled;
                owner.getSettings().setPointerControlSnapYEnabled(snapRowEnabled);
                repaint();
                return;
            }

            if (getBottomBarBounds().contains(event.position.toInt()))
                return;

            if (event.mods.isRightButtonDown())
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Clear all points for this tab");

                menu.showMenuAsync(juce::PopupMenu::Options(),
                                   [tc, this](int result)
                                   {
                                       if (result == 1)
                                       {
                                           tc->clearPointerJumpPoints();
                                           snapAnchor.reset();
                                           repaint();
                                       }
                                   });
                return;
            }

            if (event.mods.isLeftButtonDown())
            {
                pendingDeletePointIndex = tc->findPointerJumpPointAt(event.position, hitRadius);
                previewActive = true;
                repaint();
            }
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (!previewActive)
                return;

            previewPosition = event.position;
            hoverPosition = event.position;
            hasHoverPosition = true;

            if (auto* tc = owner.getSettingsSafeCurrentTabForOverlay())
            {
                const auto snapped = applySnapToPosition(*tc, event.position);
                owner.showPointerOverlayCoordinates(snapped);
            }
            else
            {
                owner.showPointerOverlayCoordinates(event.position);
            }

            repaint();
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            auto* tc = owner.getSettingsSafeCurrentTabForOverlay();

            if (tc == nullptr)
            {
                previewActive = false;
                pendingDeletePointIndex = -1;
                repaint();
                return;
            }

            constexpr float hitRadius = 6.0f;

            if (previewActive)
            {
                const auto releasePosition = event.position;
                const auto snappedReleasePosition = applySnapToPosition(*tc, releasePosition);

                if (pendingDeletePointIndex >= 0)
                {
                    const int releaseIndex = tc->findPointerJumpPointAt(releasePosition, hitRadius);

                    if (releaseIndex == pendingDeletePointIndex)
                    {
                        tc->removePointerJumpPointAtIndex(releaseIndex);
                    }
                    else
                    {
                        tc->addPointerJumpPoint(snappedReleasePosition);
                    }
                }
                else
                {
                    tc->addPointerJumpPoint(snappedReleasePosition);
                }

                owner.showPointerOverlayCoordinates(snappedReleasePosition);
            }

            previewActive = false;
            pendingDeletePointIndex = -1;
            repaint();
        }

        void mouseMove(const juce::MouseEvent& event) override
        {
            hoverPosition = event.position;
            hasHoverPosition = true;

            auto* tc = owner.getSettingsSafeCurrentTabForOverlay();
            if (tc == nullptr)
            {
                owner.showPointerOverlayCoordinates(event.position);
                repaint();
                return;
            }

            constexpr float hitRadius = 6.0f;
            const int hoveredIndex = tc->findPointerJumpPointAt(event.position, hitRadius);

            if (hoveredIndex >= 0)
            {
                auto hoveredPoint = tc->getPointerJumpPoints().getReference(hoveredIndex);
                owner.showPointerOverlayCoordinates(event.position, &hoveredPoint);
            }
            else
            {
                owner.showPointerOverlayCoordinates(event.position);
            }

            repaint();
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            hasHoverPosition = false;
            owner.clearPointerOverlayCoordinates();
            repaint();
        }

    private:
        juce::Rectangle<int> getBottomBarBounds() const
        {
            auto area = getLocalBounds();
            return area.removeFromBottom(34).reduced(8, 4);
        }

        juce::Rectangle<int> getSnapXButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            return bar.removeFromLeft(90);
        }

        juce::Rectangle<int> getSnapYButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(98);
            return bar.removeFromLeft(90);
        }

        juce::String getSnapModeText() const
        {
            if (snapRowEnabled && snapColumnEnabled)
                return "Snap X + Y";

            if (snapRowEnabled)
                return "Snap X";

            if (snapColumnEnabled)
                return "Snap Y";

            return "Free";
        }

        void drawSnapButton(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const juce::String& text,
                            bool isActive)
        {
            g.setColour(isActive ? juce::Colour(0xFF3E6FB0)
                                 : juce::Colour(0xFF2A2A2A));
            g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

            g.setColour(isActive ? juce::Colours::white
                                 : juce::Colours::lightgrey);
            g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);

            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText(text, bounds, juce::Justification::centred);
        }

        juce::Point<float> applySnapToPosition(PluginTabComponent& tc,
                                               juce::Point<float> position) const
        {
            constexpr float rowTolerance = 14.0f;
            constexpr float columnTolerance = 14.0f;

            const auto& points = tc.getPointerJumpPoints();

            if (points.isEmpty() || (!snapRowEnabled && !snapColumnEnabled))
                return position;

            // Original "Snap X" behavior: snap to an existing row (adjust Y)
            if (snapRowEnabled)
            {
                float bestDelta = rowTolerance + 1.0f;
                float snappedY = position.y;
                bool matchedExistingRow = false;

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);
                    const float delta = std::abs(point.y - position.y);

                    if (delta <= rowTolerance && delta < bestDelta)
                    {
                        bestDelta = delta;
                        snappedY = point.y;
                        matchedExistingRow = true;
                    }
                }

                if (matchedExistingRow)
                    position.y = snappedY;
            }

            // Original "Snap Y" behavior: snap to an existing column (adjust X)
            if (snapColumnEnabled)
            {
                float bestDelta = columnTolerance + 1.0f;
                float snappedX = position.x;
                bool matchedExistingColumn = false;

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);
                    const float delta = std::abs(point.x - position.x);

                    if (delta <= columnTolerance && delta < bestDelta)
                    {
                        bestDelta = delta;
                        snappedX = point.x;
                        matchedExistingColumn = true;
                    }
                }

                if (matchedExistingColumn)
                    position.x = snappedX;
            }

            return position;
        }

        MainComponent& owner;
        bool previewActive = false;
        juce::Point<float> previewPosition;
        juce::Point<float> hoverPosition;
        bool hasHoverPosition = false;
        int pendingDeletePointIndex = -1;
        juce::Point<float> mouseDownPosition;
        bool snapRowEnabled = false;
        bool snapColumnEnabled = false;
        juce::Optional<juce::Point<float>> snapAnchor;
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

class MainComponent::PointerEditOverlayWindow final : public juce::DocumentWindow
{
public:
    explicit PointerEditOverlayWindow(MainComponent& ownerIn)
        : juce::DocumentWindow("PointerEditOverlay",
                               juce::Colours::transparentBlack,
                               0),
          owner(ownerIn)
    {
        setUsingNativeTitleBar(false);
        setOpaque(false);
        setResizable(false, false);
        setAlwaysOnTop(true);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        setTitleBarHeight(0);
        setContentOwned(new PointerEditOverlayContent(owner), true);
    }

    void closeButtonPressed() override
    {
    }

    void refresh()
    {
        if (auto* content = getContentComponent())
            content->repaint();
    }

private:
    MainComponent& owner;
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
    tabs.setTabBarDepth(26);
    tabs.getTabbedButtonBar().setMinimumTabScaleFactor(1.0f);
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

    routingView.onSetPointerAdjustMethodOverride = [this](int tabIndex, int methodOverride)
    {
        setPointerAdjustMethodOverrideForTab(tabIndex, methodOverride);
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

    addEmptyTab();

    refreshRoutingView();

    pointerControl.setSnapWeights(settings.getPointerControlXSnapWeight(),
                                  settings.getPointerControlYSnapWeight());

    pointerControl.setLaneTolerance(30.0f);

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

    midiEngine.onMidiMessageReceived = [this](const MidiEngine::IncomingMidiMessage& incoming)
    {
        if (incoming.message.isController())
        {
            const int cc = incoming.message.getControllerNumber();
            const int value = incoming.message.getControllerValue();

            refreshPointerControlTarget();

            const int pointerXcc = settings.getPointerControlXccNumber();
            const int pointerYcc = settings.getPointerControlYccNumber();
            const int pointerAdjustCc = settings.getPointerControlAdjustCcNumber();
            const int pointerToleranceCc = settings.getPointerControlToleranceCcNumber();
            const int pointerSensitivityCc = settings.getPointerControlSensitivityCcNumber();

            if (cc == pointerXcc)
            {
                pointerControl.panX(value);
            }
            else if (cc == pointerYcc)
            {
                pointerControl.panY(value);
            }
            else if (cc == pointerAdjustCc)
            {
                const int adjustMode = settings.getPointerControlAdjustCcMode();
                int delta = 0;

                if (adjustMode == 2)
                {
                    // Relative 1/127
                    if (value == 127)
                        delta = -1;
                    else if (value == 1)
                        delta = 1;
                }
                else if (adjustMode == 3)
                {
                    // Relative 63/64/65
                    if (value == 63)
                        delta = -1;
                    else if (value == 65)
                        delta = 1;
                }
                else if (adjustMode == 4)
                {
                    // Relative 15/16/17
                    if (value == 15)
                        delta = -1;
                    else if (value == 17)
                        delta = 1;
                }
                else
                {
                    if (lastPointerAdjustCcValue >= 0)
                    {
                        delta = value - lastPointerAdjustCcValue;

                        // Handle wraparound for endless encoders a bit better
                        if (delta > 64)
                            delta -= 128;
                        else if (delta < -64)
                            delta += 128;
                    }

                    lastPointerAdjustCcValue = value;
                }

                if (delta != 0)
                {
                    const int sensitivity = settings.getPointerControlAdjustSensitivity();

                    int adjustMethod = settings.getPointerControlAdjustMethod();

                    if (auto* tc = getTabComponent(tabs.getCurrentTabIndex()))
                    {
                        const int methodOverride = tc->getPointerAdjustMethodOverride();

                        if (methodOverride == 1)
                            adjustMethod = 1;
                        else if (methodOverride == 2)
                            adjustMethod = 2;
                    }

                    const int direction = (delta > 0 ? 1 : -1);
                    const int repeatCount = std::abs(delta) * sensitivity;

                    ++debugPointerAdjustEventCount;
                    debugPointerAdjustRepeatTotal += repeatCount;

                    const double nowMs = juce::Time::getMillisecondCounterHiRes();

                    if (settings.getAdvancedDebugLoggingEnabled()
                        && (nowMs - debugLastPointerAdjustLogMs >= 250.0))
                    {
                        DebugLog::writeAdvanced("[PointerControl] adjust burst"
                                                " | eventsInWindow=" + juce::String(debugPointerAdjustEventCount)
                                                + " | totalRepeatsInWindow=" + juce::String(debugPointerAdjustRepeatTotal)
                                                + " | currentDelta=" + juce::String(delta)
                                                + " | sensitivity=" + juce::String(sensitivity)
                                                + " | repeatCount=" + juce::String(repeatCount)
                                                + " | method=" + juce::String(adjustMethod)
                                                + " | ccValue=" + juce::String(value));
                        debugPointerAdjustEventCount = 0;
                        debugPointerAdjustRepeatTotal = 0;
                        debugLastPointerAdjustLogMs = nowMs;
                    }

                    if (adjustMethod == 2)
                    {
                        pointerControl.dragAdjust(direction * repeatCount);
                    }
                    else
                    {
                        for (int i = 0; i < repeatCount; ++i)
                            pointerControl.wheelAdjust(direction);
                    }
                }

                if (adjustMode != 1)
                    lastPointerAdjustCcValue = -1;
            }
            else if (cc == pointerToleranceCc)
            {
                if (lastPointerToleranceCcValue >= 0)
                {
                    int delta = value - lastPointerToleranceCcValue;

                    if (delta > 64)
                        delta -= 128;
                    else if (delta < -64)
                        delta += 128;

                    if (delta != 0)
                    {
                        float tolerance = pointerControl.getLaneTolerance() + (float) delta;
                        tolerance = juce::jlimit(1.0f, 128.0f, tolerance);

                        pointerControl.setLaneTolerance(tolerance);

                        if (auto* tc = getTabComponent(tabs.getCurrentTabIndex()))
                            tc->setPointerLaneTolerance(tolerance);

                        refreshPointerControlTarget();

                        showTemporaryStatusMessage("PolyHostInterface  |  Pointer Tolerance: "
                                                   + juce::String(tolerance, 1),
                                                   1800);
                    }
                }

                lastPointerToleranceCcValue = value;
            }
            else if (cc == pointerSensitivityCc)
            {
                const int mappedSensitivity = juce::jlimit(1, 20,
                    1 + (value * 19) / 127);

                settings.setPointerControlAdjustSensitivity(mappedSensitivity);

                showTemporaryStatusMessage("PolyHostInterface  |  Adjust Sensitivity: "
                                           + juce::String(mappedSensitivity),
                                           1800);
            }
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

    startTimerHz(5);

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
    destroyPointerEditOverlay();
    stopTimer();
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

            tab.pointerLaneTolerance = tc->getPointerLaneTolerance();
            tab.pointerAdjustMethodOverride = tc->getPointerAdjustMethodOverride();

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

            tc->setPointerLaneTolerance(tabData.pointerLaneTolerance);
            tc->setPointerAdjustMethodOverride(tabData.pointerAdjustMethodOverride);

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

                    const bool loadedMap = loadPointerMapForTab(*tc);

                    DebugLog::write("[PointerControl] applySessionData: tab="
                                    + juce::String(i)
                                    + " | plugin=" + tc->getPluginName()
                                    + " | loadedMap=" + juce::String(loadedMap ? "true" : "false")
                                    + " | tabPoints=" + juce::String(tc->getPointerJumpPoints().size()));

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

void MainComponent::saveCurrentRoutingWindowSize()
{
    if (!showingRoutingView)
        return;

    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        settings.setRoutingWindowSize(top->getWidth(), top->getHeight());
}

void MainComponent::refreshToolbarButtonStates()
{
    toolbar.repaint();
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

    if (auto* tc = getTabComponent(tabIndex))
    {
        const bool loadedMap = loadPointerMapForTab(*tc);

        DebugLog::write("[PointerControl] handleSuccessfulPluginLoadIntoTab: tab="
                        + juce::String(tabIndex)
                        + " | plugin=" + tc->getPluginName()
                        + " | loadedMap=" + juce::String(loadedMap ? "true" : "false")
                        + " | tabPoints=" + juce::String(tc->getPointerJumpPoints().size()));

        refreshPointerControlTarget();
        tc->repaint();
    }

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
    tc->setPointerLaneTolerance(30.0f);
    configurePluginTabComponent(*tc);
    tc->addChangeListener(this);

    rebuildVisibleTabs();

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

PluginTabComponent* MainComponent::getSettingsSafeCurrentTabForOverlay() const
{
    return getTabComponent(tabs.getCurrentTabIndex());
}

juce::String MainComponent::getPointerMapKeyForPlugin(const PluginTabComponent& tabComponent) const
{
    auto desc = tabComponent.getLoadedPluginDescription();

    auto key = desc.descriptiveName.trim();

    if (key.isEmpty())
        key = desc.name.trim();

    return key;
}

juce::String MainComponent::sanitisePointerMapFileName(const juce::String& name) const
{
    auto result = name.trim();

    if (result.isEmpty())
        return {};

    const juce::String invalidChars = "\\/:*?\"<>|";

    for (auto ch : invalidChars)
        result = result.replaceCharacter(ch, '_');

    while (result.contains("__"))
        result = result.replace("__", "_");

    result = result.trim();

    while (result.startsWithChar('.'))
        result = result.substring(1);

    while (result.endsWithChar('.'))
        result = result.dropLastCharacters(1);

    return result.trim();
}

juce::File MainComponent::getPointerMapFileForPlugin(const PluginTabComponent& tabComponent) const
{
    auto key = getPointerMapKeyForPlugin(tabComponent);

    if (key.isEmpty())
        return {};

    auto safeFileName = sanitisePointerMapFileName(key);

    if (safeFileName.isEmpty())
        return {};

    return AppSettings::getPluginMapsDirectory().getChildFile(safeFileName + ".xml");
}

juce::File MainComponent::getLegacyPointerMapFileForPlugin(const PluginTabComponent& tabComponent) const
{
    auto pluginFile = tabComponent.getPluginFile();

    if (!pluginFile.existsAsFile())
        return {};

    auto pluginFileName = pluginFile.getFileName();

    if (pluginFileName.isEmpty())
        return {};

    return AppSettings::getPluginMapsDirectory().getChildFile(pluginFileName + ".xml");
}

bool MainComponent::loadPointerMapForTab(PluginTabComponent& tabComponent)
{
    auto mapFile = getPointerMapFileForPlugin(tabComponent);
    auto legacyMapFile = getLegacyPointerMapFileForPlugin(tabComponent);

    DebugLog::write("[PointerControl] loadPointerMapForTab: key="
                    + getPointerMapKeyForPlugin(tabComponent)
                    + " | mapFile=" + mapFile.getFullPathName()
                    + " | exists=" + juce::String(mapFile.existsAsFile() ? "true" : "false")
                    + " | legacyMapFile=" + legacyMapFile.getFullPathName()
                    + " | legacyExists=" + juce::String(legacyMapFile.existsAsFile() ? "true" : "false"));

    if (mapFile == juce::File() || !mapFile.existsAsFile())
    {
        if (legacyMapFile != juce::File() && legacyMapFile.existsAsFile())
            mapFile = legacyMapFile;
    }

    if (mapFile == juce::File() || !mapFile.existsAsFile())
    {
        DebugLog::write("[PointerControl] No pointer map found for key="
                        + getPointerMapKeyForPlugin(tabComponent));
        return false;
    }

    auto xml = juce::XmlDocument::parse(mapFile);

    if (xml == nullptr || !xml->hasTagName("PointerMap"))
    {
        DebugLog::write("[PointerControl] Failed to parse pointer map: "
                        + mapFile.getFullPathName());
        return false;
    }

    juce::Array<PointerControl::JumpPoint> points;

    for (auto* pointXml : xml->getChildIterator())
    {
        if (!pointXml->hasTagName("Point"))
            continue;

        PointerControl::JumpPoint point;
        point.x = (float) pointXml->getDoubleAttribute("x", 0.0);
        point.y = (float) pointXml->getDoubleAttribute("y", 0.0);
        points.add(point);
    }

    tabComponent.setPointerJumpPoints(points);

    DebugLog::write("[PointerControl] Loaded pointer map: "
                    + mapFile.getFullPathName()
                    + " | key=" + getPointerMapKeyForPlugin(tabComponent)
                    + " | points=" + juce::String(points.size()));

    return true;
}

bool MainComponent::savePointerMapForTab(const PluginTabComponent& tabComponent) const
{
    if (!tabComponent.hasPlugin())
        return false;

    auto mapFile = getPointerMapFileForPlugin(tabComponent);
    auto mapKey = getPointerMapKeyForPlugin(tabComponent);

    if (mapFile == juce::File())
    {
        DebugLog::write("[PointerControl] Could not resolve pointer map file for plugin");
        return false;
    }

    const auto& points = tabComponent.getPointerJumpPoints();

    if (points.isEmpty())
    {
        if (mapFile.existsAsFile())
            mapFile.deleteFile();

        DebugLog::write("[PointerControl] Deleted empty pointer map: "
                        + mapFile.getFullPathName()
                        + " | key=" + mapKey);

        return true;
    }

    auto desc = tabComponent.getLoadedPluginDescription();

    juce::XmlElement xml("PointerMap");
    xml.setAttribute("pluginKey", mapKey);
    xml.setAttribute("pluginName", desc.name);
    xml.setAttribute("pluginDescriptiveName", desc.descriptiveName);

    for (int i = 0; i < points.size(); ++i)
    {
        const auto& point = points.getReference(i);
        auto* pointXml = xml.createNewChildElement("Point");
        pointXml->setAttribute("x", point.x);
        pointXml->setAttribute("y", point.y);
    }

    const bool ok = xml.writeTo(mapFile, {});

    DebugLog::write("[PointerControl] "
                    + juce::String(ok ? "Saved" : "Failed to save")
                    + " pointer map: "
                    + mapFile.getFullPathName()
                    + " | key=" + mapKey
                    + " | points=" + juce::String(points.size()));

    return ok;
}

void MainComponent::saveAllPointerMaps() const
{
    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
        {
            if (tc->hasPlugin())
                savePointerMapForTab(*tc);
        }
    }
}

void MainComponent::setPointerControlEditMode(bool shouldEnable)
{
    pointerControlEditModeEnabled = shouldEnable;

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
            tc->setPointerControlEditMode(i == tabs.getCurrentTabIndex() && pointerControlEditModeEnabled);
    }

    if (auto* item = toolbar.getItemComponent(toolbarPointerControlEdit))
    {
        item->setToggleState(pointerControlEditModeEnabled, juce::dontSendNotification);
        item->repaint();
    }

    toolbar.repaint();
    updatePointerEditOverlay();
}

void MainComponent::togglePointerControlEditMode()
{
    setPointerControlEditMode(!pointerControlEditModeEnabled);
}

bool MainComponent::isPointerControlEditModeEnabled() const
{
    return pointerControlEditModeEnabled;
}

void MainComponent::hidePointerEditOverlay()
{
    clearPointerOverlayCoordinates();

    if (pointerEditOverlayWindow != nullptr)
        pointerEditOverlayWindow->setVisible(false);
}

void MainComponent::destroyPointerEditOverlay()
{
    clearPointerOverlayCoordinates();

    if (pointerEditOverlayWindow != nullptr)
    {
        pointerEditOverlayWindow->setVisible(false);
        pointerEditOverlayWindow.reset();
    }
}

void MainComponent::updatePointerEditOverlay()
{
    if (!pointerControlEditModeEnabled)
    {
        destroyPointerEditOverlay();
        return;
    }

    auto* thisWindow = findParentComponentOfClass<juce::TopLevelWindow>();
    auto* activeWindow = juce::TopLevelWindow::getActiveTopLevelWindow();

    if (thisWindow == nullptr || activeWindow != thisWindow)
    {
        hidePointerEditOverlay();
        return;
    }

    auto* tc = getTabComponent(tabs.getCurrentTabIndex());

    if (tc == nullptr || !tc->isShowing())
    {
        hidePointerEditOverlay();
        return;
    }

    auto screenBounds = tc->getScreenBounds();

    if (screenBounds.isEmpty())
    {
        hidePointerEditOverlay();
        return;
    }

    const bool wasVisible = (pointerEditOverlayWindow != nullptr && pointerEditOverlayWindow->isVisible());

    if (pointerEditOverlayWindow == nullptr)
        pointerEditOverlayWindow = std::make_unique<PointerEditOverlayWindow>(*this);

    pointerEditOverlayWindow->setBounds(screenBounds);
    pointerEditOverlayWindow->setVisible(true);

    if (!wasVisible)
        pointerEditOverlayWindow->toFront(false);

    pointerEditOverlayWindow->refresh();
}

void MainComponent::timerCallback()
{
    if (pointerControlEditModeEnabled)
        updatePointerEditOverlay();
}

void MainComponent::handleCurrentTabChanged()
{
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        refreshTabAppearance(i);

    refreshRoutingView();
    refreshPointerControlTarget();

    if (auto* tc = getTabComponent(tabs.getCurrentTabIndex()))
    {
        showTemporaryStatusMessage("PolyHostInterface  |  Pointer Tolerance: "
                                   + juce::String(tc->getPointerLaneTolerance(), 1),
                                   1200);
    }

    lastPointerToleranceCcValue = -1;

    for (int i = 0; i < pluginTabs.size(); ++i)
    {
        if (auto* tc = getTabComponent(i))
            tc->setPointerControlEditMode(i == tabs.getCurrentTabIndex() && pointerControlEditModeEnabled);
    }

    updatePointerEditOverlay();

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

bool MainComponent::isCurrentTabWindowSizeDirty() const
{
    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr || !tc->hasSavedWindowBounds())
        return false;

    auto* top = findParentComponentOfClass<juce::DocumentWindow>();
    if (top == nullptr)
        return false;

    auto savedBounds = tc->getSavedWindowBounds();

    return top->getWidth() != savedBounds.getWidth()
        || top->getHeight() != savedBounds.getHeight();
}

juce::Colour MainComponent::getCurrentTabWindowSizeSaveButtonColour() const
{
    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr || !tc->hasSavedWindowBounds())
        return ButtonStyling::defaultBackground();

    if (isCurrentTabWindowSizeDirty())
        return ButtonStyling::destructiveBackground();

    return ButtonStyling::bypassActiveBackground();
}

juce::Colour MainComponent::getCurrentTabWindowSizeClearButtonColour() const
{
    auto* tc = getTabComponent(tabs.getCurrentTabIndex());
    if (tc == nullptr || !tc->hasSavedWindowBounds())
        return ButtonStyling::defaultBackground();

    if (isCurrentTabWindowSizeDirty())
        return ButtonStyling::destructiveBackground();

    return ButtonStyling::bypassActiveBackground();
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

    const int plusButtonWidth = 22;
    const int plusButtonHeight = tabs.getTabBarDepth() - 4;
    const int plusButtonMarginRight = 6;
    const int plusButtonMarginTop = 2;

    addTabButton.setBounds(tabs.getRight() - plusButtonWidth - plusButtonMarginRight,
                           tabs.getY() + plusButtonMarginTop,
                           plusButtonWidth,
                           plusButtonHeight);

    updatePointerEditOverlay();
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
            menu.addItem(menuPointerControlSettings, "Pointer Control Settings...");
            menu.addSeparator();

            juce::PopupMenu pluginRepairsMenu;
            pluginRepairsMenu.addItem(501,
                                      "Auto-Save Preset After Plugin Repair",
                                      true,
                                      settings.getAutoSaveAfterPluginRepair());
            pluginRepairsMenu.addSeparator();
            pluginRepairsMenu.addItem(502, "Add Plugin Scan Folder...");
            pluginRepairsMenu.addItem(503, "Show Plugin Scan Folders");
            pluginRepairsMenu.addItem(504, "Clear Plugin Scan Folders",
                                      settings.getPluginScanFolders().size() > 0);

            menu.addSubMenu("Plugin Repairs", pluginRepairsMenu);
            menu.addSeparator();

            juce::PopupMenu debugMenu;
            debugMenu.addItem(505,
                              "Enable Debug Logging",
                              true,
                              settings.getDebugLoggingEnabled());
            debugMenu.addItem(507,
                              "Enable Advanced Debug Logging",
                              settings.getDebugLoggingEnabled(),
                              settings.getAdvancedDebugLoggingEnabled());
            debugMenu.addItem(506,
                              "Clear Debug Log On Startup",
                              true,
                              settings.getClearDebugLogOnStartup());
            debugMenu.addItem(508, "Clear Debug Log Now");

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
        case menuPointerControlSettings:
            showPointerControlSettingsDialog();
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

    saveAllPointerMaps();

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
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    ++debugMarkDirtyRequestCount;

    if (suppressDirtyMarking)
    {
        if (settings.getAdvancedDebugLoggingEnabled()
            && (nowMs - debugLastMarkDirtyLogMs >= 500.0))
        {
            DebugLog::writeAdvanced("markSessionDirty suppressed"
                                    " | requestsInWindow=" + juce::String(debugMarkDirtyRequestCount)
                                    + " | isLoadingPreset=" + juce::String(isLoadingPreset ? "true" : "false")
                                    + " | suppressDirtyMarking=" + juce::String(suppressDirtyMarking ? "true" : "false")
                                    + " | ignoreDirtyChangesUntilMs=" + juce::String(ignoreDirtyChangesUntilMs)
                                    + " | nowMs=" + juce::String(nowMs));
            debugMarkDirtyRequestCount = 0;
            debugLastMarkDirtyLogMs = nowMs;
        }

        return;
    }

    if (getLoadedPluginTabCount() > 1)
        suppressDirtyForSinglePluginQuickOpen = false;

    if (sessionDocument.isDirty())
    {
        if (settings.getAdvancedDebugLoggingEnabled()
            && (nowMs - debugLastMarkDirtyLogMs >= 500.0))
        {
            DebugLog::writeAdvanced("markSessionDirty skipped (already dirty)"
                                    " | requestsInWindow=" + juce::String(debugMarkDirtyRequestCount)
                                    + " | loadedPluginTabs=" + juce::String(getLoadedPluginTabCount()));
            debugMarkDirtyRequestCount = 0;
            debugLastMarkDirtyLogMs = nowMs;
        }

        return;
    }

    const bool alreadyDirty = sessionDocument.isDirty();

    if (settings.getAdvancedDebugLoggingEnabled()
        && (nowMs - debugLastMarkDirtyLogMs >= 500.0))
    {
        DebugLog::writeAdvanced("markSessionDirty applied"
                                " | requestsInWindow=" + juce::String(debugMarkDirtyRequestCount)
                                + " | sessionAlreadyDirty=" + juce::String(alreadyDirty ? "true" : "false")
                                + " | loadedPluginTabs=" + juce::String(getLoadedPluginTabCount()));
        debugMarkDirtyRequestCount = 0;
        debugLastMarkDirtyLogMs = nowMs;
    }

    if (alreadyDirty)
        return;

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
        tc->clearPointerJumpPoints();
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
    {
        tc->removeChangeListener(this);
        tc->clearPointerJumpPoints();
    }

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
    ids.add(toolbarPointerControlEdit);
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
    ids.add(toolbarPointerControlEdit);
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
        case toolbarRoutingToggle:      return ButtonStyling::Glyphs::routing();
        case toolbarPointerControlEdit: return ButtonStyling::Glyphs::pointerControl();
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

    if (itemId == toolbarPointerControlEdit)
    {
        auto* button = new ButtonStyling::ToolbarIconButton(itemId,
                                               ButtonStyling::Tooltips::pointerControlEditMode(),
                                               getToolbarIconGlyph(itemId),
                                               ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                                               ButtonStyling::defaultButtonWidth(),
                                               [this] { return isPointerControlEditModeEnabled(); });

        button->onClick = [this, button]
        {
            togglePointerControlEditMode();
            button->setToggleState(isPointerControlEditModeEnabled(), juce::dontSendNotification);
            button->repaint();
        };

        button->setToggleState(isPointerControlEditModeEnabled(), juce::dontSendNotification);
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
                                               {},
                                               ButtonStyling::defaultBackground(),
                                               0,
                                               ButtonStyling::defaultIconSize(),
                                               [this]
                                               {
                                                   return getCurrentTabWindowSizeSaveButtonColour();
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
                                               {},
                                               ButtonStyling::defaultBackground(),
                                               0,
                                               ButtonStyling::defaultIconSize(),
                                               [this]
                                               {
                                                   return getCurrentTabWindowSizeClearButtonColour();
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

    if (showingRoutingView)
        refreshMidiDevices();

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
        entry.pointerAdjustMethodOverride = tc->getPointerAdjustMethodOverride();
        if (const auto* midiState = getMidiRoutingStateForTab(tabIndex))
            entry.midiAssignmentCount = midiState->assignedDeviceIdentifiers.size();
        else
            entry.midiAssignmentCount = 0;

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

void MainComponent::showTemporaryStatusMessage(const juce::String& text, int durationMs)
{
    temporaryStatusMessage = text;
    temporaryStatusMessageUntilMs = juce::Time::getMillisecondCounterHiRes() + (double) durationMs;
    statusBar.setText(temporaryStatusMessage, juce::dontSendNotification);
}

void MainComponent::updateStatusBarText()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (temporaryStatusMessageUntilMs > nowMs)
    {
        statusBar.setText(temporaryStatusMessage, juce::dontSendNotification);
        return;
    }

    temporaryStatusMessage.clear();
    temporaryStatusMessageUntilMs = 0.0;

    juce::String midiText = "No MIDI device selected";
    auto openNames = midiEngine.getOpenDeviceNames();

    if (!openNames.isEmpty())
        midiText = "MIDI: " + openNames.joinIntoString(", ");

    juce::String tempoModeText = "Internal Tempo";

    statusBar.setText("PolyHostInterface  |  " + midiText + "  |  " + tempoModeText + "  |  Ready",
                      juce::dontSendNotification);
}

// ===================================
// Pointer Control
// ===================================
void MainComponent::showPointerOverlayCoordinates(juce::Point<float> position,
                                                  PointerControl::JumpPoint* hoveredPoint)
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (temporaryStatusMessageUntilMs > nowMs)
        return;

    showingPointerOverlayCoordinates = true;

    juce::String text = "PolyHostInterface  |  Position: X="
                        + juce::String((int) std::round(position.x))
                        + " Y="
                        + juce::String((int) std::round(position.y));

    if (hoveredPoint != nullptr)
    {
        text += "  |  Current Point: X="
                + juce::String((int) std::round(hoveredPoint->x))
                + " Y="
                + juce::String((int) std::round(hoveredPoint->y));
    }

    statusBar.setText(text, juce::dontSendNotification);
}

void MainComponent::clearPointerOverlayCoordinates()
{
    if (!showingPointerOverlayCoordinates)
        return;

    showingPointerOverlayCoordinates = false;
    updateStatusBarText();
}

void MainComponent::refreshPointerControlTarget()
{
    auto* tc = getTabComponent(tabs.getCurrentTabIndex());

    if (tc == nullptr || !tc->hasPlugin())
    {
        DebugLog::write("[PointerControl] refreshPointerControlTarget: no plugin, clearing target");
        pointerControl.clearTarget();
        return;
    }

    auto targetBounds = tc->getScreenBounds();

    if (targetBounds.isEmpty())
    {
        DebugLog::write("[PointerControl] refreshPointerControlTarget: target bounds empty, keeping previous target");
        return;
    }

    pointerControl.setTargetScreenBounds(targetBounds);
    pointerControl.setSnapWeights(settings.getPointerControlXSnapWeight(),
                                  settings.getPointerControlYSnapWeight());
    pointerControl.setLaneTolerance(tc->getPointerLaneTolerance());

    const auto& jumpPoints = tc->getPointerJumpPoints();
    pointerControl.setJumpPoints(jumpPoints, targetBounds);

    ++debugRefreshPointerTargetCount;
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (settings.getAdvancedDebugLoggingEnabled()
        && (nowMs - debugLastRefreshPointerTargetLogMs >= 500.0))
    {
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget burst"
                                " | callsInWindow=" + juce::String(debugRefreshPointerTargetCount)
                                + " | bounds=" + targetBounds.toString()
                                + " | points=" + juce::String(jumpPoints.size())
                                + " | tabIndex=" + juce::String(tabs.getCurrentTabIndex())
                                + " | plugin=" + tc->getPluginName());
        debugRefreshPointerTargetCount = 0;
        debugLastRefreshPointerTargetLogMs = nowMs;
    }
}

void MainComponent::showPointerControlSettingsDialog()
{
    class PointerControlSettingsDialog final : public juce::Component
    {
    public:
        PointerControlSettingsDialog(MainComponent& ownerIn)
            : owner(ownerIn),
              settingsComponent(ownerIn.getSettings(),
                                ownerIn.pointerControl.getLaneTolerance())
        {
            addAndMakeVisible(settingsComponent);
            addAndMakeVisible(resetButton);
            addAndMakeVisible(okButton);
            addAndMakeVisible(cancelButton);

            resetButton.setButtonText("Reset");
            okButton.setButtonText("OK");
            cancelButton.setButtonText("Cancel");

            resetButton.onClick = [this]
            {
                settingsComponent.resetToDefaults();
            };

            okButton.onClick = [this]
            {
                settingsComponent.apply();
                owner.pointerControl.setSnapWeights(owner.getSettings().getPointerControlXSnapWeight(),
                                                    owner.getSettings().getPointerControlYSnapWeight());
                owner.refreshPointerControlTarget();
                owner.updatePointerEditOverlay();

                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(1);
            };

            cancelButton.onClick = [this]
            {
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(0);
            };
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF4A4A4A));
        }

        void resized() override
        {
            auto area = getLocalBounds().withTrimmedLeft(8)
                                        .withTrimmedRight(8)
                                        .withTrimmedTop(8)
                                        .withTrimmedBottom(8);

            auto buttonRow = area.removeFromBottom(25);

            cancelButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            okButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            resetButton.setBounds(buttonRow.removeFromRight(90));

            area.removeFromBottom(4);
            settingsComponent.setBounds(area);
        }

    private:
        MainComponent& owner;
        PointerControlSettingsComponent settingsComponent;
        juce::TextButton resetButton, okButton, cancelButton;
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new PointerControlSettingsDialog(*this));
    options.dialogTitle = "Pointer Control Settings";
    options.dialogBackgroundColour = juce::Colour(0xFF1E1E1E);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
        options.componentToCentreAround = top;
    else
    options.componentToCentreAround = nullptr;

    auto* dialog = options.launchAsync();
    if (dialog != nullptr)
    {
        dialog->centreWithSize(460, 655);
    }
}

void MainComponent::setPointerAdjustMethodOverrideForTab(int tabIndex, int methodOverride)
{
    if (!juce::isPositiveAndBelow(tabIndex, pluginTabs.size()))
        return;

    if (auto* tc = getTabComponent(tabIndex))
    {
        tc->setPointerAdjustMethodOverride(methodOverride);
        refreshRoutingView();
        markSessionDirty();
    }
}

