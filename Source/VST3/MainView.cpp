#include "DebugLog.h"
#include "MainView.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
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

            versionLabel.setText("Version " + juce::String(POLYHOST_VERSION_STRING),
                                 juce::dontSendNotification);
            versionLabel.setJustificationType(juce::Justification::centred);
            versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(versionLabel);

            infoLabel.setText("A lightweight tabbed plugin host for:\nVST3.\n\nCLAP 64 and VST2 32-bit bridging planned.\n\nBuilt with JUCE.\n\n- sl23 -",
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

            const int dialogWidth = 360;
            const int topMargin = 20;
            const int bottomMargin = 20;
            const int closeButtonHeight = 30;
            const int closeButtonGap = 12;
            const int titleHeight = 34;
            const int versionHeight = 22;
            const int titleGap = 14;

            auto infoFont = juce::Font(juce::FontOptions(15.0f));
            infoLabel.setFont(infoFont);

            const int textLineCount = juce::jmax(1, juce::StringArray::fromLines(infoLabel.getText()).size());
            const int infoHeight = juce::jmax(80,
                (int) std::ceil(infoFont.getHeight() * 1.35f * (float) textLineCount));

            const int dialogHeight = topMargin
                                   + titleHeight
                                   + versionHeight
                                   + titleGap
                                   + infoHeight
                                   + closeButtonGap
                                   + closeButtonHeight
                                   + bottomMargin;

            setSize(dialogWidth, dialogHeight);
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

            const int closeButtonHeight = 30;
            const int closeButtonGap = 12;
            const int reservedForButton = closeButtonGap + closeButtonHeight;

            infoLabel.setBounds(area.removeFromTop(juce::jmax(80, area.getHeight() - reservedForButton)));
            area.removeFromTop(closeButtonGap);
            closeButton.setBounds(area.removeFromTop(closeButtonHeight).withSizeKeepingCentre(90, closeButtonHeight));
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
        explicit PointerEditOverlayContent(MainView& ownerIn)
            : owner(ownerIn)
        {
            setInterceptsMouseClicks(true, true);
            setWantsKeyboardFocus(false);
            setMouseClickGrabsKeyboardFocus(false);

            snapXEnabled = owner.getAppSettings().getPointerControlSnapXEnabled();
            snapYEnabled = owner.getAppSettings().getPointerControlSnapYEnabled();

            addAndMakeVisible(saveGlobalButton);
            addAndMakeVisible(savePresetButton);
            addAndMakeVisible(clearPresetButton);
            addAndMakeVisible(useGlobalToggle);

            saveGlobalButton.setTooltip("Save current pointer map as a global plugin map");
            savePresetButton.setTooltip("Save current pointer map to the current preset");
            clearPresetButton.setTooltip("Clear preset pointer map so global fallback can be used");

            saveGlobalButton.onHoverStart = [this]
            {
                owner.showTemporaryStatusMessage("Save current pointer map as the global plugin map");
            };

            savePresetButton.onHoverStart = [this]
            {
                owner.showTemporaryStatusMessage("Save current pointer map into the current preset");
            };

            clearPresetButton.onHoverStart = [this]
            {
                owner.showTemporaryStatusMessage("Clear preset pointer map so global fallback can be used");
            };

            saveGlobalButton.onHoverEnd = [] {};
            savePresetButton.onHoverEnd = [] {};
            clearPresetButton.onHoverEnd = [] {};

            useGlobalToggle.setButtonText("Use Global");
            useGlobalToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
            useGlobalToggle.setTooltip("Ignore preset point data and use the global point map instead");

            useGlobalToggle.onStateChange = [this]
            {
                if (useGlobalToggle.isMouseOver(true))
                {
                    owner.showTemporaryStatusMessage(
                        "Ignore preset point data and use the global point map instead");
                }
            };

            useGlobalToggle.onClick = [this]
            {
                auto& core = owner.getProcessor().getCore();
                const int tabIndex = core.getSelectedTabIndex();

                if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                    return;

                core.setTabPreferGlobalPointerMap(tabIndex, useGlobalToggle.getToggleState());

                owner.showTemporaryStatusMessage(useGlobalToggle.getToggleState()
                    ? "Using global pointer map"
                    : "Using preset/global automatic pointer map selection");

                repaint();
            };

            saveGlobalButton.onClick = [this]
            {
                auto& core = owner.getProcessor().getCore();
                const int tabIndex = core.getSelectedTabIndex();

                if (core.saveCurrentPointerMapToGlobalFile(tabIndex))
                    owner.showTemporaryStatusMessage("Global pointer map saved");
                else
                    owner.showTemporaryStatusMessage("Failed to save global pointer map");

                repaint();
            };

            savePresetButton.onClick = [this]
            {
                auto& core = owner.getProcessor().getCore();
                const int tabIndex = core.getSelectedTabIndex();

                if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                    return;

                core.saveCurrentPointerMapToPreset(tabIndex);

                if (owner.saveCurrentTabPointerMapToCurrentPresetFile())
                    owner.showTemporaryStatusMessage("Pointer map saved to current preset");
                else
                    owner.showTemporaryStatusMessage("No preset file exists. Save a preset first.");

                repaint();
            };

            clearPresetButton.onClick = [this]
            {
                auto& core = owner.getProcessor().getCore();
                const int tabIndex = core.getSelectedTabIndex();

                if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                    return;

                const bool hasGlobalFallback = core.tabHasGlobalPointerMap(tabIndex);

                juce::String message =
                    "Clear the preset pointer map for this tab?\n\n"
                    "This removes the preset-specific point map.";

                if (hasGlobalFallback)
                    message += "\n\nAfter clearing, the global point map will be used.";
                else
                    message += "\n\nAfter clearing, no point map will remain active.";

                const bool confirmed = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Clear Preset Pointer Map",
                    message,
                    "Clear Preset",
                    "Cancel",
                    owner.getWindowCenterTarget());

                if (! confirmed)
                    return;

                core.clearCurrentPresetPointerMap(tabIndex);
                owner.showTemporaryStatusMessage("Preset pointer map cleared");
                repaint();
            };
        }

        void resized() override
        {
            saveGlobalButton.setBounds(getSaveGlobalButtonBounds());
            savePresetButton.setBounds(getSavePresetButtonBounds());
            clearPresetButton.setBounds(getClearPresetButtonBounds());
            useGlobalToggle.setBounds(getUseGlobalToggleBounds());
        }

        void paint(juce::Graphics& g) override
        {
            saveGlobalButton.setBounds(getSaveGlobalButtonBounds());
            savePresetButton.setBounds(getSavePresetButtonBounds());
            clearPresetButton.setBounds(getClearPresetButtonBounds());
            useGlobalToggle.setBounds(getUseGlobalToggleBounds());

            auto& core = owner.getProcessor().getCore();
            const int tabIndex = core.getSelectedTabIndex();

            saveGlobalButton.setActiveState(core.tabHasGlobalPointerMap(tabIndex));
            savePresetButton.setActiveState(core.tabHasCustomPointerMap(tabIndex));

            useGlobalToggle.setToggleState(core.getTabPreferGlobalPointerMap(tabIndex),
                                           juce::dontSendNotification);

            const auto overlayAlpha = (juce::uint8) juce::jlimit(0, 150,
                owner.getAppSettings().getPointerControlOverlayTransparency());

            g.fillAll(juce::Colour::fromRGBA(0, 0, 0, overlayAlpha));

            auto bounds = getLocalBounds();
            auto banner = bounds.removeFromTop(24);

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(banner);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawText("Pointer Control Edit Mode",
                       banner.reduced(8, 0),
                       juce::Justification::centred);

            if (owner.getAppSettings().getPointerControlShowCrosshair() && hasHoverPosition)
            {
                const auto crosshairPos = previewActive ? previewPosition : hoverPosition;

                const int crossX = juce::jlimit(0, getWidth() - 1, (int) std::round(crosshairPos.x));
                const int crossY = juce::jlimit(0, getHeight() - 1, (int) std::round(crosshairPos.y));

                g.setColour(owner.getAppSettings().getPointerControlCrosshairColour());
                g.fillRect(0, crossY, getWidth(), 1);
                g.fillRect(crossX, 0, 1, getHeight());
            }

            if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
            {
                const auto& points = core.getTabPointerJumpPoints(tabIndex);

                g.setColour(owner.getAppSettings().getPointerControlPointColour());

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);

                    const float pointSize = (float) owner.getAppSettings().getPointerControlPointSize();
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
                const float pointSize = (float) owner.getAppSettings().getPointerControlPointSize();
                const float previewSize = pointSize + 2.0f;
                const float previewHalfSize = previewSize * 0.5f;

                g.setColour(owner.getAppSettings().getPointerControlPreviewColour().withAlpha(0.9f));

                auto previewRect = juce::Rectangle<float>(previewPosition.x - previewHalfSize,
                                                          previewPosition.y - previewHalfSize,
                                                          previewSize,
                                                          previewSize);
                g.fillRect(previewRect);
            }

            auto bottomBar = getBottomBarBounds();

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(bottomBar);

            drawSnapButton(g, getSnapXButtonBounds(), "Snap X", snapXEnabled);
            drawSnapButton(g, getSnapYButtonBounds(), "Snap Y", snapYEnabled);

            const int selectedTabIndex = tabIndex;
            float currentTolerance = 30.0f;

            if (juce::isPositiveAndBelow(selectedTabIndex, core.getNumTabs()))
                currentTolerance = core.getTabPointerLaneTolerance(selectedTabIndex);

            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));

            const juce::String infoText =
                "Map: " + core.getActivePointerMapSourceText(selectedTabIndex)
                + "  |  Placement: " + getSnapModeText()
                + "  |  Tol (CC"
                + juce::String(owner.getAppSettings().getPointerControlToleranceCcNumber())
                + "): " + juce::String(currentTolerance, 1);

            auto textLayoutBar = bottomBar.reduced(8, 0);
            auto infoArea = textLayoutBar.removeFromRight(360);

            g.drawText(infoText,
                       infoArea,
                       juce::Justification::centredRight,
                       true);
        }

        void mouseMove(const juce::MouseEvent& event) override
        {
            hoverPosition = event.position;
            hasHoverPosition = true;

            const auto mousePos = event.position.toInt();
            const bool overSnapX = getSnapXButtonBounds().contains(mousePos);
            const bool overSnapY = getSnapYButtonBounds().contains(mousePos);

            if (overSnapX)
            {
                if (! hoverShowingSnapXHelp)
                {
                    owner.showTemporaryStatusMessage("Snap X: align new points to the X positions of nearby saved points");
                    hoverShowingSnapXHelp = true;
                    hoverShowingSnapYHelp = false;
                }
            }
            else if (overSnapY)
            {
                if (! hoverShowingSnapYHelp)
                {
                    owner.showTemporaryStatusMessage("Snap Y: align new points to the Y positions of nearby saved points");
                    hoverShowingSnapYHelp = true;
                    hoverShowingSnapXHelp = false;
                }
            }
            else
            {
                hoverShowingSnapXHelp = false;
                hoverShowingSnapYHelp = false;

                owner.showTemporaryStatusMessage("Position: X="
                                                 + juce::String((int) std::round(event.position.x))
                                                 + " Y="
                                                 + juce::String((int) std::round(event.position.y)));
            }

            repaint();
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            hasHoverPosition = false;
            hoverShowingSnapXHelp = false;
            hoverShowingSnapYHelp = false;
            repaint();
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (getSnapXButtonBounds().contains(event.position.toInt()))
            {
                snapXEnabled = !snapXEnabled;
                owner.getAppSettings().setPointerControlSnapXEnabled(snapXEnabled);
                repaint();
                return;
            }

            if (getSnapYButtonBounds().contains(event.position.toInt()))
            {
                snapYEnabled = !snapYEnabled;
                owner.getAppSettings().setPointerControlSnapYEnabled(snapYEnabled);
                repaint();
                return;
            }

            if (getBottomBarBounds().contains(event.position.toInt()))
                return;

            constexpr float hitRadius = 6.0f;

            hoverPosition = event.position;
            hasHoverPosition = true;
            previewPosition = event.position;
            previewActive = false;
            pendingDeletePointIndex = -1;

            if (event.mods.isLeftButtonDown())
            {
                pendingDeletePointIndex = findPointAt(event.position, hitRadius);
                previewActive = true;
                repaint();
            }
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (! previewActive)
                return;

            previewPosition = applySnapToPosition(event.position);
            hoverPosition = event.position;
            hasHoverPosition = true;

            owner.showTemporaryStatusMessage("Position: X="
                                             + juce::String((int) std::round(previewPosition.x))
                                             + " Y="
                                             + juce::String((int) std::round(previewPosition.y)));
            repaint();
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            auto& core = owner.getProcessor().getCore();
            const int tabIndex = core.getSelectedTabIndex();

            if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
            {
                previewActive = false;
                pendingDeletePointIndex = -1;
                repaint();
                return;
            }

            auto points = core.getTabPointerJumpPoints(tabIndex);
            constexpr float hitRadius = 6.0f;

            if (previewActive)
            {
                const auto snappedReleasePosition = applySnapToPosition(event.position);

                if (pendingDeletePointIndex >= 0)
                {
                    const int releaseIndex = findPointAt(event.position, hitRadius);

                    if (releaseIndex == pendingDeletePointIndex
                        && juce::isPositiveAndBelow(releaseIndex, points.size()))
                    {
                        points.remove(releaseIndex);
                    }
                    else
                    {
                        SessionTabData::PointerJumpPoint point;
                        point.x = snappedReleasePosition.x;
                        point.y = snappedReleasePosition.y;
                        points.add(point);
                    }
                }
                else
                {
                    SessionTabData::PointerJumpPoint point;
                    point.x = snappedReleasePosition.x;
                    point.y = snappedReleasePosition.y;
                    points.add(point);
                }

                core.setTabPointerJumpPoints(tabIndex, points);
            }

            previewActive = false;
            pendingDeletePointIndex = -1;
            repaint();
        }

    private:
        class OverlayActionButton final : public juce::Button
        {
        public:
            OverlayActionButton(const juce::String& buttonName,
                                const juce::String& glyphText,
                                const juce::String& labelText,
                                bool destructiveIn = false)
                : juce::Button(buttonName),
                  glyph(glyphText),
                  label(labelText),
                  destructive(destructiveIn)
            {
            }

            void setActiveState(bool shouldBeActive)
            {
                if (active != shouldBeActive)
                {
                    active = shouldBeActive;
                    repaint();
                }
            }

            std::function<void()> onHoverStart;
            std::function<void()> onHoverEnd;

            void mouseEnter(const juce::MouseEvent& event) override
            {
                juce::Button::mouseEnter(event);

                if (onHoverStart)
                    onHoverStart();
            }

            void mouseExit(const juce::MouseEvent& event) override
            {
                juce::Button::mouseExit(event);

                if (onHoverEnd)
                    onHoverEnd();
            }

            void paintButton(juce::Graphics& g,
                             bool isMouseOverButton,
                             bool isButtonDown) override
            {
                auto bounds = getLocalBounds().toFloat();

                auto fill = destructive ? juce::Colour(0xFF6A2E2E)
                                        : (active ? juce::Colour(0xFF2E7D32)
                                                  : juce::Colour(0xFF2A2A2A));

                if (isButtonDown)
                    fill = fill.brighter(0.15f);
                else if (isMouseOverButton)
                    fill = fill.brighter(0.08f);

                g.setColour(fill);
                g.fillRoundedRectangle(bounds, 6.0f);

                g.setColour(fill.brighter(0.25f).withAlpha(0.65f));
                g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions("Segoe Fluent Icons", 14.0f, juce::Font::plain)));

                if (label.isEmpty())
                {
                    g.drawText(glyph,
                               getLocalBounds().translated(1, 1),
                               juce::Justification::centred,
                               false);
                }
                else
                {
                    auto textArea = getLocalBounds().reduced(8, 0);
                    auto glyphArea = textArea.removeFromLeft(18);
                    auto labelArea = textArea;

                    g.drawText(glyph,
                               glyphArea,
                               juce::Justification::centredLeft,
                               false);

                    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                    g.drawText(label,
                               labelArea,
                               juce::Justification::centredLeft,
                               false);
                }
            }

        private:
            juce::String glyph;
            juce::String label;
            bool destructive = false;
            bool active = false;
        };

        juce::Rectangle<int> getBottomBarBounds() const
        {
            auto area = getLocalBounds();
            auto bottom = area.removeFromBottom(34);
            bottom.reduce(0, 4);
            return bottom;
        }

        juce::Rectangle<int> getSnapXButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(5);
            auto bounds = bar.removeFromLeft(66);
            return bounds.reduced(0, 3);
        }

        juce::Rectangle<int> getSnapYButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(76);
            auto bounds = bar.removeFromLeft(66);
            return bounds.reduced(0, 3);
        }

        juce::Rectangle<int> getSaveGlobalButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(152);
            auto bounds = bar.removeFromLeft(76);
            return bounds.reduced(0, 2);
        }

        juce::Rectangle<int> getSavePresetButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(233);
            auto bounds = bar.removeFromLeft(76);
            return bounds.reduced(0, 2);
        }

        juce::Rectangle<int> getClearPresetButtonBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(314);
            auto bounds = bar.removeFromLeft(24);
            return bounds.reduced(0, 2);
        }

        juce::Rectangle<int> getUseGlobalToggleBounds() const
        {
            auto bar = getBottomBarBounds();
            bar.removeFromLeft(343);
            auto bounds = bar.removeFromLeft(92);
            return bounds.reduced(0, 2);
        }

        juce::String getSnapModeText() const
        {
            if (snapXEnabled && snapYEnabled)
                return "Snap X + Y";
            if (snapXEnabled)
                return "Snap X";
            if (snapYEnabled)
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

        juce::Point<float> applySnapToPosition(juce::Point<float> position) const
        {
            auto& core = owner.getProcessor().getCore();
            const int tabIndex = core.getSelectedTabIndex();

            if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                return position;

            const auto& points = core.getTabPointerJumpPoints(tabIndex);

            constexpr float rowTolerance = 14.0f;
            constexpr float columnTolerance = 14.0f;

            if (points.isEmpty() || (!snapXEnabled && !snapYEnabled))
                return position;

            if (snapXEnabled)
            {
                float bestDelta = rowTolerance + 1.0f;
                float snappedY = position.y;
                bool matched = false;

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);
                    const float delta = std::abs(point.y - position.y);

                    if (delta <= rowTolerance && delta < bestDelta)
                    {
                        bestDelta = delta;
                        snappedY = point.y;
                        matched = true;
                    }
                }

                if (matched)
                    position.y = snappedY;
            }

            if (snapYEnabled)
            {
                float bestDelta = columnTolerance + 1.0f;
                float snappedX = position.x;
                bool matched = false;

                for (int i = 0; i < points.size(); ++i)
                {
                    const auto& point = points.getReference(i);
                    const float delta = std::abs(point.x - position.x);

                    if (delta <= columnTolerance && delta < bestDelta)
                    {
                        bestDelta = delta;
                        snappedX = point.x;
                        matched = true;
                    }
                }

                if (matched)
                    position.x = snappedX;
            }

            return position;
        }

        int findPointAt(juce::Point<float> position, float hitRadius) const
        {
            auto& core = owner.getProcessor().getCore();
            const int tabIndex = core.getSelectedTabIndex();

            if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                return -1;

            const auto& points = core.getTabPointerJumpPoints(tabIndex);
            const float hitRadiusSq = hitRadius * hitRadius;

            for (int i = 0; i < points.size(); ++i)
            {
                const auto& point = points.getReference(i);
                const float dx = point.x - position.x;
                const float dy = point.y - position.y;
                const float distSq = dx * dx + dy * dy;

                if (distSq <= hitRadiusSq)
                    return i;
            }

            return -1;
        }

        MainView& owner;
        OverlayActionButton saveGlobalButton { "SaveGlobal",
                                               ButtonStyling::Glyphs::save(),
                                               "Global" };
        OverlayActionButton savePresetButton { "SavePreset",
                                               ButtonStyling::Glyphs::save(),
                                               "Preset" };
        OverlayActionButton clearPresetButton { "ClearPreset",
                                                ButtonStyling::Glyphs::close(),
                                                "",
                                                true };
        juce::ToggleButton useGlobalToggle;
        juce::Point<float> hoverPosition;
        juce::Point<float> previewPosition;
        bool hasHoverPosition = false;
        bool previewActive = false;
        int pendingDeletePointIndex = -1;
        bool snapXEnabled = false;
        bool snapYEnabled = false;
        bool hoverShowingSnapXHelp = false;
        bool hoverShowingSnapYHelp = false;
    };
}

