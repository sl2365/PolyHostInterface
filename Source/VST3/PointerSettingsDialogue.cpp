#include "PointerSettingsDialogue.h"

#include "AppSettings.h"
#include "DebugLog.h"

namespace
{
    class PointerControlSettingsContent final : public juce::Component
    {
    public:
        explicit PointerControlSettingsContent(AppSettings& settingsIn, float currentToleranceIn)
            : settings(settingsIn),
              currentTolerance(currentToleranceIn)
        {
            auto configureLabel = [this](juce::Label& label,
                                         const juce::String& text,
                                         juce::Justification justification)
            {
                addAndMakeVisible(label);
                label.setText(text, juce::dontSendNotification);
                label.setJustificationType(justification);
                label.setColour(juce::Label::textColourId, juce::Colours::white);
            };

            configureLabel(xCcLabel, "X CC:", juce::Justification::centredRight);
            configureLabel(yCcLabel, "Y CC:", juce::Justification::centredRight);
            configureLabel(adjustCcLabel, "Adjust CC:", juce::Justification::centredRight);
            configureLabel(tabCcLabel, "Tab CC:", juce::Justification::centredRight);
            configureLabel(tabCooldownLabel, "Tab Delay ms:", juce::Justification::centredRight);
            configureLabel(leftMouseCcLabel, "Left Btn CC:", juce::Justification::centredRight);
            configureLabel(middleMouseCcLabel, "Middle Btn CC:", juce::Justification::centredRight);
            configureLabel(rightMouseCcLabel, "Right Btn CC:", juce::Justification::centredRight);
            configureLabel(cursorUpKeyCcLabel, "Up Key CC:", juce::Justification::centredRight);
            configureLabel(cursorDownKeyCcLabel, "Down Key CC:", juce::Justification::centredRight);
            configureLabel(enterKeyCcLabel, "Enter Key CC:", juce::Justification::centredRight);
            configureLabel(adjustModeLabel, "Knob Mode:", juce::Justification::centredRight);
            configureLabel(toleranceCcLabel, "Tolerance CC:", juce::Justification::centredRight);
            configureLabel(sensitivityCcLabel, "Sensitivity CC:", juce::Justification::centredRight);
            configureLabel(adjustSensitivityLabel, "Adjust Sens:", juce::Justification::centredRight);
            configureLabel(adjustMethodLabel, "Adjust Method:", juce::Justification::centredRight);
            configureLabel(dragReturnDelayLabel, "Return Delay ms:", juce::Justification::centredRight);
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
            configureTipLabel(tipTabCc, "Switches between open tabs.");
            configureTipLabel(tipTabCooldown, "Cooldown between relative tab switches. 0 disables. Absolute mode is unaffected.");
            configureTipLabel(tipLeftMouseCc, "Emulates the left mouse button. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipMiddleMouseCc, "Emulates the middle mouse button. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipRightMouseCc, "Emulates the right mouse button. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipCursorUpKeyCc, "Emulates the cursor up arrow key. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipCursorDownKeyCc, "Emulates the cursor down arrow key. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipEnterKeyCc, "Emulates the Enter key. CC value 10+ = down, below 10 = up.");
            configureTipLabel(tipAdjustMode, "");
            configureTipLabel(tipAdjustSensitivity, "Repeats adjust steps per encoder movement. Higher values help stubborn plugin controls.");
            configureTipLabel(tipSensitivityCc, "MIDI CC used to control Adjust Sensitivity live.");
            configureTipLabel(tipAdjustMethod, "");
            configureTipLabel(tipDragReturnDelay, "Delay before the cursor returns to the original drag anchor after knob movement stops.");
            configureTipLabel(tipWeights,
                              "Controls how strongly pointer snapping prefers horizontal vs vertical distance.\n"
                              "Higher X Weight = stronger column matching.\n"
                              "Higher Y Weight = stronger row matching.\n"
                              "Defaults: X=1.0, Y=0.2");
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
            configureCcEditor(tabCcEditor, settings.getPointerControlTabCcNumber());
            configureIntegerEditor(tabCooldownEditor, settings.getPointerControlTabSwitchCooldownMs(), 0, 1000);
            configureCcEditor(leftMouseCcEditor, settings.getPointerControlLeftMouseCcNumber());
            configureCcEditor(middleMouseCcEditor, settings.getPointerControlMiddleMouseCcNumber());
            configureCcEditor(rightMouseCcEditor, settings.getPointerControlRightMouseCcNumber());
            configureCcEditor(cursorUpKeyCcEditor, settings.getPointerControlCursorUpKeyCcNumber());
            configureCcEditor(cursorDownKeyCcEditor, settings.getPointerControlCursorDownKeyCcNumber());
            configureCcEditor(enterKeyCcEditor, settings.getPointerControlEnterKeyCcNumber());
            configureIntegerEditor(adjustModeEditor, settings.getPointerControlAdjustCcMode(), 1, 4);

            configureCcEditor(toleranceCcEditor, settings.getPointerControlToleranceCcNumber());
            configureCcEditor(sensitivityCcEditor, settings.getPointerControlSensitivityCcNumber());
            configureIntegerEditor(adjustSensitivityEditor, settings.getPointerControlAdjustSensitivity(), 1, 20);
            configureIntegerEditor(adjustMethodEditor, settings.getPointerControlAdjustMethod(), 1, 2);
            configureIntegerEditor(dragReturnDelayEditor, settings.getPointerControlDragReturnDelayMs(), 0, 5000);

            updateAdjustModeTip();
            updateAdjustMethodTip();

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

            addAndMakeVisible(resetButton);
            addAndMakeVisible(okButton);
            addAndMakeVisible(cancelButton);

            resetButton.setButtonText("Reset");
            okButton.setButtonText("OK");
            cancelButton.setButtonText("Cancel");

            resetButton.onClick = [this]
            {
                resetToDefaults();
            };

            okButton.onClick = [this]
            {
                apply();
                DebugLog::write("[PointerControl] settings applied");

                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(1);
            };

            cancelButton.onClick = [this]
            {
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(0);
            };

            setSize(460, 965);
        }

        void apply()
        {
            settings.setPointerControlXccNumber(parseInt(xCcEditor.getText(), 24, 0, 127));
            settings.setPointerControlYccNumber(parseInt(yCcEditor.getText(), 25, 0, 127));
            settings.setPointerControlAdjustCcNumber(parseInt(adjustCcEditor.getText(), 26, 0, 127));
            settings.setPointerControlTabCcNumber(parseInt(tabCcEditor.getText(), 27, 0, 127));
            settings.setPointerControlTabSwitchCooldownMs(parseInt(tabCooldownEditor.getText(), 500, 0, 1000));
            settings.setPointerControlLeftMouseCcNumber(parseInt(leftMouseCcEditor.getText(), 52, 0, 127));
            settings.setPointerControlMiddleMouseCcNumber(parseInt(middleMouseCcEditor.getText(), 53, 0, 127));
            settings.setPointerControlRightMouseCcNumber(parseInt(rightMouseCcEditor.getText(), 54, 0, 127));
            settings.setPointerControlCursorUpKeyCcNumber(parseInt(cursorUpKeyCcEditor.getText(), 55, 0, 127));
            settings.setPointerControlCursorDownKeyCcNumber(parseInt(cursorDownKeyCcEditor.getText(), 56, 0, 127));
            settings.setPointerControlEnterKeyCcNumber(parseInt(enterKeyCcEditor.getText(), 57, 0, 127));
            settings.setPointerControlAdjustCcMode(parseInt(adjustModeEditor.getText(), 1, 1, 4));
            settings.setPointerControlToleranceCcNumber(parseInt(toleranceCcEditor.getText(), 22, 0, 127));
            settings.setPointerControlSensitivityCcNumber(parseInt(sensitivityCcEditor.getText(), 23, 0, 127));
            settings.setPointerControlAdjustSensitivity(parseInt(adjustSensitivityEditor.getText(), 1, 1, 20));
            settings.setPointerControlAdjustMethod(parseInt(adjustMethodEditor.getText(), 1, 1, 2));
            settings.setPointerControlDragReturnDelayMs(parseInt(dragReturnDelayEditor.getText(), 300, 0, 5000));

            settings.setPointerControlXSnapWeight(parseFloat(xWeightEditor.getText(), 1.0f, 0.001f, 100.0f));
            settings.setPointerControlYSnapWeight(parseFloat(yWeightEditor.getText(), 0.20f, 0.001f, 100.0f));

            settings.setPointerControlOverlayTransparency(parseInt(overlayTransparencyEditor.getText(), 30, 0, 150));
            settings.setPointerControlPointSize(parseInt(pointSizeEditor.getText(), 4, 3, 15));
            settings.setPointerControlShowCrosshair(showCrosshairToggle.getToggleState());

            settings.setPointerControlPointColour(juce::Colour::fromRGB(
                (juce::uint8) parseInt(pointColourREditor.getText(), 80, 0, 255),
                (juce::uint8) parseInt(pointColourGEditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(pointColourBEditor.getText(), 120, 0, 255)));

            settings.setPointerControlPreviewColour(juce::Colour::fromRGB(
                (juce::uint8) parseInt(previewColourREditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(previewColourGEditor.getText(), 210, 0, 255),
                (juce::uint8) parseInt(previewColourBEditor.getText(), 80, 0, 255)));

            settings.setPointerControlCrosshairColour(juce::Colour::fromRGBA(
                (juce::uint8) parseInt(crosshairColourREditor.getText(), 255, 0, 255),
                (juce::uint8) parseInt(crosshairColourGEditor.getText(), 100, 0, 255),
                (juce::uint8) parseInt(crosshairColourBEditor.getText(), 100, 0, 255),
                (juce::uint8) parseInt(crosshairColourAEditor.getText(), 200, 0, 255)));
        }

        void resetToDefaults()
        {
            xCcEditor.setText("24", juce::dontSendNotification);
            yCcEditor.setText("25", juce::dontSendNotification);
            adjustCcEditor.setText("26", juce::dontSendNotification);
            tabCcEditor.setText("27", juce::dontSendNotification);
            tabCooldownEditor.setText("150", juce::dontSendNotification);
            leftMouseCcEditor.setText("52", juce::dontSendNotification);
            middleMouseCcEditor.setText("53", juce::dontSendNotification);
            rightMouseCcEditor.setText("54", juce::dontSendNotification);
            cursorUpKeyCcEditor.setText("55", juce::dontSendNotification);
            cursorDownKeyCcEditor.setText("56", juce::dontSendNotification);
            enterKeyCcEditor.setText("57", juce::dontSendNotification);
            adjustModeEditor.setText("1", juce::dontSendNotification);
            updateAdjustModeTip();
            toleranceCcEditor.setText("22", juce::dontSendNotification);
            sensitivityCcEditor.setText("23", juce::dontSendNotification);
            adjustSensitivityEditor.setText("1", juce::dontSendNotification);
            adjustMethodEditor.setText("1", juce::dontSendNotification);
            dragReturnDelayEditor.setText("300", juce::dontSendNotification);
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
            previewColourBEditor.setText("80", juce::dontSendNotification);

            crosshairColourREditor.setText("255", juce::dontSendNotification);
            crosshairColourGEditor.setText("100", juce::dontSendNotification);
            crosshairColourBEditor.setText("100", juce::dontSendNotification);
            crosshairColourAEditor.setText("200", juce::dontSendNotification);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(16);
            area.removeFromTop(4);

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
            layoutStandardRow(tabCcLabel, tabCcEditor, &tipTabCc);
            layoutStandardRow(tabCooldownLabel, tabCooldownEditor, &tipTabCooldown);
            layoutStandardRow(leftMouseCcLabel, leftMouseCcEditor, &tipLeftMouseCc);
            layoutStandardRow(middleMouseCcLabel, middleMouseCcEditor, &tipMiddleMouseCc);
            layoutStandardRow(rightMouseCcLabel, rightMouseCcEditor, &tipRightMouseCc);
            layoutStandardRow(cursorUpKeyCcLabel, cursorUpKeyCcEditor, &tipCursorUpKeyCc);
            layoutStandardRow(cursorDownKeyCcLabel, cursorDownKeyCcEditor, &tipCursorDownKeyCc);
            layoutStandardRow(enterKeyCcLabel, enterKeyCcEditor, &tipEnterKeyCc);
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
            layoutStandardRow(dragReturnDelayLabel, dragReturnDelayEditor, &tipDragReturnDelay);
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

                const int tipX = combinedBounds.getX() + labelWidth + fieldWidth + infoGap;
                const int tipRightMargin = 4;
                const int tipWidth = combinedBounds.getRight() - tipX - tipRightMargin;

                tipWeights.setBounds(tipX,
                                     combinedBounds.getY(),
                                     juce::jmax(0, tipWidth),
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

            auto buttonRow = area.removeFromBottom(30);
            cancelButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            okButton.setBounds(buttonRow.removeFromRight(90));
            buttonRow.removeFromRight(8);
            resetButton.setBounds(buttonRow.removeFromRight(90));
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF4A4A4A));

            auto panelBounds = getLocalBounds().withTrimmedLeft(10)
                                               .withTrimmedTop(10)
                                               .withTrimmedRight(10)
                                               .withTrimmedBottom(52);

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

                if (! bounds.isEmpty())
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

        juce::Label xCcLabel, yCcLabel, adjustCcLabel, tabCcLabel, tabCooldownLabel, leftMouseCcLabel, middleMouseCcLabel, rightMouseCcLabel, cursorUpKeyCcLabel, cursorDownKeyCcLabel, enterKeyCcLabel, adjustModeLabel, toleranceCcLabel, sensitivityCcLabel, adjustMethodLabel;
        juce::Label xWeightLabel, yWeightLabel, adjustSensitivityLabel, dragReturnDelayLabel;
        juce::Label overlayTransparencyLabel, pointSizeLabel, showCrosshairLabel;
        juce::Label pointRgbLabel, previewRgbLabel, crosshairRgbaLabel;

        juce::Label currentToleranceInfoLabel, currentToleranceLineLabel;

        juce::Label tipXcc, tipYcc, tipAdjustCc, tipTabCc, tipTabCooldown, tipLeftMouseCc, tipMiddleMouseCc, tipRightMouseCc, tipCursorUpKeyCc, tipCursorDownKeyCc, tipEnterKeyCc, tipAdjustMode, tipAdjustSensitivity, tipSensitivityCc, tipAdjustMethod, tipDragReturnDelay;
        juce::Label tipWeights, tipOverlay, tipPointSize, tipShowCrosshair, tipRgb, tipCrosshairRgba;

        ScrollableTextEditor xCcEditor, yCcEditor, adjustCcEditor, tabCcEditor, tabCooldownEditor, leftMouseCcEditor, middleMouseCcEditor, rightMouseCcEditor, cursorUpKeyCcEditor, cursorDownKeyCcEditor, enterKeyCcEditor, toleranceCcEditor, sensitivityCcEditor;
        class AdjustModeEditor final : public ScrollableTextEditor
        {
        public:
            explicit AdjustModeEditor(PointerControlSettingsContent& ownerIn)
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
            PointerControlSettingsContent& owner;
        };

        AdjustModeEditor adjustModeEditor { *this };

        class AdjustMethodEditor final : public ScrollableTextEditor
        {
        public:
            explicit AdjustMethodEditor(PointerControlSettingsContent& ownerIn)
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
            PointerControlSettingsContent& owner;
        };

        AdjustMethodEditor adjustMethodEditor { *this };
        ScrollableTextEditor xWeightEditor, yWeightEditor, adjustSensitivityEditor, dragReturnDelayEditor;
        ScrollableTextEditor overlayTransparencyEditor, pointSizeEditor;
        juce::ToggleButton showCrosshairToggle;

        juce::Component separatorAfterAdjustMethod;
        juce::Component separatorAfterAdjustSensitivity;
        juce::Component separatorAfterWeights;

        ScrollableTextEditor pointColourREditor, pointColourGEditor, pointColourBEditor;
        ScrollableTextEditor previewColourREditor, previewColourGEditor, previewColourBEditor;
        ScrollableTextEditor crosshairColourREditor, crosshairColourGEditor, crosshairColourBEditor, crosshairColourAEditor;

        juce::TextButton resetButton, okButton, cancelButton;
    };
}

namespace PointerSettingsDialogue
{
    void show(AppSettings& appSettings,
              float currentTolerance,
              juce::Component* centreAroundComponent)
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new PointerControlSettingsContent(appSettings, currentTolerance));
        options.dialogTitle = "Pointer Control Settings";
        options.dialogBackgroundColour = juce::Colour(0xFF4A4A4A);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.componentToCentreAround = centreAroundComponent;

        if (auto* dialog = options.launchAsync())
            dialog->centreAroundComponent(centreAroundComponent, 460, 965);
    }
}