class MainView::StatusMetersComponent final : public juce::Component,
                                              private juce::Timer
{
public:
    explicit StatusMetersComponent(MainView& ownerIn)
        : owner(ownerIn)
    {
        startTimerHz(24);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0F3460));

        auto area = getLocalBounds().reduced(4, 4);

        constexpr int rowHeight = 7;
        constexpr int rowGap = 3;

        auto& core = owner.getProcessor().getCore();

        drawMeterRow(g, area.removeFromTop(rowHeight), "IN",
                     juce::jmax(core.getInputMeterLevelL(), core.getInputMeterLevelR()));

        area.removeFromTop(rowGap);

        drawMeterRow(g, area.removeFromTop(rowHeight), "OUT",
                     juce::jmax(core.getOutputMeterLevelL(), core.getOutputMeterLevelR()));
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    void drawMeterRow(juce::Graphics& g,
                      juce::Rectangle<int> row,
                      const juce::String& label,
                      float level)
    {
        auto labelArea = row.removeFromLeft(24);

        auto meterArea = row;
        const int meterHeight = 6;
        meterArea = meterArea.withSizeKeepingCentre(meterArea.getWidth(), meterHeight);

        g.setColour(juce::Colours::lightgrey.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
        g.drawText(label, labelArea, juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xFF0F3460).darker(0.35f));
        g.fillRect(meterArea);

        const auto innerMeterArea = meterArea.reduced(1, 1);

        const float clamped = juce::jlimit(0.0f, 1.0f, level);
        auto fillWidth = (int) std::round((float) innerMeterArea.getWidth() * clamped);

        if (fillWidth > 0)
        {
            auto fill = innerMeterArea.withWidth(fillWidth);

            juce::ColourGradient meterGradient(
                juce::Colour(0xFF42D96B), (float) innerMeterArea.getX(),     (float) innerMeterArea.getCentreY(),
                juce::Colour(0xFFFF4545), (float) innerMeterArea.getRight(), (float) innerMeterArea.getCentreY(),
                false);

            meterGradient.clearColours();
            meterGradient.addColour(0.00, juce::Colour(0xFF42D96B));
            meterGradient.addColour(0.55, juce::Colour(0xFF42D96B));
            meterGradient.addColour(0.72, juce::Colour(0xFFE6E64A));
            meterGradient.addColour(0.84, juce::Colour(0xFFFFB347));
            meterGradient.addColour(0.92, juce::Colour(0xFFFF7A3A));
            meterGradient.addColour(0.97, juce::Colour(0xFFFF4A3A));
            meterGradient.addColour(1.00, juce::Colour(0xFFFF3B3B));

            g.setGradientFill(meterGradient);
            g.fillRect(fill);
        }

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRect(meterArea, 1);
    }

    MainView& owner;
};

class MainView::PointerEditOverlayHost final : public juce::Component
{
public:
    explicit PointerEditOverlayHost(MainView& ownerIn)
        : owner(ownerIn), content(ownerIn)
    {
        setOpaque(false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(content);
    }

    void resized() override
    {
        content.setBounds(getLocalBounds());
    }

    void paint(juce::Graphics&) override
    {
    }

    void refresh()
    {
        content.repaint();
    }

private:
    MainView& owner;
    PointerEditOverlayContent content;
};

MainView::MidiAssignmentsPopup::MidiAssignmentsPopup(MainView& ownerIn,
                                                     int tabIndexIn)
    : owner(ownerIn),
      tabIndex(tabIndexIn)
{
    assignAllButton.addListener(this);
    clearAllButton.addListener(this);

    assignAllButton.setColour(juce::TextButton::buttonColourId, ButtonStyling::defaultBackground());
    assignAllButton.setColour(juce::TextButton::buttonOnColourId, ButtonStyling::defaultBackground());
    assignAllButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    clearAllButton.setColour(juce::TextButton::buttonColourId, ButtonStyling::defaultBackground());
    clearAllButton.setColour(juce::TextButton::buttonOnColourId, ButtonStyling::defaultBackground());
    clearAllButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    addAndMakeVisible(assignAllButton);
    addAndMakeVisible(clearAllButton);

    refresh();
}

void MainView::MidiAssignmentsPopup::refresh()
{
    deviceButtons.clear();

    auto& core = owner.processor.getCore();
    const auto& inputNames = core.getAvailableMidiInputNames();
    const bool allEnabled = core.isMidiInputAssignedToTab(tabIndex, "MIDI Ch: All");

    for (int i = 0; i < inputNames.size(); ++i)
    {
        const auto& inputName = inputNames[i];

        auto* button = deviceButtons.add(new juce::ToggleButton(inputName));
        button->setToggleState(core.isMidiInputAssignedToTab(tabIndex, inputName),
                               juce::dontSendNotification);
        button->addListener(this);
        button->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        button->setClickingTogglesState(false);
        button->setSize(200, 20);
        button->setWantsKeyboardFocus(false);
        button->setTooltip(inputName);

        const bool isAllButton = (inputName == "MIDI Ch: All");
        if (! isAllButton && allEnabled)
            button->setEnabled(false);
        else
            button->setEnabled(true);

        addAndMakeVisible(button);
    }

    resized();
    repaint();
}

void MainView::MidiAssignmentsPopup::resized()
{
    auto area = getLocalBounds().reduced(12);

    for (int i = 0; i < deviceButtons.size(); ++i)
    {
        deviceButtons[i]->setBounds(area.removeFromTop(20));
        area.removeFromTop(3);
    }

    area.removeFromTop(8);

    auto buttonsArea = area.removeFromTop(28);
    assignAllButton.setBounds(buttonsArea.removeFromLeft(140));
    buttonsArea.removeFromLeft(8);
    clearAllButton.setBounds(buttonsArea.removeFromLeft(80));
}

void MainView::MidiAssignmentsPopup::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(ButtonStyling::defaultBackground());
    g.fillRoundedRectangle(bounds, 10.0f);
}

void MainView::MidiAssignmentsPopup::buttonClicked(juce::Button* button)
{
    auto& core = owner.processor.getCore();
    const auto& inputNames = core.getAvailableMidiInputNames();

    if (button == &assignAllButton)
    {
        DebugLog::write("[MIDI] Assign All Enabled | tabIndex=" + juce::String(tabIndex));

        for (int i = 0; i < inputNames.size(); ++i)
            core.setMidiInputAssignedToTab(tabIndex, inputNames[i], true);

        refresh();
        owner.refreshFromCore();
        owner.repaint();
        return;
    }

    if (button == &clearAllButton)
    {
        DebugLog::write("[MIDI] Clear All assignments | tabIndex=" + juce::String(tabIndex));

        for (int i = 0; i < inputNames.size(); ++i)
            core.setMidiInputAssignedToTab(tabIndex, inputNames[i], false);

        refresh();
        owner.refreshFromCore();
        owner.repaint();
        return;
    }

    for (int i = 0; i < deviceButtons.size(); ++i)
    {
        if (button == deviceButtons[i])
        {
            DebugLog::write("[MIDI] Toggle assignment | tabIndex="
                            + juce::String(tabIndex)
                            + " | input="
                            + inputNames[i]);

            core.toggleMidiInputAssignedToTab(tabIndex, inputNames[i]);
            refresh();
            owner.refreshFromCore();
            owner.repaint();
            return;
        }
    }
}

MainView::MainView(PolyHostPluginProcessor& processorIn)
    : processor(processorIn),
      presetController(appSettings, processor.getCore().getSessionDocument()),
      presetFileHelper(appSettings, presetController),
      recentPresetMenuHelper(presetController),
      toolbarFactory(*this)
{
    DebugLog::setEnabled(appSettings.getDebugLoggingEnabled());
    DebugLog::setAdvancedEnabled(appSettings.getAdvancedDebugLoggingEnabled());

    if (appSettings.getClearDebugLogOnStartup())
        DebugLog::clear();

    DebugLog::write("MainView startup");

    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    menuBar->setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF1E2430));
    addAndMakeVisible(*menuBar);

    setWantsKeyboardFocus(true);

    tooltipWindow.setOpaque(false);

    toolbar.addDefaultItems(toolbarFactory);
    toolbar.setColour(juce::Toolbar::backgroundColourId, juce::Colour(0xFF1D2230));
    addAndMakeVisible(toolbar);

    addAndMakeVisible(presetDropdown);
    presetDropdown.setTextWhenNothingSelected("Untitled");
    presetDropdown.onChange = [this] { handlePresetSelection(); };

    tabButtonsViewport.setViewedComponent(&tabButtonsContainer, false);
    tabButtonsViewport.setScrollBarsShown(false, false);
    tabButtonsViewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(tabButtonsViewport);

    addAndMakeVisible(scrollTabsLeftButton);
    scrollTabsLeftButton.setButtonText("<");
    scrollTabsLeftButton.onClick = [this]
    {
        auto pos = tabButtonsViewport.getViewPosition();
        tabButtonsViewport.setViewPosition(juce::jmax(0, pos.getX() - 120), pos.getY());
        updateTabScrollButtonState();
    };

    addAndMakeVisible(scrollTabsRightButton);
    scrollTabsRightButton.setButtonText(">");
    scrollTabsRightButton.onClick = [this]
    {
        auto pos = tabButtonsViewport.getViewPosition();
        tabButtonsViewport.setViewPosition(pos.getX() + 120, pos.getY());
        updateTabScrollButtonState();
    };

    addAndMakeVisible(addTabButton);
    addTabButton.setButtonText("+");
    addTabButton.onClick = [this] { newTab(); };

    addAndMakeVisible(emptyLoadPluginButton);
    emptyLoadPluginButton.setButtonText("Click to Load Plugin...");
    emptyLoadPluginButton.onClick = [this] { loadPluginIntoMainSlot(); };
    emptyLoadPluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5A5A64));
    emptyLoadPluginButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);

    addAndMakeVisible(editorHolder);

    addAndMakeVisible(routingView);
    routingView.setVisible(false);

    addAndMakeVisible(macroMappingsView);
    macroMappingsView.setVisible(false);

    addAndMakeVisible(contentPlaceholder);
    contentPlaceholder.setJustificationType(juce::Justification::centred);
    contentPlaceholder.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    contentPlaceholder.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    contentPlaceholder.setInterceptsMouseClicks(false, false);

    statusMeters = std::make_unique<StatusMetersComponent>(*this);
    addAndMakeVisible(*statusMeters);

    routingView.onSelectTab = [this](int tabIndex)
    {
        if (! juce::isPositiveAndBelow(tabIndex, processor.getCore().getNumTabs()))
            return;

        showingRoutingView = false;
        selectTab(tabIndex);
    };

    routingView.onCloseTab = [this](int tabIndex)
    {
        closeTab(tabIndex);
    };

    routingView.onMoveUp = [this](int tabIndex)
    {
        auto& core = processor.getCore();

        if (core.moveTabUp(tabIndex))
        {
            refreshFromCore();
            repaint();
        }
    };

    routingView.onMoveDown = [this](int tabIndex)
    {
        auto& core = processor.getCore();

        if (core.moveTabDown(tabIndex))
        {
            refreshFromCore();
            repaint();
        }
    };

    routingView.onToggleBypass = [this](int tabIndex)
    {
        auto& core = processor.getCore();

        while (manualBypassStates.size() < core.getNumTabs())
            manualBypassStates.add(false);

        if (! juce::isPositiveAndBelow(tabIndex, manualBypassStates.size()))
            return;

        manualBypassStates.set(tabIndex, ! manualBypassStates[tabIndex]);
        applyEffectiveBypassStates();
        refreshFromCore();
        repaint();
    };

    routingView.onToggleSolo = [this](int tabIndex)
    {
        handleToggleSolo(tabIndex);
    };

    routingView.onShowMidiAssignments = [this](int tabIndex, juce::Component* anchorComponent)
    {
        showMidiAssignmentsPopup(tabIndex, anchorComponent);
    };

    routingView.onRefreshMidiDevices = [this]
    {
        processor.getCore().refreshAvailableMidiInputs();
        refreshFromCore();
        repaint();
    };

    routingView.onSetPointerAdjustMethodOverride = [this](int tabIndex, int methodOverride)
    {
        processor.getCore().setTabPointerAdjustMethodOverride(tabIndex, methodOverride);
        refreshFromCore();
        repaint();
    };

    macroMappingsView.onDeleteMapping = [this](int macroIndex)
    {
        auto& core = processor.getCore();

        if (core.clearMacroMapping(macroIndex))
        {
            showTemporaryStatusMessage("Deleted Macro "
                                       + juce::String(macroIndex + 1).paddedLeft('0', 3));
            refreshFromCore();
            repaint();
        }
    };

    macroMappingsView.onMoveMappingUp = [this](int macroIndex)
    {
        auto& core = processor.getCore();

        if (core.moveMacroMappingUp(macroIndex))
        {
            showTemporaryStatusMessage("Moved Macro "
                                       + juce::String(macroIndex + 1).paddedLeft('0', 3)
                                       + " up");
            refreshFromCore();
            repaint();
        }
    };

    macroMappingsView.onMoveMappingDown = [this](int macroIndex)
    {
        auto& core = processor.getCore();

        if (core.moveMacroMappingDown(macroIndex))
        {
            showTemporaryStatusMessage("Moved Macro "
                                       + juce::String(macroIndex + 1).paddedLeft('0', 3)
                                       + " down");
            refreshFromCore();
            repaint();
        }
    };

    macroMappingsView.onReplaceMapping = [this](int macroIndex)
    {
        auto& core = processor.getCore();

        if (! core.hasLastTouchedParameter())
        {
            showTemporaryStatusMessage("No last touched parameter detected");
            refreshDirtyUiOnly();
            repaint();
            return;
        }

        const auto description = core.getLastTouchedParameterDescription();

        if (core.replaceMacroMappingFromLastTouched(macroIndex))
        {
            showTemporaryStatusMessage("Replaced Macro "
                                       + juce::String(macroIndex + 1).paddedLeft('0', 3)
                                       + " with "
                                       + description);
            refreshFromCore();
            repaint();
            return;
        }

        showTemporaryStatusMessage("That parameter is already mapped for this tab");
        refreshDirtyUiOnly();
        repaint();
    };

    macroMappingsView.onUndoLastEdit = [this]
    {
        auto& core = processor.getCore();

        if (core.undoLastMacroMappingsEdit())
        {
            showTemporaryStatusMessage("Restored previous macro mappings");
            refreshFromCore();
            repaint();
        }
    };

    macroMappingsView.onClearAllMappings = [this]
    {
        auto& core = processor.getCore();

        if (core.getMacroMappings().isEmpty())
            return;

        const bool confirmed = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Clear All Macro Mappings",
            "Delete all macro mappings?",
            "Clear All",
            "Cancel",
            getWindowCenterTarget());

        if (! confirmed)
            return;

        core.clearAllMacroMappings();
        showTemporaryStatusMessage("Cleared all macro mappings");
        refreshFromCore();
        repaint();
    };

    lastKnownDirtyState = processor.getCore().isDirty();
    lastKnownShowingState = isShowing();

    pointerControl.setSnapWeights(appSettings.getPointerControlXSnapWeight(),
                                  appSettings.getPointerControlYSnapWeight());
    pointerControl.setLaneTolerance(30.0f);

    startTimer(60);
    refreshFromCore();
    syncManualBypassStatesFromCore();
    updateSaveButtonColours();

    // Prompt for missing plugins that were detected during state restore
    // (happens when host loads preset before editor is opened)
    if (! processor.getCore().getMissingPlugins().isEmpty())
    {
        pendingMissingPluginPrompt = true;
        pendingMissingPluginPromptDelayTicks = 0;
    }
}

MainView::~MainView()
{
    DebugLog::write("[MainView] destructor | shutting down");

    stopTimer();

    if (menuBar != nullptr)
        menuBar->setModel(nullptr);

    hidePointerEditOverlay();
    clearHostedEditor();
    pointerEditOverlayHost.reset();

    savePresetToolbarButton = nullptr;
    savePresetAsToolbarButton = nullptr;

    DebugLog::write("[MainView] destructor complete");
}

juce::Point<int> MainView::getHostedEditorLocalPointFromScreen(juce::Point<int> screenPoint) const
{
    if (hostedEditor == nullptr)
        return {};

    return hostedEditor->getLocalPoint(nullptr, screenPoint);
}

juce::Point<int> MainView::getHostedEditorScreenPointFromLocal(juce::Point<int> localPoint) const
{
    if (hostedEditor == nullptr)
        return {};

    return hostedEditor->localPointToGlobal(localPoint);
}

juce::StringArray MainView::getMenuBarNames()
{
    return { "File", "MIDI", "Options", "Help" };
}

juce::PopupMenu MainView::getMenuForIndex(int topLevelMenuIndex,
                                          const juce::String& menuName)
{
    juce::ignoreUnused(menuName);

    juce::PopupMenu menu;

    switch (topLevelMenuIndex)
    {
        case 0:
        {
            auto recentMenu = recentPresetMenuHelper.buildRecentPresetMenu(commandRecentPresetBase);

            menu.addItem(commandNewPreset, "New Preset");
            menu.addSeparator();
            menu.addItem(commandNewTab, "New Tab");
            menu.addItem(commandCloseCurrentTab, "Close Current Tab",
                         processor.getCore().getNumTabs() > 0);
            menu.addSeparator();
            menu.addItem(commandReplacePlugin, "Replace Plugin...");
            menu.addItem(commandNewPlugin, "New Plugin...");
            menu.addSeparator();
            menu.addItem(commandSavePreset, "Save Preset");
            menu.addItem(commandSavePresetAs, "Save Preset As...");
            menu.addItem(commandLoadPreset, "Load Preset...");
            menu.addSubMenu("Recent Presets", recentMenu);
            menu.addItem(commandLocateMissingPluginsNow, "Locate Missing Plugins...");
            menu.addItem(commandDeleteCurrentPreset,
                         "Delete Current Preset",
                         presetController.hasCurrentFile());
            menu.addSeparator();
            menu.addItem(commandOpenPresetsFolder, "Open Presets Folder");
            break;
        }

        case 1:
            menu.addItem(commandMidiMonitor, "MIDI Monitor");
            menu.addItem(commandRefreshMidiDevices, "Refresh MIDI Devices");
            menu.addSeparator();
            menu.addItem(commandMidiPanic, "Panic!   \t(Ctrl+Shift+P)");
            break;

        case 2:
        {
            menu.addItem(commandPointerControlSettings, "Pointer Control Settings...");
            menu.addSeparator();

            juce::PopupMenu pluginRepairsMenu;
            pluginRepairsMenu.addItem(commandAutoSaveAfterPluginRepair,
                                      "Auto-Save Preset After Plugin Repair",
                                      true,
                                      appSettings.getAutoSaveAfterPluginRepair());
            pluginRepairsMenu.addSeparator();
            pluginRepairsMenu.addItem(commandAddPluginScanFolder,
                                      "Add Plugin Scan Folder...");
            pluginRepairsMenu.addItem(commandShowPluginScanFolders,
                                      "Show Plugin Scan Folders");
            pluginRepairsMenu.addItem(commandClearPluginScanFolders,
                                      "Clear Plugin Scan Folders",
                                      appSettings.getPluginScanFolders().size() > 0);

            menu.addSubMenu("Plugin Repairs", pluginRepairsMenu);
            menu.addSeparator();

            juce::PopupMenu debugMenu;
            debugMenu.addItem(commandEnableDebugLogging,
                              "Enable Debug Logging",
                              true,
                              appSettings.getDebugLoggingEnabled());
            debugMenu.addItem(commandEnableAdvancedDebugLogging,
                              "Enable Advanced Debug Logging",
                              appSettings.getDebugLoggingEnabled(),
                              appSettings.getAdvancedDebugLoggingEnabled());
            debugMenu.addItem(commandClearDebugLogOnStartup,
                              "Clear Debug Log On Startup",
                              true,
                              appSettings.getClearDebugLogOnStartup());
            debugMenu.addItem(commandClearDebugLogNow,
                              "Clear Debug Log Now");

            menu.addSubMenu("Debug", debugMenu);
            break;
        }

        case 3:
            menu.addItem(9005, "About PolyHost...");
            break;

        default:
            break;
    }

    return menu;
}

void MainView::menuItemSelected(int menuItemID,
                                int topLevelMenuIndex)
{
    juce::ignoreUnused(topLevelMenuIndex);

    if (recentPresetMenuHelper.isRecentPresetItemId(menuItemID, commandRecentPresetBase))
    {
        loadRecentPreset(menuItemID);
        return;
    }

    switch (menuItemID)
    {
        case commandNewPreset:
            if (promptToSaveIfNeeded())
                createNewPreset();
            break;

        case commandNewTab:
            newTab();
            break;

        case commandCloseCurrentTab:
            closeCurrentTab();
            break;

        case commandSavePreset:
            savePreset();
            break;

        case commandSavePresetAs:
            savePresetAs();
            break;

        case commandLoadPreset:
            if (promptToSaveIfNeeded())
                loadPresetFromFile();
            break;

        case commandMidiMonitor:
            if (midiMonitorWindow == nullptr)
                midiMonitorWindow = std::make_unique<VstMidiMonitorWindow>(processor,
                                                                          getWindowCenterTarget());

            midiMonitorWindow->setVisible(true);
            midiMonitorWindow->toFront(true);
            break;

        case commandRefreshMidiDevices:
            processor.getCore().refreshAvailableMidiInputs();
            refreshFromCore();
            repaint();
            break;

        case commandMidiPanic:
            sendMidiPanic();
            break;

        case commandDeleteCurrentPreset:
            deleteCurrentPreset();
            break;

        case commandOpenPresetsFolder:
            openPresetsFolder();
            break;

        case commandReplacePlugin:
            replacePlugin();
            break;

        case commandNewPlugin:
            newPlugin();
            break;

        case commandPointerControlSettings:
            showPointerControlSettingsDialog();
            break;

        case commandAutoSaveAfterPluginRepair:
            appSettings.setAutoSaveAfterPluginRepair(! appSettings.getAutoSaveAfterPluginRepair());
            break;

        case commandAddPluginScanFolder:
            addPluginScanFolder();
            break;

        case commandShowPluginScanFolders:
            showPluginScanFolders();
            break;

        case commandClearPluginScanFolders:
            clearPluginScanFolders();
            break;

        case commandLocateMissingPluginsNow:
            locateMissingPluginsNow();
            break;

        case commandEnableDebugLogging:
        {
            auto enabled = !appSettings.getDebugLoggingEnabled();
            appSettings.setDebugLoggingEnabled(enabled);
            DebugLog::setEnabled(enabled);

            if (!enabled)
            {
                appSettings.setAdvancedDebugLoggingEnabled(false);
                DebugLog::setAdvancedEnabled(false);
            }

            DebugLog::write("Debug logging " + juce::String(enabled ? "enabled" : "disabled"));
            refreshDirtyUiOnly();
            repaint();
            break;
        }

        case commandEnableAdvancedDebugLogging:
        {
            if (!appSettings.getDebugLoggingEnabled())
                break;

            auto advancedEnabled = !appSettings.getAdvancedDebugLoggingEnabled();
            appSettings.setAdvancedDebugLoggingEnabled(advancedEnabled);
            DebugLog::setAdvancedEnabled(advancedEnabled);
            DebugLog::write("Advanced debug logging "
                            + juce::String(advancedEnabled ? "enabled" : "disabled"));
            refreshDirtyUiOnly();
            repaint();
            break;
        }

        case commandClearDebugLogOnStartup:
        {
            auto shouldClear = !appSettings.getClearDebugLogOnStartup();
            appSettings.setClearDebugLogOnStartup(shouldClear);
            refreshDirtyUiOnly();
            repaint();
            break;
        }

        case commandClearDebugLogNow:
            DebugLog::clear();
            DebugLog::write("Debug log cleared");
            refreshDirtyUiOnly();
            repaint();
            break;

        case 9005:
            showAboutDialog();
            break;

        default:
            break;
    }
}

bool MainView::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('P',
                              juce::ModifierKeys::ctrlModifier
                                  | juce::ModifierKeys::shiftModifier,
                              0))
    {
        sendMidiPanic();
        return true;
    }

    return false;
}

void MainView::timerCallback()
{
    processPendingPointerMidi();
    pointerControl.handleExternalMouseMove();
    pointerControl.releaseDragIfIdle((double) appSettings.getPointerControlDragReturnDelayMs());

    if (pointerControlEditModeEnabled)
        updatePointerEditOverlay();

    auto currentDirtyState = processor.getCore().isDirty();

    if (currentDirtyState != lastKnownDirtyState)
    {
        DebugLog::writeAdvanced("[MainView] timerCallback | dirty state changed | dirty="
                                + juce::String(currentDirtyState ? "true" : "false"));

        lastKnownDirtyState = currentDirtyState;
        refreshDirtyUiOnly();
    }

    if (temporaryStatusExpiryMs != 0
        && juce::Time::getMillisecondCounter() >= temporaryStatusExpiryMs)
    {
        temporaryStatusExpiryMs = 0;
        temporaryStatusMessage.clear();
        refreshDirtyUiOnly();
        repaint();
    }

    if (pendingMissingPluginPrompt && isShowing())
    {
        // Wait ~2 seconds (timer fires at ~60ms, so 33 ticks ≈ 2 seconds)
        // This gives the host's context menu time to appear and be dismissed
        if (pendingMissingPluginPromptDelayTicks < 33)
        {
            ++pendingMissingPluginPromptDelayTicks;
        }
        else
        {
            pendingMissingPluginPrompt = false;
            pendingMissingPluginPromptDelayTicks = 0;

            auto& missing = processor.getCore().getMissingPlugins();

            if (! missing.isEmpty())
            {
                juce::String message;
                message << missing.size() << " plugin(s) could not be found:\n\n";

                for (int i = 0; i < missing.size(); ++i)
                    message << "  - " << missing.getReference(i).pluginName << "\n";

                message << "\nWould you like to locate them now?";

                const bool locate = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Missing Plugins",
                    message,
                    "Locate Now",
                    "Later",
                    getWindowCenterTarget());

                if (locate)
                {
                    locateMissingPluginsNow();
                }
                else
                {
                    // User chose Later — do not prompt again this session.
                    // The missing plugins list is preserved so File > Locate
                    // Missing Plugins... still works, but the auto-prompt
                    // will not reappear until the preset is reloaded.
                    processor.getCore().clearMissingPlugins();
                }
            }
        }
    }

    const bool currentShowingState = isShowing();

    if (currentShowingState != lastKnownShowingState)
    {
        DebugLog::write("[Window] timerCallback | visibility changed | isShowing="
                        + juce::String(currentShowingState ? "true" : "false"));

        lastKnownShowingState = currentShowingState;

        if (currentShowingState)
            recoverCurrentTabAfterWindowReopen();
        else
            clearHostedEditorForWindowClose();
    }
}

void MainView::clearHostedEditor()
{
    if (hostedEditor != nullptr)
    {
        DebugLog::writeAdvanced("[HostedEditor] clearing hosted editor");

        if (auto* instance = processor.getCore().getMainPluginInstance())
            instance->editorBeingDeleted(hostedEditor.get());

        editorHolder.removeChildComponent(hostedEditor.get());
        hostedEditor.reset();
    }

    contentPlaceholder.setVisible(true);
    updatePointerEditOverlay();
}

bool MainView::promptToSaveIfNeeded()
{
    auto& core = processor.getCore();

    DebugLog::write("[Preset] promptToSaveIfNeeded | dirty="
                    + juce::String(core.isDirty() ? "true" : "false"));

    if (! core.isDirty())
        return true;

    auto decision = unsavedChangesHelper.promptToSaveChanges(getWindowCenterTarget());

    if (decision == UnsavedChangesHelper::Decision::cancel)
    {
        DebugLog::write("[Preset] promptToSaveIfNeeded | user chose cancel");
        return false;
    }

    if (decision == UnsavedChangesHelper::Decision::discard)
    {
        DebugLog::write("[Preset] promptToSaveIfNeeded | user chose discard");
        return true;
    }

    if (decision == UnsavedChangesHelper::Decision::save)
    {
        DebugLog::write("[Preset] promptToSaveIfNeeded | user chose save");
        savePreset();
        return ! core.isDirty();
    }

    return true;
}

void MainView::createNewPreset()
{
    DebugLog::write("[Preset] createNewPreset requested");

    clearHostedEditor();
    soloedTabIndices.clear();
    manualBypassStates.clear();

    auto& core = processor.getCore();
    core.resetForNewPreset();
    presetController.clearForNewPreset();
    showingRoutingView = false;

    auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>();
    if (parentEditor != nullptr)
        parentEditor->setSize(800, 500);

    refreshFromCore();
    repaint();
    syncManualBypassStatesFromCore();
}

void MainView::rebuildPresetDropdown()
{
    auto currentText = presetDropdown.getText();

    presetDropdown.clear(juce::dontSendNotification);
    presetDropdown.addItem("New Preset", 1);
    presetDropdown.addSeparator();

    auto recentPresetPaths = presetController.getRecentPresetPaths();
    juce::StringArray presetNames;

    for (int i = 0; i < recentPresetPaths.size(); ++i)
    {
        juce::File presetFile(recentPresetPaths[i]);

        if (presetFile.existsAsFile())
            presetNames.addIfNotAlreadyThere(presetFile.getFileNameWithoutExtension());
    }

    DebugLog::writeAdvanced("[Preset] rebuildPresetDropdown | recentPresetCount="
                            + juce::String(presetNames.size()));

    presetNames.sort(true);

    int itemId = 2;
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetDropdown.addItem(presetNames[i], itemId);
        ++itemId;
    }

    presetDropdown.setText(currentText, juce::dontSendNotification);
}

void MainView::handlePresetSelection()
{
    auto selectedId = presetDropdown.getSelectedId();

    DebugLog::write("[Preset] handlePresetSelection | selectedId=" + juce::String(selectedId));

    if (selectedId == 1)
    {
        DebugLog::write("[Preset] New Preset selected from dropdown");

        if (promptToSaveIfNeeded())
            createNewPreset();

        refreshDirtyUiOnly();
        return;
    }

    if (selectedId <= 1)
        return;

    auto recentPresetPaths = presetController.getRecentPresetPaths();
    juce::Array<juce::File> validFiles;
    juce::StringArray presetNames;

    for (int i = 0; i < recentPresetPaths.size(); ++i)
    {
        juce::File presetFile(recentPresetPaths[i]);

        if (! presetFile.existsAsFile())
            continue;

        auto presetName = presetFile.getFileNameWithoutExtension();

        if (! presetNames.contains(presetName))
        {
            presetNames.add(presetName);
            validFiles.add(presetFile);
        }
    }

    juce::Array<int> sortedIndices;
    for (int i = 0; i < presetNames.size(); ++i)
        sortedIndices.add(i);

    std::sort(sortedIndices.begin(), sortedIndices.end(),
              [&presetNames](int a, int b)
              {
                  return presetNames[a].compareIgnoreCase(presetNames[b]) < 0;
              });

    int recentIndex = selectedId - 2;

    if (! juce::isPositiveAndBelow(recentIndex, sortedIndices.size()))
        return;

    auto selectedFile = validFiles.getReference(sortedIndices.getReference(recentIndex));
    DebugLog::write("[Preset] Dropdown preset selected | file=" + selectedFile.getFullPathName());

    if (! promptToSaveIfNeeded())
    {
        refreshDirtyUiOnly();
        return;
    }

    loadSessionFromFile(selectedFile);
}

void MainView::reloadCurrentPreset()
{
    DebugLog::write("[Preset] reloadCurrentPreset requested");

    dismissMidiAssignmentsPopup();

    auto currentFile = presetController.getCurrentFile();

    if (! currentFile.existsAsFile())
    {
        DebugLog::write("[Preset] reloadCurrentPreset | no saved preset file exists");
        processor.getCore().setStatusText("No saved preset to reload");
        refreshDirtyUiOnly();
        repaint();
        return;
    }

    juce::AlertWindow alert("Revert Preset",
                            "Discard all unsaved changes and reload \""
                                + currentFile.getFileNameWithoutExtension()
                                + "\"?",
                            juce::AlertWindow::WarningIcon);

    alert.addButton("Revert Changes", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    alert.centreAroundComponent(getWindowCenterTarget(), 420, 180);

    if (alert.runModalLoop() != 1)
    {
        DebugLog::write("[Preset] reloadCurrentPreset | user cancelled");
        return;
    }

    DebugLog::write("[Preset] reloadCurrentPreset | user confirmed | file="
                    + currentFile.getFullPathName());

    loadSessionFromFile(currentFile);
}

void MainView::sendMidiPanic()
{
    DebugLog::write("[MIDI] sendMidiPanic requested");

    dismissMidiAssignmentsPopup();
    processor.getCore().sendMidiPanic();
    refreshDirtyUiOnly();
    repaint();
}

void MainView::showTemporaryStatusMessage(const juce::String& text)
{
    temporaryStatusMessage = text;
    temporaryStatusExpiryMs = juce::Time::getMillisecondCounter() + 5000;
    repaint();
}

juce::Component* MainView::getWindowCenterTarget() const
{
    if (auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>())
        return parentEditor;

    return const_cast<MainView*>(this);
}

void MainView::setPointerControlEditMode(bool shouldEnable)
{
    pointerControlEditModeEnabled = shouldEnable;

    if (auto* item = toolbar.getItemComponent(ToolbarFactory::pointerControlEdit))
        item->repaint();

    updatePointerEditOverlay();
    repaint();
}

void MainView::togglePointerControlEditMode()
{
    setPointerControlEditMode(! pointerControlEditModeEnabled);
}

void MainView::hidePointerEditOverlay()
{
    if (pointerEditOverlayHost != nullptr)
        pointerEditOverlayHost->setVisible(false);
}

void MainView::updatePointerEditOverlay()
{
    if (! pointerControlEditModeEnabled)
    {
        hidePointerEditOverlay();
        return;
    }

    if (showingRoutingView || showingMacroMappingsView || hostedEditor == nullptr || ! hostedEditor->isShowing())
    {
        hidePointerEditOverlay();
        return;
    }

    auto screenBounds = hostedEditor->getScreenBounds();

    if (screenBounds.isEmpty())
    {
        hidePointerEditOverlay();
        return;
    }

    if (pointerEditOverlayHost == nullptr)
    {
        pointerEditOverlayHost = std::make_unique<PointerEditOverlayHost>(*this);
        pointerEditOverlayHost->addToDesktop(juce::ComponentPeer::windowIsTemporary
                                             | juce::ComponentPeer::windowIgnoresKeyPresses);
    }

    pointerEditOverlayHost->setBounds(screenBounds);
    pointerEditOverlayHost->setVisible(true);
    pointerEditOverlayHost->toFront(false);
    pointerEditOverlayHost->refresh();
}

void MainView::refreshPointerControlTarget()
{
    if (showingRoutingView)
    {
        pointerControl.clearTarget();
        lastPointerTargetBounds = {};
        lastPointerTargetTabIndex = -1;
        lastPointerJumpPointCount = -1;
        lastPointerXccValue = -1;
        lastPointerYccValue = -1;
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget | skipped, showing routing view");
        return;
    }

    if (hostedEditor == nullptr || ! hostedEditor->isShowing())
    {
        pointerControl.clearTarget();
        lastPointerTargetBounds = {};
        lastPointerTargetTabIndex = -1;
        lastPointerJumpPointCount = -1;
        lastPointerXccValue = -1;
        lastPointerYccValue = -1;
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget | skipped, no hosted editor");
        return;
    }

    auto targetBounds = hostedEditor->getScreenBounds();

    if (targetBounds.isEmpty())
    {
        pointerControl.clearTarget();
        lastPointerTargetBounds = {};
        lastPointerTargetTabIndex = -1;
        lastPointerJumpPointCount = -1;
        lastPointerXccValue = -1;
        lastPointerYccValue = -1;
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget | skipped, empty bounds");
        return;
    }

    auto& core = processor.getCore();
    const int tabIndex = core.getSelectedTabIndex();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
    {
        pointerControl.clearTarget();
        lastPointerTargetBounds = {};
        lastPointerTargetTabIndex = -1;
        lastPointerJumpPointCount = -1;
        lastPointerXccValue = -1;
        lastPointerYccValue = -1;
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget | skipped, invalid tab index");
        return;
    }

    const auto& storedPoints = core.getTabPointerJumpPoints(tabIndex);

    const bool boundsChanged = (targetBounds != lastPointerTargetBounds);
    const bool tabChanged = (tabIndex != lastPointerTargetTabIndex);
    const bool pointCountChanged = (storedPoints.size() != lastPointerJumpPointCount);
    juce::ignoreUnused(tabChanged, pointCountChanged);

    const bool shouldRefreshPoints = true;

    if (boundsChanged)
    {
        DebugLog::writeAdvanced("[PointerControl] refreshPointerControlTarget | bounds changed | tabIndex="
                                + juce::String(tabIndex)
                                + " | bounds="
                                + targetBounds.toString()
                                + " | points="
                                + juce::String(storedPoints.size()));

        pointerControl.setTargetScreenBounds(targetBounds);
        lastPointerTargetBounds = targetBounds;
    }

    pointerControl.setSnapWeights(appSettings.getPointerControlXSnapWeight(),
                                  appSettings.getPointerControlYSnapWeight());
    pointerControl.setLaneTolerance(core.getTabPointerLaneTolerance(tabIndex));

    if (shouldRefreshPoints)
    {
        juce::Array<PointerControl::JumpPoint> convertedPoints;

        for (int i = 0; i < storedPoints.size(); ++i)
        {
            const auto& stored = storedPoints.getReference(i);

            PointerControl::JumpPoint point;
            point.x = stored.x;
            point.y = stored.y;
            convertedPoints.add(point);
        }

        pointerControl.setJumpPoints(convertedPoints, hostedEditor->getLocalBounds());
        lastPointerTargetTabIndex = tabIndex;
        lastPointerJumpPointCount = storedPoints.size();
    }
}

void MainView::processPendingPointerMidi()
{
    DebugLog::writeAdvanced("[PointerControl] processPendingPointerMidi called");

    auto* thisWindow = findParentComponentOfClass<juce::TopLevelWindow>();
    juce::ignoreUnused(thisWindow);

    refreshPointerControlTarget();

    PolyHostPluginProcessor::PointerMidiEvent event;

    while (processor.popNextPointerMidiEvent(event))
    {
        if (! event.valid)
            continue;

        const int cc = event.controllerNumber;
        const int value = event.controllerValue;

        const int pointerXcc = appSettings.getPointerControlXccNumber();
        const int pointerYcc = appSettings.getPointerControlYccNumber();
        const int pointerAdjustCc = appSettings.getPointerControlAdjustCcNumber();
        const int pointerToleranceCc = appSettings.getPointerControlToleranceCcNumber();
        const int pointerSensitivityCc = appSettings.getPointerControlSensitivityCcNumber();

        if (cc == pointerXcc)
        {
            DebugLog::writeAdvanced("[PointerControl] MIDI X | cc="
                                    + juce::String(cc)
                                    + " | value="
                                    + juce::String(value));
            pointerControl.panX(value);
            lastPointerXccValue = value;
        }
        else if (cc == pointerYcc)
        {
            DebugLog::writeAdvanced("[PointerControl] MIDI Y | cc="
                                    + juce::String(cc)
                                    + " | value="
                                    + juce::String(value));
            pointerControl.panY(value);
            lastPointerYccValue = value;
        }
        else if (cc == pointerAdjustCc)
        {
            const int adjustMode = appSettings.getPointerControlAdjustCcMode();
            int delta = 0;

            if (adjustMode == 2)
            {
                if (value == 127)
                    delta = -1;
                else if (value == 1)
                    delta = 1;
            }
            else if (adjustMode == 3)
            {
                if (value == 63)
                    delta = -1;
                else if (value == 65)
                    delta = 1;
            }
            else if (adjustMode == 4)
            {
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

                    if (delta > 64)
                        delta -= 128;
                    else if (delta < -64)
                        delta += 128;
                }

                lastPointerAdjustCcValue = value;
            }

            if (delta != 0)
            {
                int adjustMethod = appSettings.getPointerControlAdjustMethod();

                auto& core = processor.getCore();
                const int tabIndex = core.getSelectedTabIndex();

                if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                {
                    const int methodOverride = core.getTabPointerAdjustMethodOverride(tabIndex);

                    if (methodOverride == 1)
                        adjustMethod = 1;
                    else if (methodOverride == 2)
                        adjustMethod = 2;
                }

                int sensitivity = 1;

                if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                    sensitivity = core.getTabPointerAdjustSensitivity(tabIndex);
                const int direction = (delta > 0 ? 1 : -1);
                const int repeatCount = std::abs(delta) * sensitivity;

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
            if (! pointerControlEditModeEnabled)
            {
                lastPointerToleranceCcValue = value;
                continue;
            }

            if (lastPointerToleranceCcValue >= 0)
            {
                int delta = value - lastPointerToleranceCcValue;

                if (delta > 64)
                    delta -= 128;
                else if (delta < -64)
                    delta += 128;

                if (delta != 0)
                {
                    auto& core = processor.getCore();
                    const int tabIndex = core.getSelectedTabIndex();

                    if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
                    {
                        float tolerance = core.getTabPointerLaneTolerance(tabIndex) + (float) delta;
                        tolerance = juce::jlimit(1.0f, 128.0f, tolerance);

                        DebugLog::writeAdvanced("[PointerControl] tolerance changed | delta="
                                                + juce::String(delta)
                                                + " | newTolerance="
                                                + juce::String(tolerance, 1));

                        pointerControl.setLaneTolerance(tolerance);
                        core.setTabPointerLaneTolerance(tabIndex, tolerance);
                        core.setStatusText("Pointer tolerance: " + juce::String(tolerance, 1));
                        showTemporaryStatusMessage("Pointer tolerance: " + juce::String(tolerance, 1));

                        if (pointerEditOverlayHost != nullptr)
                            pointerEditOverlayHost->refresh();
                    }
                }
            }

            lastPointerToleranceCcValue = value;
        }
        else if (cc == pointerSensitivityCc)
        {
            const int mappedSensitivity = juce::jlimit(1, 20, 1 + (value * 19) / 127);

            DebugLog::writeAdvanced("[PointerControl] sensitivity changed | ccValue="
                                    + juce::String(value)
                                    + " | mappedSensitivity="
                                    + juce::String(mappedSensitivity));

            auto& core = processor.getCore();
            const int tabIndex = core.getSelectedTabIndex();

            if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
            {
                core.setTabPointerAdjustSensitivity(tabIndex, mappedSensitivity);
                core.setStatusText("Adjust sensitivity: " + juce::String(mappedSensitivity));
                showTemporaryStatusMessage("Adjust sensitivity: " + juce::String(mappedSensitivity));
            }
        }
    }
}

void MainView::rebuildTabButtons()
{
    tabButtons.clear();

    auto& tabModel = processor.getCore().getTabModel();

    DebugLog::writeAdvanced("[Tabs] rebuildTabButtons | tabCount="
                            + juce::String(tabModel.getNumTabs()));

    for (int i = 0; i < tabModel.getNumTabs(); ++i)
    {
        const auto& tab = tabModel.getTab(i);

        auto* button = tabButtons.add(new TabButton(tab.name, tab.type, tab.selected));
        button->setTriggeredOnMouseDown(true);

        button->onClick = [this, i, button]
        {
            auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();

            if (mods.isRightButtonDown())
            {
                showTabContextMenuAsync(i, button);
                return;
            }

            selectTab(i);
        };

        tabButtonsContainer.addAndMakeVisible(button);
    }

    addAndMakeVisible(tabButtonsViewport);
    addAndMakeVisible(scrollTabsLeftButton);
    addAndMakeVisible(scrollTabsRightButton);
    addAndMakeVisible(addTabButton);

    auto& core = processor.getCore();
    const int selectedTab = core.getSelectedTabIndex();

    if (juce::isPositiveAndBelow(selectedTab, tabButtons.size()))
    {
        if (auto* selectedButton = tabButtons[selectedTab])
        {
            auto viewPos = tabButtonsViewport.getViewPosition();
            auto viewArea = tabButtonsViewport.getViewArea();
            auto buttonBounds = selectedButton->getBounds();

            if (buttonBounds.getX() < viewArea.getX())
            {
                tabButtonsViewport.setViewPosition(buttonBounds.getX(), viewPos.getY());
            }
            else if (buttonBounds.getRight() > viewArea.getRight())
            {
                tabButtonsViewport.setViewPosition(buttonBounds.getRight() - viewArea.getWidth(),
                                                   viewPos.getY());
            }
        }
    }

    updateTabScrollButtonState();
}

void MainView::handleToggleSolo(int tabIndex)
{
    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    if (soloedTabIndices.contains(tabIndex))
        soloedTabIndices.removeFirstMatchingValue(tabIndex);
    else
        soloedTabIndices.addIfNotAlreadyThere(tabIndex);

    applyEffectiveBypassStates();
    refreshFromCore();
    toolbar.repaint();
    repaint();
}

void MainView::clearAllSolos()
{
    if (soloedTabIndices.isEmpty())
        return;

    soloedTabIndices.clear();
    applyEffectiveBypassStates();
    refreshFromCore();
    toolbar.repaint();
    repaint();
}

void MainView::mapLastTouchedParameterToMacro()
{
    auto& core = processor.getCore();

    if (! core.hasLastTouchedParameter())
    {
        showTemporaryStatusMessage("No last touched parameter detected");
        refreshDirtyUiOnly();
        updateSaveButtonColours();
        repaint();
        return;
    }

    const int nextFreeMacro = core.getNextFreeMacroIndex();

    if (nextFreeMacro < 0)
    {
        showTemporaryStatusMessage("No free macro slots available");
        refreshDirtyUiOnly();
        repaint();
        return;
    }

    const auto description = core.getLastTouchedParameterDescription();

    if (core.assignLastTouchedToNextFreeMacro())
    {
        showTemporaryStatusMessage("Mapped " + description
                                   + " to Macro "
                                   + juce::String(nextFreeMacro + 1).paddedLeft('0', 3));
        refreshDirtyUiOnly();
        repaint();
        return;
    }

    showTemporaryStatusMessage("That parameter is already mapped for this tab");
    refreshDirtyUiOnly();
    repaint();
}

void MainView::showMacroMappingsView()
{
    dismissMidiAssignmentsPopup();

    if (showingMacroMappingsView)
    {
        showingMacroMappingsView = false;
        refreshFromCore();
        repaint();
        resizeParentEditorToFitHostedPlugin();
        updatePointerEditOverlay();
        return;
    }

    showingRoutingView = false;
    showingMacroMappingsView = true;
    refreshFromCore();
    repaint();

    if (auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>())
        parentEditor->resizeToRoutingView();

    updatePointerEditOverlay();
}

void MainView::syncManualBypassStatesFromCore()
{
    auto& core = processor.getCore();
    manualBypassStates.clearQuick();

    for (int i = 0; i < core.getNumTabs(); ++i)
        manualBypassStates.add(core.isTabBypassed(i));
}

void MainView::applyEffectiveBypassStates()
{
    auto& core = processor.getCore();

    while (manualBypassStates.size() < core.getNumTabs())
        manualBypassStates.add(false);

    if (soloedTabIndices.isEmpty())
    {
        for (int i = 0; i < core.getNumTabs(); ++i)
            core.setTabBypassed(i, manualBypassStates[i]);
    }
    else
    {
        for (int i = 0; i < core.getNumTabs(); ++i)
        {
            const bool isSoloed = soloedTabIndices.contains(i);

            // During solo:
            // - soloed tabs are always audible
            // - non-soloed tabs are always bypassed
            const bool effectiveBypassed = ! isSoloed;
            core.setTabBypassed(i, effectiveBypassed);
        }
    }
}

void MainView::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    grabKeyboardFocus();
}

void MainView::mouseWheelMove(const juce::MouseEvent& event,
                              const juce::MouseWheelDetails& wheel)
{
    if (tabButtonsViewport.getBounds().contains(event.position.toInt()))
    {
        auto pos = tabButtonsViewport.getViewPosition();
        const int delta = (int) std::round((wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY) * 80.0f);
        tabButtonsViewport.setViewPosition(juce::jmax(0, pos.getX() - delta), pos.getY());
        updateTabScrollButtonState();
        return;
    }

    juce::Component::mouseWheelMove(event, wheel);
}

void MainView::updateTabScrollButtonState()
{
    const int viewX = tabButtonsViewport.getViewPositionX();
    const int viewWidth = tabButtonsViewport.getViewWidth();
    const int contentWidth = tabButtonsContainer.getWidth();

    const bool canScrollLeft = viewX > 0;
    const bool canScrollRight = (viewX + viewWidth) < contentWidth;

    scrollTabsLeftButton.setEnabled(canScrollLeft);
    scrollTabsRightButton.setEnabled(canScrollRight);

    scrollTabsLeftButton.repaint();
    scrollTabsRightButton.repaint();
}

void MainView::toggleRoutingView()
{
    DebugLog::write("[Routing] toggleRoutingView requested | current="
                    + juce::String(showingRoutingView ? "true" : "false"));

    dismissMidiAssignmentsPopup();
    showingRoutingView = ! showingRoutingView;

    if (showingRoutingView)
        showingMacroMappingsView = false;

    DebugLog::write("[Routing] toggleRoutingView new state="
                    + juce::String(showingRoutingView ? "true" : "false"));

    auto& core = processor.getCore();

    refreshFromCore();
    repaint();

    if (auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>())
    {
        if (showingRoutingView)
        {
            if (core.hasRoutingViewSize())
                parentEditor->setSize(core.getRoutingViewWidth(), core.getRoutingViewHeight());
            else
                parentEditor->resizeToRoutingView();
        }
        else
        {
            resizeParentEditorToFitHostedPlugin();
        }
    }

    updatePointerEditOverlay();
}

void MainView::rebuildRoutingView()
{
    juce::Array<RoutingView::ModuleEntry> modules;
    auto& core = processor.getCore();
    auto& tabModel = core.getTabModel();

    DebugLog::writeAdvanced("[Routing] rebuildRoutingView | tabCount="
                            + juce::String(tabModel.getNumTabs()));

    for (int i = 0; i < tabModel.getNumTabs(); ++i)
    {
        const auto& tab = tabModel.getTab(i);

        RoutingView::ModuleEntry entry;
        entry.tabIndex = i;
        entry.name = tab.name;
        entry.type = tab.type;
        const bool isSoloed = soloedTabIndices.contains(i);
        const bool manualBypassed = juce::isPositiveAndBelow(i, manualBypassStates.size())
                                        ? manualBypassStates[i]
                                        : core.isTabBypassed(i);

        entry.isBypassed = manualBypassed;
        entry.isSoloed = isSoloed;
        entry.isMutedBySolo = (! soloedTabIndices.isEmpty() && ! isSoloed && ! manualBypassed);
        entry.canMoveUp = core.canMoveTabUp(i);
        entry.canMoveDown = core.canMoveTabDown(i);
        entry.midiAssignmentCount = core.getTabMidiAssignmentCount(i);
        entry.routingTooltip = core.getRoutingTooltipForTab(i);
        entry.pointerAdjustMethodOverride = core.getTabPointerAdjustMethodOverride(i);

        modules.add(entry);
    }

    routingView.setModules(modules);
}

void MainView::selectTab(int tabIndex)
{
    DebugLog::write("[Tabs] selectTab | tabIndex=" + juce::String(tabIndex));

    dismissMidiAssignmentsPopup();
    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    if (showingRoutingView)
        showingRoutingView = false;

    if (showingMacroMappingsView)
        showingMacroMappingsView = false;

    core.setSelectedTabIndex(tabIndex);
    core.getTabModel().selectTab(tabIndex);

    refreshFromCore();

    showTemporaryStatusMessage("Adjust sensitivity: "
                               + juce::String(core.getTabPointerAdjustSensitivity(tabIndex)));

    repaint();
    updatePointerEditOverlay();
}

void MainView::showTabContextMenuAsync(int tabIndex, juce::Component* anchor)
{
    DebugLog::write("[Tabs] showTabContextMenuAsync | tabIndex=" + juce::String(tabIndex));

    auto& core = processor.getCore();
    const bool canCloseTab = core.getNumTabs() > 1;
    const bool canReloadPlugin = juce::isPositiveAndBelow(tabIndex, core.getNumTabs())
                              && core.getTabModel().getTab(tabIndex).type != PluginSlotType::Empty;

    juce::PopupMenu menu;
    menu.addItem(commandTabContextNewTab, "New Tab");
    menu.addItem(commandTabContextReplacePlugin, "Replace Plugin...");
    menu.addItem(commandTabContextReloadPlugin, "Reload Plugin", canReloadPlugin);
    menu.addItem(commandTabContextClearTab, "Clear Tab");
    menu.addSeparator();
    menu.addItem(commandTabContextCloseTab, "Close Tab", canCloseTab);

    juce::PopupMenu::Options opts;
    if (anchor != nullptr)
        opts = opts.withTargetComponent(anchor);

    menu.showMenuAsync(opts, [safe = juce::Component::SafePointer<MainView>(this), tabIndex](int result)
    {
        if (safe == nullptr || result == 0)
            return;

        safe->handleTabContextCommand(result, tabIndex);
    });
}

void MainView::handleTabContextCommand(int commandId, int tabIndex)
{
    DebugLog::write("[Tabs] handleTabContextCommand | commandId="
                    + juce::String(commandId)
                    + " | tabIndex="
                    + juce::String(tabIndex));

    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    core.setSelectedTabIndex(tabIndex);
    core.getTabModel().selectTab(tabIndex);

    switch (commandId)
    {
        case commandTabContextNewTab:
            newTab();
            return;

        case commandTabContextReplacePlugin:
            refreshFromCore();
            repaint();
            replacePlugin();
            return;

        case commandTabContextReloadPlugin:
        {
            clearHostedEditor();

            if (! core.reloadTabPlugin(tabIndex))
            {
                DebugLog::write("[Plugin] context reload failed | tabIndex=" + juce::String(tabIndex));
                core.setStatusText("Plugin reload failed");
            }
            else
            {
                DebugLog::write("[Plugin] context reload succeeded | tabIndex=" + juce::String(tabIndex));
            }

            core.getTabModel().selectTab(core.getSelectedTabIndex());
            refreshFromCore();
            repaint();
            return;
        }

        case commandTabContextClearTab:
            DebugLog::write("[Tabs] context clear tab | tabIndex=" + juce::String(tabIndex));
            clearHostedEditor();
            core.clearTab(tabIndex);
            soloedTabIndices.removeFirstMatchingValue(tabIndex);

            if (juce::isPositiveAndBelow(tabIndex, manualBypassStates.size()))
                manualBypassStates.set(tabIndex, false);

            applyEffectiveBypassStates();
            core.getTabModel().selectTab(core.getSelectedTabIndex());
            refreshFromCore();
            repaint();
            return;

        case commandTabContextCloseTab:
            DebugLog::write("[Tabs] context close tab | tabIndex=" + juce::String(tabIndex));

            if (core.getNumTabs() <= 1)
                return;

            clearHostedEditor();
            if (core.closeSelectedTab())
            {
                juce::Array<int> updatedSoloIndices;

                for (int i = 0; i < soloedTabIndices.size(); ++i)
                {
                    const int soloIndex = soloedTabIndices.getReference(i);

                    if (soloIndex == tabIndex)
                        continue;

                    if (soloIndex > tabIndex)
                        updatedSoloIndices.add(soloIndex - 1);
                    else
                        updatedSoloIndices.add(soloIndex);
                }

                soloedTabIndices = updatedSoloIndices;

                if (juce::isPositiveAndBelow(tabIndex, manualBypassStates.size()))
                    manualBypassStates.remove(tabIndex);

                applyEffectiveBypassStates();
                core.getTabModel().selectTab(core.getSelectedTabIndex());
                refreshFromCore();
                repaint();
            }
            return;

        default:
            return;
    }
}

void MainView::clearTab(int tabIndex)
{
    DebugLog::write("[Tabs] clearTab | tabIndex=" + juce::String(tabIndex));

    dismissMidiAssignmentsPopup();
    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    core.clearTab(tabIndex);
    soloedTabIndices.removeFirstMatchingValue(tabIndex);

    if (juce::isPositiveAndBelow(tabIndex, manualBypassStates.size()))
        manualBypassStates.set(tabIndex, false);

    applyEffectiveBypassStates();

    if (tabIndex == core.getSelectedTabIndex())
        clearHostedEditor();

    refreshFromCore();
    repaint();
}

void MainView::closeTab(int tabIndex)
{
    DebugLog::write("[Tabs] closeTab | tabIndex=" + juce::String(tabIndex));

    dismissMidiAssignmentsPopup();
    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    core.setSelectedTabIndex(tabIndex);
    core.getTabModel().selectTab(tabIndex);

    if (core.closeSelectedTab())
    {
        juce::Array<int> updatedSoloIndices;

        for (int i = 0; i < soloedTabIndices.size(); ++i)
        {
            const int soloIndex = soloedTabIndices.getReference(i);

            if (soloIndex == tabIndex)
                continue;

            if (soloIndex > tabIndex)
                updatedSoloIndices.add(soloIndex - 1);
            else
                updatedSoloIndices.add(soloIndex);
        }

        soloedTabIndices = updatedSoloIndices;

        if (juce::isPositiveAndBelow(tabIndex, manualBypassStates.size()))
            manualBypassStates.remove(tabIndex);

        applyEffectiveBypassStates();
        core.getTabModel().selectTab(core.getSelectedTabIndex());
        clearHostedEditor();
        refreshFromCore();
        repaint();
    }
}

void MainView::newTab()
{
    DebugLog::write("[Tabs] newTab requested");

    auto& core = processor.getCore();
    core.addTab();
    core.getTabModel().selectTab(core.getSelectedTabIndex());

    manualBypassStates.add(false);
    applyEffectiveBypassStates();
    refreshFromCore();
    repaint();
}

void MainView::closeCurrentTab()
{
    DebugLog::write("[Tabs] closeCurrentTab requested");

    auto& core = processor.getCore();

    if (core.closeSelectedTab())
    {
        core.getTabModel().selectTab(core.getSelectedTabIndex());
        refreshFromCore();
        repaint();
    }
}

void MainView::loadPluginIntoMainSlot()
{
    DebugLog::write("[Plugin] loadPluginIntoMainSlot opened chooser");

    juce::String fileFilter = "*.vst3";

   #if JUCE_PLUGINHOST_VST
    fileFilter += ";*.dll";
   #endif

    juce::FileChooser chooser("Select a plugin to associate with the current tab",
                              {},
                              fileFilter,
                              true,
                              false,
                              getWindowCenterTarget());

    if (chooser.browseForFileToOpen())
    {
        clearHostedEditor();

        auto chosenFile = chooser.getResult();
        auto& core = processor.getCore();

        const bool ok = core.loadMainSlotPluginFromPath(chosenFile.getFullPathName());

        if (ok)
        {
            core.refreshTabModel();
            core.getTabModel().selectTab(core.getSelectedTabIndex());
        }
        else
        {
            core.markDirty();
        }

        refreshFromCore();
        repaint();
    }
}

void MainView::replacePlugin()
{
    DebugLog::write("[Plugin] replacePlugin requested");
    loadPluginIntoMainSlot();
}

void MainView::newPlugin()
{
    DebugLog::write("[Plugin] newPlugin requested");

    auto& core = processor.getCore();

    core.addTab();
    core.getTabModel().selectTab(core.getSelectedTabIndex());

    manualBypassStates.add(false);
    applyEffectiveBypassStates();
    refreshFromCore();
    repaint();

    loadPluginIntoMainSlot();
}

void MainView::openPresetsFolder()
{
    DebugLog::write("[Preset] openPresetsFolder requested");
    AppSettings::getPresetsDirectory().startAsProcess();
}

void MainView::deleteCurrentPreset()
{
    auto currentFile = presetController.getCurrentFile();

    if (! currentFile.existsAsFile())
        return;

    DebugLog::write("[Preset] deleteCurrentPreset requested | file=" + currentFile.getFullPathName());

    juce::AlertWindow alert("Delete Current Preset",
                            "Delete preset file?\n\n" + currentFile.getFullPathName(),
                            juce::AlertWindow::WarningIcon);

    alert.addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert.centreAroundComponent(getWindowCenterTarget(), 460, 220);

    if (alert.runModalLoop() != 1)
        return;

    if (currentFile.deleteFile())
    {
        DebugLog::write("[Preset] deleteCurrentPreset success");

        clearHostedEditor();

        presetController.forgetFile(currentFile);
        appSettings.removeMissingRecentPresetPaths();

        auto& core = processor.getCore();
        core.resetForNewPreset();
        core.setStatusText("Preset deleted");

        auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>();
        if (parentEditor != nullptr)
            parentEditor->setSize(800, 500);

        refreshFromCore();
        repaint();
    }
    else
    {
        DebugLog::write("[Preset] deleteCurrentPreset failed");

        processor.getCore().setStatusText("Preset delete failed");
        refreshDirtyUiOnly();
    }
}

void MainView::savePreset()
{
    DebugLog::write("[Preset] savePreset requested");

    if (presetController.hasCurrentFile())
    {
        saveSessionToFile(presetController.getCurrentFile());
        refreshDirtyUiOnly();
        return;
    }

    savePresetAs();
}

void MainView::savePresetAs()
{
    DebugLog::write("[Preset] savePresetAs requested");

    auto initialDirectory = presetFileHelper.getDefaultPresetDirectory();
    auto suggestedName = presetFileHelper.getSuggestedSaveName();

    juce::File initialFile = suggestedName.isNotEmpty()
        ? initialDirectory.getChildFile(suggestedName).withFileExtension(".xml")
        : initialDirectory.getChildFile("Untitled").withFileExtension(".xml");

    juce::FileChooser chooser("Save PolyHost preset",
                              initialFile,
                              "*.xml",
                              true,
                              false,
                              getWindowCenterTarget());

    if (chooser.browseForFileToSave(true))
    {
        auto file = presetFileHelper.withXmlExtension(chooser.getResult());
        saveSessionToFile(file);
        refreshFromCore();
        repaint();
    }
}

void MainView::updateSaveButtonColours()
{
    const auto colour = processor.getCore().isDirty()
                            ? juce::Colour(0xFF3A7BD5)
                            : ButtonStyling::defaultBackground();

    if (savePresetToolbarButton != nullptr)
        savePresetToolbarButton->setBackgroundColour(colour);

    if (savePresetAsToolbarButton != nullptr)
        savePresetAsToolbarButton->setBackgroundColour(colour);

    toolbar.repaint();
}

void MainView::loadPresetFromFile()
{
    DebugLog::write("[Preset] loadPresetFromFile chooser opened");

    juce::FileChooser chooser("Load PolyHost preset",
                              presetFileHelper.getDefaultPresetDirectory(),
                              "*.xml",
                              true,
                              false,
                              getWindowCenterTarget());

    if (chooser.browseForFileToOpen())
        loadSessionFromFile(chooser.getResult());
}

void MainView::loadRecentPreset(int menuItemID)
{
    DebugLog::write("[Preset] loadRecentPreset | menuItemID=" + juce::String(menuItemID));

    if (! promptToSaveIfNeeded())
        return;

    auto file = recentPresetMenuHelper.getRecentPresetFileForItemId(menuItemID,
                                                                    commandRecentPresetBase);

    if (file.existsAsFile())
    {
        DebugLog::write("[Preset] loadRecentPreset | file=" + file.getFullPathName());
        loadSessionFromFile(file);
    }
}

bool MainView::saveCurrentTabPointerMapToCurrentPresetFile()
{
    auto currentFile = presetController.getCurrentFile();

    if (! currentFile.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(currentFile);

    if (xml == nullptr || ! xml->hasTagName("PolyHostPreset"))
        return false;

    auto& core = processor.getCore();
    const int tabIndex = core.getSelectedTabIndex();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return false;

    auto tabData = core.buildMainTabSessionData();

    int currentTabCounter = 0;

    for (auto* tabXml : xml->getChildIterator())
    {
        if (! tabXml->hasTagName("Tab"))
            continue;

        if (currentTabCounter != tabIndex)
        {
            ++currentTabCounter;
            continue;
        }

        tabXml->removeChildElement(tabXml->getChildByName("PointerJumpPoints"), true);
        tabXml->setAttribute("preferGlobalPointerMap", tabData.preferGlobalPointerMap);

        if (tabData.hasCustomPointerMap)
        {
            auto* pointerPointsXml = tabXml->createNewChildElement("PointerJumpPoints");

            for (auto& point : tabData.pointerJumpPoints)
            {
                auto* pointXml = pointerPointsXml->createNewChildElement("Point");
                pointXml->setAttribute("x", point.x);
                pointXml->setAttribute("y", point.y);
            }
        }

        break;
    }

    return xml->writeTo(currentFile, {});
}

bool MainView::saveSessionToFile(const juce::File& file)
{
    DebugLog::write("[Preset] saveSessionToFile | file=" + file.getFullPathName());

    auto& core = processor.getCore();

    if (showingRoutingView)
    {
        if (auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>())
            core.setRoutingViewSize(parentEditor->getWidth(), parentEditor->getHeight());
    }

    auto sessionData = core.buildSessionData();
    sessionData.name = file.getFileNameWithoutExtension();

    if (SessionManager::saveSessionToFile(sessionData, file))
    {
        DebugLog::write("[Preset] saveSessionToFile success");

        presetController.handleSuccessfulSave(file);
        core.setStatusText("Preset saved");
        core.markClean();
        core.suppressDirtyMarkingFor(2500);
        lastKnownDirtyState = core.isDirty();
        refreshDirtyUiOnly();
        return true;
    }

    DebugLog::write("[Preset] saveSessionToFile failed");

    core.setStatusText("Preset save failed");
    refreshDirtyUiOnly();
    return false;
}

bool MainView::loadSessionFromFile(const juce::File& file)
{
    DebugLog::write("[Preset] loadSessionFromFile | file=" + file.getFullPathName());

    SessionData sessionData;
    juce::StringArray warnings;

    if (! SessionManager::loadSessionFromFile(file, sessionData, warnings))
    {
        DebugLog::write("[Preset] loadSessionFromFile failed during parse/load");

        processor.getCore().setStatusText("Preset load failed");
        refreshDirtyUiOnly();
        return false;
    }

    dismissMidiAssignmentsPopup();
    clearHostedEditor();
    showingRoutingView = false;
    showingMacroMappingsView = false;
    soloedTabIndices.clear();
    manualBypassStates.clear();

    emptyLoadPluginButton.setVisible(false);
    contentPlaceholder.setText("Loading Preset...\n\nPlease wait...", juce::dontSendNotification);
    contentPlaceholder.setVisible(true);
    editorHolder.setVisible(true);
    routingView.setVisible(false);
    macroMappingsView.setVisible(false);
    resized();
    repaint();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);

    if (processor.getCore().restoreSessionData(sessionData, warnings))
    {
        if (warnings.size() > 0)
            DebugLog::write("[Preset] loadSessionFromFile success with warnings | count=" + juce::String(warnings.size()));
        else
            DebugLog::write("[Preset] loadSessionFromFile success");

        presetController.rememberLoadedFile(file);

        if (warnings.size() > 0)
            processor.getCore().setStatusText("Preset loaded with warnings");
        else
            processor.getCore().setStatusText("Preset loaded");

        lastKnownDirtyState = processor.getCore().isDirty();
        refreshFromCore();
        repaint();
        syncManualBypassStatesFromCore();

        // Flag for timer to pick up — deferred so MuLab's init thread doesn't block it
        if (! processor.getCore().getMissingPlugins().isEmpty())
        {
            pendingMissingPluginPrompt = true;
            pendingMissingPluginPromptDelayTicks = 0;
        }

        return true;
    }

    DebugLog::write("[Preset] loadSessionFromFile failed during restoreSessionData");

    processor.getCore().setStatusText("Preset load failed");
    refreshFromCore();
    repaint();
    return false;
}

void MainView::refreshDirtyUiOnly()
{
    auto& core = processor.getCore();

    DebugLog::writeAdvanced("[MainView] refreshDirtyUiOnly | sessionName="
                            + core.getSessionName()
                            + " | dirty="
                            + juce::String(core.isDirty() ? "true" : "false"));

    rebuildPresetDropdown();

    juce::String displayName = core.getSessionName();

    if (core.isDirty())
        displayName += " *";

    presetDropdown.setSelectedId(0, juce::dontSendNotification);
    presetDropdown.setText(displayName, juce::dontSendNotification);

    updateSaveButtonColours();
    repaint();
}

void MainView::resizeParentEditorToFitHostedPlugin()
{
    auto* parentEditor = findParentComponentOfClass<PolyHostPluginEditor>();

    if (parentEditor == nullptr)
        return;

    if (hostedEditor == nullptr)
    {
        DebugLog::writeAdvanced("[HostedEditor] resizeParentEditorToFitHostedPlugin | no hosted editor, using default 800x500");
        parentEditor->setSize(800, 500);
        return;
    }

    int editorWidth = hostedEditor->getWidth();
    int editorHeight = hostedEditor->getHeight();

    if (editorWidth <= 0)
        editorWidth = 800;

    if (editorHeight <= 0)
        editorHeight = 500;

    DebugLog::writeAdvanced("[HostedEditor] resizeParentEditorToFitHostedPlugin | size="
                            + juce::String(editorWidth)
                            + "x"
                            + juce::String(editorHeight));

    parentEditor->resizeToFitContent(editorWidth, editorHeight);
}

void MainView::refreshHostedEditor()
{
    DebugLog::writeAdvanced("[HostedEditor] refreshHostedEditor begin");

    clearHostedEditor();

    auto* instance = processor.getCore().getMainPluginInstance();

    if (instance == nullptr)
    {
        DebugLog::writeAdvanced("[HostedEditor] no plugin instance");
        resizeParentEditorToFitHostedPlugin();
        return;
    }

    if (! instance->hasEditor())
    {
        DebugLog::writeAdvanced("[HostedEditor] plugin has no editor");
        resizeParentEditorToFitHostedPlugin();
        return;
    }

    hostedEditor.reset(instance->createEditorIfNeeded());

    if (hostedEditor != nullptr)
    {
        DebugLog::writeAdvanced("[HostedEditor] editor created successfully");

        editorHolder.addAndMakeVisible(hostedEditor.get());
        hostedEditor->setTopLeftPosition(0, 0);
        hostedEditor->setVisible(true);
        hostedEditor->toFront(true);

        contentPlaceholder.setVisible(false);
        emptyLoadPluginButton.setVisible(false);

        juce::MessageManager::callAsync([safePtr = juce::Component::SafePointer<MainView>(this)]
        {
            if (safePtr == nullptr)
                return;

            if (safePtr->hostedEditor != nullptr)
            {
                safePtr->hostedEditor->setTopLeftPosition(0, 0);
                safePtr->hostedEditor->setVisible(true);
                safePtr->hostedEditor->toFront(true);
            }

            safePtr->resizeParentEditorToFitHostedPlugin();
            safePtr->resized();
            safePtr->repaint();
        });
    }
    else
    {
        DebugLog::writeAdvanced("[HostedEditor] createEditorIfNeeded returned null");
        resizeParentEditorToFitHostedPlugin();
    }

    updatePointerEditOverlay();
}

void MainView::clearHostedEditorForWindowClose()
{
    dismissMidiAssignmentsPopup();
    clearHostedEditor();
    resized();
    repaint();
}

void MainView::recoverCurrentTabAfterWindowReopen()
{
    DebugLog::write("[Window] recoverCurrentTabAfterWindowReopen requested");

    if (showingRoutingView)
        return;

    auto& core = processor.getCore();
    const int tabIndex = core.getSelectedTabIndex();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        return;

    if (core.getTabModel().getTab(tabIndex).type == PluginSlotType::Empty)
    {
        DebugLog::writeAdvanced("[Window] selected tab is empty, refreshing hosted editor only");
        refreshHostedEditorForWindowReopen();
        return;
    }

    clearHostedEditor();

    juce::MessageManager::callAsync([safePtr = juce::Component::SafePointer<MainView>(this), tabIndex]
    {
        if (safePtr == nullptr)
            return;

        if (safePtr->showingRoutingView)
            return;

        auto& core = safePtr->processor.getCore();

        if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
            return;

        core.setSelectedTabIndex(tabIndex);
        core.getTabModel().selectTab(tabIndex);

        if (! core.reloadTabPlugin(tabIndex))
        {
            DebugLog::write("[Window] plugin reload failed after window reopen");
            core.setStatusText("Plugin reload failed after window reopen");
        }
        else
        {
            DebugLog::writeAdvanced("[Window] plugin reload succeeded after window reopen");
        }

        safePtr->refreshFromCore();
        safePtr->repaint();
    });
}

void MainView::refreshHostedEditorForWindowReopen()
{
    DebugLog::writeAdvanced("[Window] refreshHostedEditorForWindowReopen requested");

    if (showingRoutingView)
        return;

    clearHostedEditor();

    juce::MessageManager::callAsync([safePtr = juce::Component::SafePointer<MainView>(this)]
    {
        if (safePtr == nullptr)
            return;

        if (safePtr->showingRoutingView)
            return;

        safePtr->refreshHostedEditor();
        safePtr->resized();
        safePtr->repaint();
    });
}

void MainView::refreshFromCore()
{
    DebugLog::writeAdvanced("[MainView] refreshFromCore | showingRoutingView="
                            + juce::String(showingRoutingView ? "true" : "false")
                            + " | dirty="
                            + juce::String(processor.getCore().isDirty() ? "true" : "false")
                            + " | selectedTab="
                            + juce::String(processor.getCore().getSelectedTabIndex()));

    auto& core = processor.getCore();

    if (! showingRoutingView)
        dismissMidiAssignmentsPopup();

    rebuildPresetDropdown();
    rebuildTabButtons();
    rebuildRoutingView();

    juce::Array<MacroMappingsView::MappingEntry> mappingEntries;
    for (auto& mapping : core.getMacroMappings())
    {
        MacroMappingsView::MappingEntry entry;
        entry.macroIndex = mapping.macroIndex;
        entry.label = mapping.label;
        entry.tabIndex = mapping.tabIndex;
        entry.pluginName = mapping.pluginName;
        entry.parameterIndex = mapping.parameterIndex;
        entry.parameterName = mapping.parameterName;
        entry.enabled = mapping.enabled;
        entry.canMoveUp = core.isMacroMapped(mapping.macroIndex - 1);
        entry.canMoveDown = core.isMacroMapped(mapping.macroIndex + 1);
        mappingEntries.add(entry);
    }

    macroMappingsView.setMappings(mappingEntries);
    macroMappingsView.setUndoAvailable(core.hasMacroMappingsUndoState());

    while (manualBypassStates.size() < core.getNumTabs())
        manualBypassStates.add(false);

    if (! showingRoutingView && ! showingMacroMappingsView)
        refreshHostedEditor();
    else
        clearHostedEditor();

    auto& slot = core.getMainSlot();

    juce::String contentText;

    if (showingRoutingView || showingMacroMappingsView)
    {
        emptyLoadPluginButton.setVisible(false);
        contentText.clear();
    }
    else if (slot.isPluginLoaded())
    {
        emptyLoadPluginButton.setVisible(false);

        if (hostedEditor == nullptr)
        {
            contentText = "Hosted plugin instantiated\n\n"
                          "Slot: " + slot.getSlotName() + "\n"
                          "Status: " + slot.getStatusText() + "\n\n"
                          "This plugin does not provide an editor.";
        }
        else
        {
            contentText.clear();
        }
    }
    else
    {
        emptyLoadPluginButton.setVisible(true);
        emptyLoadPluginButton.toFront(false);
        contentText = "Empty slot - drop or click to load";
    }

    contentPlaceholder.setText(contentText, juce::dontSendNotification);

    routingView.setVisible(showingRoutingView);
    macroMappingsView.setVisible(showingMacroMappingsView);
    editorHolder.setVisible(! showingRoutingView && ! showingMacroMappingsView);

    refreshDirtyUiOnly();
    resized();
    updatePointerEditOverlay();
}

void MainView::paint(juce::Graphics& g)
{
    auto toolbarBackground = juce::Colour(0xFF1D2230);

    g.fillAll(juce::Colour(0xFF23283A));

    auto bounds = getLocalBounds();

    auto menuBarArea = bounds.removeFromTop(24);
    g.setColour(juce::Colour(0xFF1E2430));
    g.fillRect(menuBarArea);

    auto topToolbarArea = bounds.removeFromTop(28);
    g.setColour(toolbarBackground);
    g.fillRect(topToolbarArea);

    auto statusArea = bounds.removeFromBottom(24);
    g.setColour(juce::Colour(0xFF0F3460));
    g.fillRect(statusArea);

    statusArea.removeFromRight(150);

    auto& core = processor.getCore();
    juce::String statusSource = temporaryStatusMessage.isNotEmpty() ? temporaryStatusMessage
                                                                    : core.getStatusText();
    juce::String statusText = "PolyHostInterface VST3  |  " + statusSource;

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(statusText,
               statusArea.reduced(8, 0),
               juce::Justification::centredLeft,
               true);

    auto mainArea = bounds.reduced(2, 2);
    g.setColour(juce::Colour(0xFF2C3E58));
    g.fillRect(mainArea);

    g.setColour(juce::Colour(0xFF3E556F));
    g.drawRect(mainArea, 1);

    auto tabBarArea = mainArea.removeFromTop(30);
    g.setColour(toolbarBackground);
    g.fillRect(tabBarArea);

    g.setColour(juce::Colour(0xFF31445E));
    g.drawLine((float) tabBarArea.getX(),
               (float) tabBarArea.getBottom() - 0.5f,
               (float) tabBarArea.getRight(),
               (float) tabBarArea.getBottom() - 0.5f,
               1.0f);
}

void MainView::resized()
{
    auto area = getLocalBounds();

    auto menuBarArea = area.removeFromTop(24);
    if (menuBar != nullptr)
        menuBar->setBounds(menuBarArea);

    auto topRow = area.removeFromTop(28);

    auto presetArea = topRow.removeFromLeft(280);
    presetArea.removeFromLeft(8);
    presetArea.removeFromRight(6);

    auto presetBounds = presetArea.reduced(2);
    presetBounds = presetBounds.withSizeKeepingCentre(
        presetBounds.getWidth(),
        topRow.getHeight() - 8);
    presetDropdown.setBounds(presetBounds);

    toolbar.setBounds(topRow);

    auto statusArea = area.removeFromBottom(24);
    auto metersArea = statusArea.removeFromRight(150);

    if (statusMeters != nullptr)
        statusMeters->setBounds(metersArea);

    auto contentOuter = area.reduced(12, 8);

    auto tabBarArea = contentOuter.removeFromTop(30);

    const int buttonHeight = 24;
    const int buttonY = tabBarArea.getY() - 2;
    const int arrowButtonWidth = 24;
    const int plusButtonWidth = 24;
    const int controlsGap = 2;
    const int tabGap = 2;

    auto rightControlsArea = tabBarArea.removeFromRight((arrowButtonWidth * 2)
                                                        + plusButtonWidth
                                                        + (controlsGap * 2)
                                                        + 6);

    auto leftArrowBounds = rightControlsArea.removeFromLeft(arrowButtonWidth);
    leftArrowBounds.setY(buttonY);
    leftArrowBounds.setHeight(buttonHeight);
    scrollTabsLeftButton.setBounds(leftArrowBounds);
    rightControlsArea.removeFromLeft(controlsGap);

    auto rightArrowBounds = rightControlsArea.removeFromLeft(arrowButtonWidth);
    rightArrowBounds.setY(buttonY);
    rightArrowBounds.setHeight(buttonHeight);
    scrollTabsRightButton.setBounds(rightArrowBounds);
    rightControlsArea.removeFromLeft(controlsGap);

    auto addButtonBounds = rightControlsArea.removeFromLeft(plusButtonWidth);
    addButtonBounds.setY(buttonY);
    addButtonBounds.setHeight(buttonHeight);
    addTabButton.setBounds(addButtonBounds);

    auto viewportBounds = tabBarArea.reduced(4, 0);
    tabButtonsViewport.setBounds(viewportBounds);

    int x = 0;
    int h = buttonHeight;

    for (int i = 0; i < tabButtons.size(); ++i)
    {
        auto* button = tabButtons[i];
        int w = juce::jlimit(58, 230, 22 + button->getName().length() * 8);
        button->setBounds(x, 0, w, h);
        x += w + tabGap;
    }

    tabButtonsContainer.setSize(juce::jmax(viewportBounds.getWidth(), x), h);
    updateTabScrollButtonState();

    auto contentBounds = contentOuter.reduced(8);
    editorHolder.setBounds(contentBounds);
    routingView.setBounds(contentBounds);
    macroMappingsView.setBounds(contentBounds);

    auto emptyTextBounds = juce::Rectangle<int>(contentBounds.getX(),
                                                contentBounds.getY() + contentBounds.getHeight() / 2 - 90,
                                                contentBounds.getWidth(),
                                                80);
    contentPlaceholder.setBounds(emptyTextBounds);

    auto centredButtonBounds = juce::Rectangle<int>(240, 42)
        .withCentre(contentBounds.getCentre().translated(0, 10));
    emptyLoadPluginButton.setBounds(centredButtonBounds);

    if (hostedEditor != nullptr)
        hostedEditor->setTopLeftPosition(0, 0);
}

void MainView::dismissMidiAssignmentsPopup()
{
    if (midiAssignmentsCallout != nullptr)
    {
        DebugLog::writeAdvanced("[MIDI] dismissMidiAssignmentsPopup | dismissing open callout");
        midiAssignmentsCallout->dismiss();
    }

    midiAssignmentsCallout = nullptr;
    midiAssignmentsAnchor = nullptr;
    midiAssignmentsTabIndex = -1;
}

void MainView::showMidiAssignmentsPopup(int tabIndex, juce::Component* anchorComponent)
{
    DebugLog::write("[MIDI] showMidiAssignmentsPopup | tabIndex=" + juce::String(tabIndex));

    auto previousAnchor = midiAssignmentsAnchor;
    auto previousTabIndex = midiAssignmentsTabIndex;
    const bool wasOpen = (midiAssignmentsCallout != nullptr);

    if (wasOpen)
        dismissMidiAssignmentsPopup();

    if (wasOpen && previousAnchor == anchorComponent && previousTabIndex == tabIndex)
    {
        DebugLog::writeAdvanced("[MIDI] showMidiAssignmentsPopup | dismissed same popup, not reopening");
        return;
    }

    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
    {
        DebugLog::write("[MIDI] showMidiAssignmentsPopup | invalid tabIndex, aborting");
        return;
    }

    core.refreshAvailableMidiInputs();

    auto popup = std::make_unique<MidiAssignmentsPopup>(*this, tabIndex);

    const int popupWidth = 290;
    const int numInputs = core.getAvailableMidiInputNames().size();
    const int popupHeight = 24 + (numInputs * 23) + 8 + 28 + 12;
    popup->setSize(popupWidth, popupHeight);

    DebugLog::writeAdvanced("[MIDI] showMidiAssignmentsPopup | availableInputs="
                            + juce::String(numInputs));

    juce::Rectangle<int> anchorArea;

    if (anchorComponent != nullptr)
        anchorArea = anchorComponent->getScreenBounds();
    else
        anchorArea = getScreenBounds().withSizeKeepingCentre(1, 1);

    midiAssignmentsAnchor = anchorComponent;
    midiAssignmentsTabIndex = tabIndex;

    auto& callout = juce::CallOutBox::launchAsynchronously(std::move(popup),
                                                            anchorArea,
                                                            nullptr);

    midiAssignmentsCallout = &callout;
}

void MainView::showAboutDialog()
{
    DebugLog::write("[UI] showAboutDialog requested");

    auto content = std::make_unique<AboutDialogContent>();
    const int dialogWidth = content->getWidth();
    const int dialogHeight = content->getHeight();

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = "About PolyHostInterface";
    options.dialogBackgroundColour = juce::Colour(0xFF1D2230);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = getWindowCenterTarget();

    if (auto* window = options.launchAsync())
        window->centreAroundComponent(getWindowCenterTarget(), dialogWidth, dialogHeight);
}

bool MainView::isInterestedInFileDrag(const juce::StringArray& files)
{
    DebugLog::write("[DragDrop] isInterestedInFileDrag called | count=" + juce::String(files.size()));

    for (auto& path : files)
    {
        const juce::File file(path);
        DebugLog::writeAdvanced("[DragDrop] candidate path=" + path);

        if (file.hasFileExtension(".vst3"))
        {
            DebugLog::write("[DragDrop] accepted .vst3 file");
            return true;
        }

       #if JUCE_PLUGINHOST_VST
        if (file.hasFileExtension(".dll"))
        {
            DebugLog::write("[DragDrop] accepted .dll file");
            return true;
        }
       #endif
    }

    DebugLog::writeAdvanced("[DragDrop] no accepted files");
    return false;
}

void MainView::filesDropped(const juce::StringArray& files, int x, int y)
{
    DebugLog::write("[DragDrop] filesDropped called | count=" + juce::String(files.size())
                    + " | x=" + juce::String(x)
                    + " | y=" + juce::String(y));

    auto& core = processor.getCore();
    int targetTabIndex = core.getSelectedTabIndex();

    if (! juce::isPositiveAndBelow(targetTabIndex, core.getNumTabs()))
        targetTabIndex = 0;

    for (auto& path : files)
    {
        const juce::File file(path);
        DebugLog::writeAdvanced("[DragDrop] dropped path=" + path);

        if (handleDroppedPluginFile(file, targetTabIndex))
        {
            DebugLog::write("[DragDrop] handled successfully");
            return;
        }
    }

    DebugLog::write("[DragDrop] filesDropped received, but nothing handled");
}

int MainView::promptForDroppedPluginAction(const juce::File& droppedFile, int targetTabIndex) const
{
    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(targetTabIndex, core.getNumTabs()))
        return 0;

    const auto& tab = core.getTabModel().getTab(targetTabIndex);

    if (tab.type == PluginSlotType::Empty)
        return 1;

    auto currentPluginName = tab.name.trim();

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

    w.centreAroundComponent(getWindowCenterTarget(), 460, 220);
    return w.runModalLoop();
}

bool MainView::loadDroppedPluginInNewTab(const juce::File& file)
{
    auto& core = processor.getCore();

    core.addTab();
    core.getTabModel().selectTab(core.getSelectedTabIndex());

    manualBypassStates.add(false);
    applyEffectiveBypassStates();
    refreshFromCore();
    repaint();

    clearHostedEditor();

    if (core.loadMainSlotPluginFromPath(file.getFullPathName()))
    {
        core.refreshTabModel();
        core.getTabModel().selectTab(core.getSelectedTabIndex());
        refreshFromCore();
        repaint();
        return true;
    }

    refreshFromCore();
    repaint();
    return false;
}

bool MainView::handleDroppedPluginFile(const juce::File& file, int targetTabIndex)
{
    const bool isAcceptedFormat = file.hasFileExtension(".vst3")
                               #if JUCE_PLUGINHOST_VST
                                || file.hasFileExtension(".dll")
                               #endif
                                ;

    if (! isAcceptedFormat)
    {
        DebugLog::writeAdvanced("[DragDrop] rejected unsupported file: " + file.getFullPathName());
        return false;
    }

    DebugLog::write("[DragDrop] handleDroppedPluginFile | file=" + file.getFullPathName()
                    + " | targetTabIndex=" + juce::String(targetTabIndex));

    auto& core = processor.getCore();

    if (! juce::isPositiveAndBelow(targetTabIndex, core.getNumTabs()))
    {
        DebugLog::write("[DragDrop] invalid target tab index");
        return false;
    }

    const auto& tab = core.getTabModel().getTab(targetTabIndex);

    if (tab.type == PluginSlotType::Empty)
    {
        DebugLog::write("[DragDrop] target tab is empty, loading directly");

        core.setSelectedTabIndex(targetTabIndex);
        core.getTabModel().selectTab(targetTabIndex);

        clearHostedEditor();

        if (core.loadMainSlotPluginFromPath(file.getFullPathName()))
        {
            DebugLog::write("[DragDrop] plugin loaded into empty tab");
            core.refreshTabModel();
            core.getTabModel().selectTab(core.getSelectedTabIndex());
            refreshFromCore();
            repaint();
            return true;
        }

        DebugLog::write("[DragDrop] failed to load plugin into empty tab");
        refreshFromCore();
        repaint();
        return false;
    }

    const int action = promptForDroppedPluginAction(file, targetTabIndex);

    if (action == 1)
    {
        DebugLog::write("[DragDrop] user chose Open New Tab");
        return loadDroppedPluginInNewTab(file);
    }

    if (action == 2)
    {
        DebugLog::write("[DragDrop] user chose Replace Plugin");

        core.setSelectedTabIndex(targetTabIndex);
        core.getTabModel().selectTab(targetTabIndex);

        clearHostedEditor();

        if (core.loadMainSlotPluginFromPath(file.getFullPathName()))
        {
            DebugLog::write("[DragDrop] plugin replaced successfully");
            core.refreshTabModel();
            core.getTabModel().selectTab(core.getSelectedTabIndex());
            refreshFromCore();
            repaint();
            return true;
        }

        DebugLog::write("[DragDrop] plugin replace failed");
        refreshFromCore();
        repaint();
        return false;
    }

    DebugLog::write("[DragDrop] user cancelled drop action");
    return false;
}

void MainView::addPluginScanFolder()
{
    juce::FileChooser chooser("Select Plugin Scan Folder",
                              juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                              "*",
                              true,
                              false,
                              getWindowCenterTarget());

    if (! chooser.browseForDirectory())
        return;

    auto folder = chooser.getResult();

    if (! folder.isDirectory())
        return;

    appSettings.addPluginScanFolder(folder.getFullPathName());

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folder Added",
        "Added:\n" + folder.getFullPathName(),
        "OK",
        getWindowCenterTarget());
}

void MainView::showPluginScanFolders()
{
    auto folders = appSettings.getPluginScanFolders();

    if (folders.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Plugin Scan Folders",
            "No plugin scan folders have been configured.",
            "OK",
            getWindowCenterTarget());
        return;
    }

    juce::String message = "Configured plugin scan folders:\n\n";

    for (int i = 0; i < folders.size(); ++i)
        message += folders[i] + "\n";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folders",
        message.trimEnd(),
        "OK",
        getWindowCenterTarget());
}

void MainView::clearPluginScanFolders()
{
    if (appSettings.getPluginScanFolders().isEmpty())
        return;

    const bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Clear Plugin Scan Folders",
        "Remove all configured plugin scan folders?",
        "Clear All",
        "Cancel",
        getWindowCenterTarget());

    if (! confirmed)
        return;

    appSettings.setPluginScanFolders({});

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Scan Folders",
        "All plugin scan folders have been cleared.",
        "OK",
        getWindowCenterTarget());
}

bool MainView::tryRepairMissingPlugin(int tabIndex,
                                      const juce::String& pluginName,
                                      const juce::String& pluginStateBase64)
{
    auto& core = processor.getCore();

    // Build start directory from scan folders or app directory
    juce::File startDir = AppSettings::getAppDirectory();
    auto scanFolders = appSettings.getPluginScanFolders();

    if (! scanFolders.isEmpty())
        startDir = juce::File(scanFolders[0]);

    // Let user browse for the replacement file
    juce::FileChooser chooser("Locate Plugin: " + pluginName,
                              startDir,
                              "*.vst3;*.dll",
                              true,
                              false,
                              getWindowCenterTarget());

    if (! chooser.browseForFileToOpen())
        return false;

    auto replacementFile = chooser.getResult();

    if (! replacementFile.existsAsFile())
        return false;

    // Confirm with user
    const bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon,
        "Confirm Replacement Plugin",
        "Use this file as the replacement for:\n"
            + pluginName
            + "\n\nSelected:\n"
            + replacementFile.getFullPathName(),
        "Use Replacement",
        "Cancel",
        getWindowCenterTarget());

    if (! confirmed)
        return false;

    // Switch to the affected tab and load the replacement
    auto previousSelected = core.getSelectedTabIndex();
    core.setSelectedTabIndex(tabIndex);
    core.getTabModel().selectTab(tabIndex);

    clearHostedEditor();

    if (! core.loadMainSlotPluginFromPath(replacementFile.getFullPathName()))
    {
        core.setSelectedTabIndex(previousSelected);
        core.getTabModel().selectTab(previousSelected);
        return false;
    }

    // Restore plugin state if available
    if (pluginStateBase64.isNotEmpty())
    {
        juce::MemoryBlock state;
        if (state.fromBase64Encoding(pluginStateBase64))
            core.setMainSlotPluginState(state.getData(), static_cast<int>(state.getSize()));
    }

    core.refreshTabModel();
    core.getTabModel().selectTab(core.getSelectedTabIndex());
    refreshFromCore();
    repaint();

    return true;
}

void MainView::locateMissingPluginsNow()
{
    auto& core = processor.getCore();
    const auto& missing = core.getMissingPlugins();

    if (missing.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Locate Missing Plugins",
            "There are no unresolved missing plugins.",
            "OK",
            getWindowCenterTarget());
        return;
    }

    juce::StringArray restored;
    juce::StringArray skipped;

    // Work on a copy so we can modify the live list as we go
    auto missingCopy = missing;

    for (int i = 0; i < missingCopy.size(); ++i)
    {
        const auto& entry = missingCopy.getReference(i);

        juce::String message;
        message << "Missing plugin " << (i + 1) << " of " << missingCopy.size() << ":\n\n";
        message << "Name: " << entry.pluginName << "\n";

        if (entry.pluginManufacturer.isNotEmpty())
            message << "Manufacturer: " << entry.pluginManufacturer << "\n";

        if (entry.pluginFormatName.isNotEmpty())
            message << "Format: " << entry.pluginFormatName << "\n";

        message << "\nWould you like to locate this plugin now?";

        const int action = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::WarningIcon,
            "Repair Missing Plugin",
            message,
            "Locate",
            "Skip",
            "Cancel",
            getWindowCenterTarget());

        if (action == 0) // Cancel
        {
            skipped.add(entry.pluginName + " (cancelled)");
            break;
        }

        if (action == 2) // Skip
        {
            skipped.add(entry.pluginName);
            continue;
        }

        // Locate
        if (tryRepairMissingPlugin(entry.tabIndex,
                                   entry.pluginName,
                                   entry.pluginStateBase64))
        {
            restored.add(entry.pluginName);
            core.clearMissingPlugins(); // rebuilt on next load
        }
        else
        {
            skipped.add(entry.pluginName + " (not located)");
        }
    }

    if (restored.isEmpty() && skipped.isEmpty())
        return;

    // Build summary
    juce::String summary;

    if (! restored.isEmpty())
    {
        summary += "Restored:\n";
        for (auto& name : restored)
            summary += "  - " + name + "\n";
        summary += "\n";
    }

    if (! skipped.isEmpty())
    {
        summary += "Not repaired:\n";
        for (auto& name : skipped)
            summary += "  - " + name + "\n";
        summary += "\n";
    }

    if (! restored.isEmpty())
    {
        if (appSettings.getAutoSaveAfterPluginRepair())
        {
            summary += "Preset was saved automatically.";
            savePreset();

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Plugin Repair Complete",
                summary.trimEnd(),
                "OK",
                getWindowCenterTarget());
            return;
        }

        summary += "Save the preset now to store the repaired plugin paths.";

        const bool saveNow = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::InfoIcon,
            "Plugin Repair Complete",
            summary.trimEnd(),
            "Save Now",
            "Later",
            getWindowCenterTarget());

        if (saveNow)
            savePreset();

        return;
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Plugin Repair Complete",
        summary.trimEnd(),
        "OK",
        getWindowCenterTarget());
}

void MainView::showPointerControlSettingsDialog()
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
            configureLabel(adjustModeLabel, "Adjust Mode:", juce::Justification::centredRight);
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

            setSize(460, 690);
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

        juce::Label xCcLabel, yCcLabel, adjustCcLabel, adjustModeLabel, toleranceCcLabel, sensitivityCcLabel, adjustMethodLabel;
        juce::Label xWeightLabel, yWeightLabel, adjustSensitivityLabel, dragReturnDelayLabel;
        juce::Label overlayTransparencyLabel, pointSizeLabel, showCrosshairLabel;
        juce::Label pointRgbLabel, previewRgbLabel, crosshairRgbaLabel;

        juce::Label currentToleranceInfoLabel, currentToleranceLineLabel;

        juce::Label tipXcc, tipYcc, tipAdjustCc, tipAdjustMode, tipAdjustSensitivity, tipSensitivityCc, tipAdjustMethod, tipDragReturnDelay;
        juce::Label tipWeights, tipOverlay, tipPointSize, tipShowCrosshair, tipRgb, tipCrosshairRgba;

        ScrollableTextEditor xCcEditor, yCcEditor, adjustCcEditor, toleranceCcEditor, sensitivityCcEditor;
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

    float currentTolerance = 30.0f;
    auto& core = processor.getCore();
    const int tabIndex = core.getSelectedTabIndex();

    if (juce::isPositiveAndBelow(tabIndex, core.getNumTabs()))
        currentTolerance = core.getTabPointerLaneTolerance(tabIndex);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new PointerControlSettingsContent(appSettings, currentTolerance));
    options.dialogTitle = "Pointer Control Settings";
    options.dialogBackgroundColour = juce::Colour(0xFF4A4A4A);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = getWindowCenterTarget();

    if (auto* dialog = options.launchAsync())
        dialog->centreAroundComponent(getWindowCenterTarget(), 460, 690);
}

