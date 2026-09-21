// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "SpectrumDisplay.h"
#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
// The palette is deliberately a little brighter than the paper. It keeps the
// printed, one-ink feeling of the reference while leaving enough contrast for
// parameter values and host automation controls.
const juce::Colour backgroundColour    (0xff0d0911);
const juce::Colour paperColour         (0xff15101a);
const juce::Colour panelColour         (0xff1a121f);
const juce::Colour panelRaisedColour   (0xff24192b);
const juce::Colour trackColour         (0xff392440);
const juce::Colour lineColour          (0xff754183);
const juce::Colour accentColour        (0xffad62c4);
const juce::Colour accentBrightColour  (0xffd89ae7);
const juce::Colour copperColour        (0xffb66acb);
const juce::Colour copperBrightColour  (0xffe6afea);
const juce::Colour textColour          (0xfff4e9f1);
const juce::Colour mutedTextColour     (0xffc493cd);
const juce::Colour shadowColour        (0xff09060b);

juce::Font makeFont (float height, int style = juce::Font::plain)
{
    juce::ignoreUnused (style);
    static const auto typeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::CormorantGaramond_ttf, BinaryData::CormorantGaramond_ttfSize);
    return juce::Font (juce::FontOptions (typeface).withHeight (height * 1.12f));
}

juce::Font makeMonoFont (float height, int style = juce::Font::plain)
{
    juce::ignoreUnused(style);
    static const auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    return juce::Font(juce::FontOptions(typeface).withHeight(height));
}

float peakToDecibels (float peak)
{
    if (! std::isfinite (peak) || peak <= 0.000001f)
        return -60.0f;

    return juce::jlimit (-60.0f, 6.0f, 20.0f * std::log10 (peak));
}

float normaliseMeter (float decibels)
{
    return juce::jlimit (0.0f, 1.0f, (decibels + 60.0f) / 66.0f);
}

std::uint32_t textureHash (int x, int y) noexcept
{
    auto value = static_cast<std::uint32_t> (x) * 374761393u
               + static_cast<std::uint32_t> (y) * 668265263u
               + 0x9e3779b9u;
    value ^= value >> 13;
    value *= 1274126177u;
    value ^= value >> 16;
    return value;
}

// Fixed, subpixel variations imitate uneven ink without animated noise.
void drawInkBorder (juce::Graphics& g, juce::Rectangle<float> box, float width)
{
    const juce::Point<float> corners[] = { box.getTopLeft(), box.getTopRight(),
                                         box.getBottomRight(), box.getBottomLeft() };
    for (int side = 0; side < 4; ++side)
    {
        const auto from = corners[side];
        const auto delta = corners[(side + 1) % 4] - from;
        const int count = juce::jmax (1, (int) (delta.getDistanceFromOrigin() / 4.0f));
        for (int n = 0; n < count; ++n)
        {
            const auto hash = textureHash (n + (int) box.getX(), side + (int) box.getY());
            if ((hash & 63u) == 0u) continue;
            const float offset = ((float) ((hash >> 8) & 15u) / 15.0f - 0.5f) * 0.65f;
            const auto shift = side % 2 == 0 ? juce::Point<float> (0.0f, offset)
                                            : juce::Point<float> (offset, 0.0f);
            const auto a = from + delta * ((float) n / (float) count) + shift;
            const auto b = from + delta * ((float) (n + 1) / (float) count) + shift;
            g.drawLine ({a, b}, width * (0.65f + (float) (hash & 15u) / 20.0f));
        }
    }
}

void drawCircuitTrace (juce::Graphics& g, juce::Rectangle<float> area,
                       int lane, bool reverse = false)
{
    if (area.getWidth() < 50.0f || area.getHeight() < 12.0f)
        return;

    const auto y = area.getY() + 9.0f + std::fmod (static_cast<float> (lane * 19),
                                                   juce::jmax (12.0f, area.getHeight() - 18.0f));
    const auto left = area.getX() + 12.0f + std::fmod (static_cast<float> (lane * 23), 26.0f);
    const auto right = area.getRight() - 12.0f - std::fmod (static_cast<float> (lane * 17), 22.0f);
    const auto bend = left + (right - left) * (reverse ? 0.72f : 0.34f);

    juce::Path trace;
    trace.startNewSubPath (reverse ? right : left, y);
    trace.lineTo (bend, y);
    trace.lineTo (bend, y + (reverse ? 7.0f : -7.0f));
    trace.lineTo (reverse ? left : right, y + (reverse ? 7.0f : -7.0f));
    g.setColour (accentColour.withAlpha (0.65f));
    g.strokePath (trace, { 0.7f, juce::PathStrokeType::mitered,
                           juce::PathStrokeType::butt });

    g.setColour (accentColour.withAlpha (0.68f));
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ bend, y }));
    g.drawLine (bend - 4.0f, y, bend + 4.0f, y, 0.7f);
    const auto symbolX = left + (right - left) * 0.55f;
    const auto symbolY = y + (reverse ? 7.0f : -7.0f);
    g.setColour (accentColour.withAlpha (0.85f));
    if (lane % 2 == 0) {
        juce::Path resistor;
        resistor.startNewSubPath (symbolX - 10.0f, symbolY);
        for (int n = 0; n < 6; ++n)
            resistor.lineTo (symbolX - 8.0f + (float) n * 3.0f, symbolY + (n % 2 ? 3.0f : -3.0f));
        resistor.lineTo (symbolX + 10.0f, symbolY);
        g.strokePath (resistor, juce::PathStrokeType (0.85f));
    } else {
        g.drawLine (symbolX - 2.0f, symbolY - 4.0f, symbolX - 2.0f, symbolY + 4.0f, 0.9f);
        g.drawLine (symbolX + 2.0f, symbolY - 4.0f, symbolX + 2.0f, symbolY + 4.0f, 0.9f);
    }
    g.drawEllipse (left - 2.0f, y - 2.0f, 4.0f, 4.0f, 0.75f);

}

void drawHalftone (juce::Graphics& g, juce::Rectangle<float> bounds, bool circular)
{
    g.saveState();
    if (circular)
    {
        juce::Path clip;
        clip.addEllipse (bounds);
        g.reduceClipRegion (clip);
    }
    else
        g.reduceClipRegion (bounds.toNearestInt());
    for (float y = bounds.getY(); y < bounds.getBottom(); y += 3.0f)
        for (float x = bounds.getX(); x < bounds.getRight(); x += 3.0f)
        {
            const float shade = juce::jlimit (0.0f, 1.0f,
                1.0f - ((x - bounds.getX()) + (y - bounds.getY()))
                           / juce::jmax (1.0f, bounds.getWidth() + bounds.getHeight()));
            g.setColour (accentBrightColour.withAlpha (0.08f + shade * 0.24f));
            const float dot = 0.7f + shade * 0.8f;
            g.fillRect (x, y, dot, dot);
        }
    g.restoreState();
}

void drawRegistrationMark (juce::Graphics& g, juce::Point<float> centre,
                           float radius, juce::Colour colour, float alpha = 0.55f)
{
    g.setColour (colour.withAlpha (alpha));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f)
                       .withCentre (centre), 0.8f);
    g.drawLine (centre.x - radius - 3.0f, centre.y,
                centre.x + radius + 3.0f, centre.y, 0.7f);
    g.drawLine (centre.x, centre.y - radius - 3.0f,
                centre.x, centre.y + radius + 3.0f, 0.7f);
}


}

//==============================================================================
class RitualAudioProcessorEditor::RitualLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RitualLookAndFeel()
    {
        setColour (juce::Slider::backgroundColourId, trackColour);
        setColour (juce::Slider::trackColourId, copperColour);
        setColour (juce::Slider::thumbColourId, copperBrightColour);
        setColour (juce::Slider::rotarySliderFillColourId, copperColour);
        setColour (juce::Slider::rotarySliderOutlineColourId, trackColour);
        setColour (juce::Slider::textBoxTextColourId, textColour);
        setColour (juce::Slider::textBoxBackgroundColourId, panelRaisedColour);
        setColour (juce::Slider::textBoxOutlineColourId, lineColour);
        setColour (juce::Slider::textBoxHighlightColourId, accentColour.withAlpha (0.45f));
        setColour (juce::Label::textColourId, textColour);
        setColour (juce::TextButton::buttonColourId, panelRaisedColour);
        setColour (juce::TextButton::buttonOnColourId, accentColour.withAlpha (0.8f));
        setColour (juce::TextButton::textColourOffId, mutedTextColour);
        setColour (juce::TextButton::textColourOnId, textColour);
        setColour (juce::ToggleButton::textColourId, textColour);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(makeMonoFont(18.0f));
        return label;
    }
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        juce::LookAndFeel_V4::drawLabel (g, label);
        if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        {
            g.setColour (label.findColour (juce::Label::outlineColourId));
            drawInkBorder (g, label.getLocalBounds().toFloat().reduced (0.5f), 0.8f);
        }
    }
    juce::Font getLabelFont(juce::Label& label) override {
        return dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr
            ? makeMonoFont(18.0f) : label.getFont();
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return makeFont (15.0f, juce::Font::bold);
    }

    static juce::TextLayout tooltipLayout (const juce::String& text)
    {
        juce::AttributedString attributed;
        attributed.setJustification (juce::Justification::centred);
        attributed.append (text, makeMonoFont (13.0f), textColour);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (attributed, 360.0f);
        return layout;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text,
                                         juce::Point<int> screenPos,
                                         juce::Rectangle<int> parentArea) override
    {
        const auto layout = tooltipLayout (text);
        const int width = juce::roundToInt (std::ceil (layout.getWidth())) + 20;
        const int height = juce::roundToInt (std::ceil (layout.getHeight())) + 12;
        return juce::Rectangle<int> (
            screenPos.x > parentArea.getCentreX() ? screenPos.x - width - 12 : screenPos.x + 24,
            screenPos.y > parentArea.getCentreY() ? screenPos.y - height - 6 : screenPos.y + 6,
            width, height).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text,
                      int width, int height) override
    {
        g.fillAll (panelRaisedColour);
        g.setColour (lineColour);
        g.drawRect (0, 0, width, height);
        tooltipLayout (text).draw (g, juce::Rectangle<int> (width, height)
                                       .toFloat().reduced (10.0f, 6.0f));
    }

    int getSliderThumbRadius (juce::Slider&) override
    {
        return 5;
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y,
                                                   (float) width, (float) height)
                              .reduced (8.0f, 4.0f);
        const auto diameter = juce::jmin (area.getWidth(), area.getHeight());

        if (diameter <= 1.0f)
            return;

        const auto dial = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre());
        const auto centre = dial.getCentre();
        const auto radius = diameter * 0.5f;
        const auto stroke = juce::jlimit (1.5f, 2.25f, diameter * 0.022f);
        const auto position = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
        const auto alpha = slider.isEnabled() ? 1.0f : 0.38f;

        // A quiet, inked face keeps the dial legible at a glance.
        g.setColour (shadowColour.withAlpha (0.42f * alpha));
        g.fillEllipse (dial.expanded (2.0f));
        g.setColour (panelRaisedColour.withAlpha (alpha));
        g.fillEllipse (dial);
        drawHalftone (g, dial.reduced (2.0f), true);
        g.setColour (lineColour.withAlpha (0.82f * alpha));
        g.drawEllipse (dial.reduced (1.0f), 1.0f);

        // Sparse index marks give the knob a precision-instrument readout
        // without competing with the active arc or pointer.
        constexpr int tickCount = 10;
        for (int tick = 0; tick <= tickCount; ++tick)
        {
            const auto proportion = (float) tick / (float) tickCount;
            const auto angle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
            const auto direction = juce::Point<float> (std::sin (angle), -std::cos (angle));
            const auto major = (tick % 2 == 0);
            const auto outer = centre + direction * (radius + 3.0f);
            const auto inner = centre + direction * (radius + (major ? -1.0f : 1.0f));
            g.setColour ((tick <= juce::roundToInt (position * (float) tickCount))
                             ? accentBrightColour.withAlpha (0.72f * alpha)
                             : mutedTextColour.withAlpha (0.30f * alpha));
            g.drawLine ({ outer, inner }, major ? 1.0f : 0.65f);
        }

        const auto arcRadius = radius - juce::jmax (3.0f, stroke * 1.6f);
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (trackColour.withAlpha (0.82f * alpha));
        g.strokePath (track, { stroke, juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded });

        if (position > 0.0001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle,
                                 rotaryStartAngle + position * (rotaryEndAngle - rotaryStartAngle),
                                 true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                             .withMultipliedAlpha (alpha));
            g.strokePath (value, { stroke + 0.8f, juce::PathStrokeType::curved,
                                   juce::PathStrokeType::rounded });
        }

        const auto pointerAngle = rotaryStartAngle
                                  + position * (rotaryEndAngle - rotaryStartAngle);
        const auto pointerDirection = juce::Point<float> (std::sin (pointerAngle),
                                                           -std::cos (pointerAngle));
        const auto pointerStart = centre + pointerDirection * radius * 0.10f;
        const auto pointerEnd = centre + pointerDirection * radius * 0.68f;
        g.setColour (slider.findColour (juce::Slider::thumbColourId)
                         .withMultipliedAlpha (alpha));
        g.drawLine ({ pointerStart, pointerEnd }, juce::jmax (2.0f, stroke * 0.9f));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        const auto alpha = slider.isEnabled() ? 1.0f : 0.4f;
        const auto highlighted = slider.isMouseOverOrDragging();
        const auto horizontal = style == juce::Slider::LinearHorizontal
                                || style == juce::Slider::LinearBar
                                || style == juce::Slider::TwoValueHorizontal
                                || style == juce::Slider::ThreeValueHorizontal;
        const auto thickness = juce::jlimit (6.0f, 10.0f,
                                             horizontal ? (float) height * 0.18f
                                                        : (float) width * 0.18f);

        juce::Rectangle<float> track;
        juce::Point<float> thumb;
        if (horizontal)
        {
            track = { (float) x + 4.0f, (float) y + (float) height * 0.5f - thickness * 0.5f,
                      juce::jmax (1.0f, (float) width - 8.0f), thickness };
            thumb = { juce::jlimit (track.getX(), track.getRight(), sliderPos), track.getCentreY() };
        }
        else
        {
            track = { (float) x + (float) width * 0.5f - thickness * 0.5f, (float) y + 4.0f,
                      thickness, juce::jmax (1.0f, (float) height - 8.0f) };
            thumb = { track.getCentreX(), juce::jlimit (track.getY(), track.getBottom(), sliderPos) };
        }

        const auto trackRadius = juce::jmin (track.getWidth(), track.getHeight()) * 0.5f;
        g.setColour (shadowColour.withAlpha (0.32f * alpha));
        g.fillRoundedRectangle (track.expanded (1.5f), trackRadius + 1.5f);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId)
                         .withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (track, trackRadius);
        g.setColour ((highlighted ? accentColour : lineColour)
                         .withAlpha ((highlighted ? 0.92f : 0.66f) * alpha));
        g.drawRoundedRectangle (track, trackRadius, 0.85f);

        juce::Rectangle<float> value = track;
        if (horizontal)
            value.setWidth (juce::jmax (0.0f, thumb.x - track.getX()));
        else
        {
            value.setY (thumb.y);
            value.setHeight (juce::jmax (0.0f, track.getBottom() - thumb.y));
        }
        if (value.getWidth() > 0.0f && value.getHeight() > 0.0f)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId)
                             .withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (value, juce::jmin (value.getWidth(), value.getHeight()) * 0.5f);
        }

        drawHalftone (g, track, false);
        auto thumbBounds = horizontal
                               ? juce::Rectangle<float> (9.0f, 24.0f).withCentre (thumb)
                               : juce::Rectangle<float> (24.0f, 9.0f).withCentre (thumb);
        g.setColour (shadowColour.withAlpha (0.35f * alpha));
        g.fillRoundedRectangle (thumbBounds.translated (0.0f, 1.0f), 1.5f);
        g.setColour (slider.findColour (juce::Slider::thumbColourId)
                         .withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (thumbBounds, 1.5f);
        g.setColour ((highlighted ? accentBrightColour : lineColour)
                         .withAlpha ((highlighted ? 0.98f : 0.82f) * alpha));
        g.drawRoundedRectangle (thumbBounds, 1.5f, 0.9f);

        juce::ignoreUnused (minSliderPos, maxSliderPos);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& background,
                               bool highlighted, bool down) override
    {
        const auto alpha = button.isEnabled() ? 1.0f : 0.45f;
        const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto fill = background.isTransparent() ? panelRaisedColour : background;
        if (down)
            fill = fill.brighter (0.10f);
        else if (highlighted)
            fill = fill.brighter (0.05f);

        g.setColour (shadowColour.withAlpha (0.28f * alpha));
        g.fillRect (bounds.translated (0.0f, 1.0f));
        g.setColour (fill.withMultipliedAlpha (alpha));
        g.fillRect (bounds);
        drawHalftone (g, bounds.reduced (1.0f), false);
        g.setColour ((highlighted || down ? accentColour : lineColour)
                         .withAlpha ((highlighted || down ? 0.88f : 0.68f) * alpha));
        drawInkBorder (g, bounds, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (5.0f, 4.0f);
        const auto size = juce::jlimit (24.0f, 32.0f, juce::jmin (bounds.getHeight() - 4.0f,
                                                                   bounds.getWidth() * 0.32f));
        const auto switchBounds = juce::Rectangle<float> (size, size)
                                      .withCentre ({ bounds.getX() + size * 0.5f,
                                                     bounds.getCentreY() });
        const auto on = button.getToggleState();
        const auto alpha = button.isEnabled() ? 1.0f : 0.45f;
        auto switchColour = on ? accentColour : panelRaisedColour;
        if (highlighted)
            switchColour = switchColour.brighter (0.05f);
        if (down)
            switchColour = switchColour.brighter (0.10f);

        g.setColour (shadowColour.withAlpha (0.28f * alpha));
        g.fillRect (switchBounds.translated (0.0f, 1.0f));
        g.setColour (switchColour.withMultipliedAlpha (alpha));
        g.fillRect (switchBounds);
        g.setColour ((highlighted || on ? accentColour : lineColour)
                         .withAlpha ((highlighted || on ? 0.88f : 0.68f) * alpha));
        drawInkBorder (g, switchBounds, 1.0f);
        if (on)
        {
            g.setColour (copperBrightColour.withAlpha (alpha));
            g.drawLine (switchBounds.getX() + 6.0f, switchBounds.getCentreY(),
                        switchBounds.getCentreX(), switchBounds.getBottom() - 6.0f, 1.4f);
            g.drawLine (switchBounds.getCentreX(), switchBounds.getBottom() - 6.0f,
                        switchBounds.getRight() - 5.0f, switchBounds.getY() + 5.0f, 1.4f);
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId)
                         .withAlpha (alpha));
        g.setFont (makeFont (14.0f, juce::Font::bold));
        g.drawText (button.getButtonText(),
                    juce::Rectangle<float> (switchBounds.getRight() + 8.0f, bounds.getY(),
                                            juce::jmax (1.0f, bounds.getRight() - switchBounds.getRight() - 8.0f),
                                            bounds.getHeight()),
                    juce::Justification::centredLeft, false);
    }
};

//==============================================================================
class RitualAudioProcessorEditor::MeterComponent final : public juce::Component,
                                                         public juce::SettableTooltipClient
{
public:
    explicit MeterComponent(SpectrumTap& tap) : spectrum(tap,makeMonoFont(12.0f)) { addAndMakeVisible(spectrum); }
    void resized() override {
        const int top = 216;
        spectrum.setBounds(12,top,getWidth()-24,getHeight()-top-12);
    }

    void setLevels (float inputPeak, float outputPeak)
    {
        const auto inputDb = peakToDecibels (inputPeak);
        const auto outputDb = peakToDecibels (outputPeak);

        // The UI timer provides a slow enough update cadence to make the meter
        // readable, while the asymmetric release keeps transients visible.
        displayedInput = inputDb > displayedInput ? inputDb
                                                    : displayedInput + (inputDb - displayedInput) * 0.24f;
        displayedOutput = outputDb > displayedOutput ? outputDb
                                                       : displayedOutput + (outputDb - displayedOutput) * 0.24f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (14.0f, 13.0f);
        g.setColour (mutedTextColour);
        g.setFont (makeFont (16.0f, juce::Font::bold));
        g.drawText ("SIGNAL", area.removeFromTop (19.0f), juce::Justification::left, false);

        const auto titleRule = area.removeFromTop (10.0f);
        g.setColour (lineColour.withAlpha (0.7f));
        g.drawLine (titleRule.getX(), titleRule.getCentreY(), titleRule.getRight(),
                    titleRule.getCentreY(), 0.8f);
        g.setColour (accentBrightColour.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (titleRule.getCentre()));
        area.removeFromTop (12.0f);

        const auto meterHeight = 64.0f;
        drawMeter (g, area.removeFromTop (meterHeight), "INPUT", displayedInput, accentColour);
        area.removeFromTop (8.0f);
        drawMeter (g, area.removeFromTop (meterHeight), "OUTPUT", displayedOutput, copperColour);
        area.removeFromTop (14.0f);


    }

private:
    SpectrumDisplay spectrum;

    static void drawMeter (juce::Graphics& g, juce::Rectangle<float> area,
                           const juce::String& name, float decibels,
                           juce::Colour fillColour)
    {
        g.setColour (mutedTextColour);
        g.setFont (makeMonoFont (11.0f));
        g.drawText (name, area.removeFromTop (17.0f), juce::Justification::left, false);
        g.setFont (makeMonoFont (12.0f));
        g.drawText (juce::String (decibels, 1) + " dB", area.removeFromTop (17.0f),
                    juce::Justification::right, false);

        auto track = area.removeFromTop (16.0f).reduced (0.0f, 2.0f);
        g.setColour (shadowColour.withAlpha (0.7f));
        g.fillRect (track.expanded (2.0f, 1.0f));

        const auto segments = juce::jmax (12, juce::jmin (26, (int) (track.getWidth() / 5.0f)));
        const auto gap = 2.0f;
        const auto segmentWidth = juce::jmax (1.0f,
                                              (track.getWidth() - gap * (float) (segments - 1))
                                                  / (float) segments);
        const auto active = juce::roundToInt (normaliseMeter (decibels) * (float) segments);
        for (int i = 0; i < segments; ++i)
        {
            const auto x = track.getX() + (segmentWidth + gap) * (float) i;
            const auto segment = juce::Rectangle<float> (segmentWidth, track.getHeight())
                                     .withPosition (x, track.getY());
            const auto isActive = i < active;
            g.setColour (isActive ? fillColour.withAlpha (0.94f)
                                  : trackColour.withAlpha (0.88f));
            g.fillRect (segment);
            g.setColour (lineColour.withAlpha (0.75f));
            drawInkBorder (g, segment, 0.6f);
            if (isActive && i >= segments - 4)
            {
                g.setColour (accentBrightColour.withAlpha (0.72f));
                g.fillRect (segment.withHeight (2.0f));
            }
        }

        const auto scaleY = track.getBottom() + 9.0f;
        g.setColour (mutedTextColour.withAlpha (0.68f));
        g.setFont (makeMonoFont (10.0f));
        g.drawText ("-60", juce::Rectangle<float> (track.getX(), scaleY, 26.0f, 12.0f),
                    juce::Justification::left, false);
        g.drawText ("-27", juce::Rectangle<float> (track.getCentreX() - 13.0f, scaleY, 26.0f, 12.0f),
                    juce::Justification::centred, false);
        g.drawText ("+6", juce::Rectangle<float> (track.getRight() - 26.0f, scaleY, 26.0f, 12.0f),
                    juce::Justification::right, false);
    }

    float displayedInput = -60.0f;
    float displayedOutput = -60.0f;
};

//==============================================================================
RitualAudioProcessorEditor::RitualAudioProcessorEditor (RitualAudioProcessor& p)
    : AudioProcessorEditor (&p),
      ritualProcessor (p),
      lookAndFeel (std::make_unique<RitualLookAndFeel>()),
      meter (std::make_unique<MeterComponent>(p.spectrumTap)),
      presets {{
          { "SUBTLE",    { 0.65f, 0.40f, 0.35f, 0.90f, 0.15f,  2.0f, 1.0f, 1.0f, 0.0f, 0.0f } },
          { "DAMAGED",  { 0.22f, 0.80f, 0.78f, 1.00f, 0.65f,  8.0f, 3.0f, 1.0f, 0.0f, 0.0f } },
          { "NO PUNCH", { 0.22f, 0.80f, 0.78f, 0.00f, 0.65f,  8.0f, 3.0f, 1.0f, 0.0f, 0.0f } },
          { "EXTREME",   { 0.05f, 1.00f, 0.94f, 1.00f, 0.90f, 16.0f, 7.0f, 1.0f, 0.0f, 0.0f } }
      }}
{
    setOpaque (true);
    setLookAndFeel (lookAndFeel.get());
    tooltips.setLookAndFeel (lookAndFeel.get());
    buildPaperTexture();

    titleLabel.setText ("Ritual Codec", juce::dontSendNotification);
    titleLabel.setFont (makeFont (38.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accentBrightColour);
    titleLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("spectral decay / transient ceremony", juce::dontSendNotification);
    subtitleLabel.setFont (makeFont (13.0f));
    subtitleLabel.setColour (juce::Label::textColourId, accentBrightColour);
    subtitleLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (subtitleLabel);

    statusLabel.setText ("LIVE / SPECTRAL DAMAGE", juce::dontSendNotification);
    statusLabel.setFont (makeMonoFont (10.0f));
    statusLabel.setColour (juce::Label::textColourId, accentBrightColour);
    statusLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (statusLabel);

    codecButton.setButtonText ("CODEC: CLASSIC");
    codecButton.setClickingTogglesState (true);
    codecButton.setTooltip ("Switch to the experimental Ritual bitstream. Budget sets the packet budget; Mix, Output and Bypass remain active. Competition, Memory, Punch and Crunch shape the encoded audio. Clock, Downsample and classic presets are inactive in RIT mode.");
    addAndMakeVisible (codecButton);
    codecAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        ritualProcessor.parameters, "bitstream", codecButton);

    spectralHeading.setText ("SPECTRAL DAMAGE", juce::dontSendNotification);
    timingHeading.setText ("TIME + OUTPUT", juce::dontSendNotification);
    for (auto* heading : { &spectralHeading, &timingHeading })
    {
        heading->setFont (makeFont (16.0f, juce::Font::bold));
        heading->setColour (juce::Label::textColourId, accentBrightColour);
        heading->setJustificationType (juce::Justification::left);
        addAndMakeVisible (*heading);
    }

    latencyLabel.setFont (makeMonoFont (10.0f, juce::Font::bold));
    latencyLabel.setColour (juce::Label::textColourId, mutedTextColour);
    latencyLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (latencyLabel);

    presetStatusLabel.setText ("CUSTOM", juce::dontSendNotification);
    presetStatusLabel.setFont (makeMonoFont (10.0f));
    presetStatusLabel.setColour (juce::Label::textColourId, copperBrightColour);
    presetStatusLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (presetStatusLabel);

    controlSliders = { &budgetSlider, &competitionSlider, &memorySlider, &punchSlider,
                       &crunchSlider, &clockSlider, &downsampleSlider, &mixSlider,
                       &outputSlider };

    const std::array<const char*, 9> labelNames {
        "BUDGET", "COMPETITION", "MEMORY", "PUNCH", "CRUNCH",
        "CLOCK", "DOWNSAMPLE", "MIX", "OUTPUT"
    };
    const std::array<const char*, 9> parameterIDs {
        "budget", "competition", "memory", "punch", "crunch",
        "clock", "downsample", "mix", "output"
    };
    const std::array<const char*, 9> controlTooltips {
        "Spectral precision budget. Lower values collapse detail into broader, more unstable shapes.",
        "Lets energetic bands win more of the available precision, making dense material fight for focus.",
        "How long each band keeps its spectral shape. High memory creates stale, smeared colour.",
        "Transient rescue: blends delayed original PCM into detected attacks so kick and clap edges stay crisp.",
        "Amplitude quantisation. Higher values add stepped levels and brittle digital grain.",
        "Shape update interval in spectral hops. Higher values freeze band shapes for longer.",
        "Global sample-and-hold interval. Higher values make the waveform staircase-like.",
        "Dry/wet blend between the input and the Ritual Codec texture.",
        "Output gain after processing, in decibels."
    };
    const std::array<double, 9> minima { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, -24.0 };
    const std::array<double, 9> maxima { 1.0, 1.0, 1.0, 1.0, 1.0, 64.0, 32.0, 1.0, 12.0 };
    const std::array<double, 9> intervals { 0.001, 0.001, 0.001, 0.001, 0.001, 1.0, 1.0, 0.001, 0.1 };

    for (size_t i = 0; i < controlSliders.size(); ++i)
    {
        controlLabels[i].setText (labelNames[i], juce::dontSendNotification);
        controlLabels[i].setFont (makeFont (15.0f, juce::Font::bold));
        controlLabels[i].setColour (juce::Label::textColourId, mutedTextColour);
        controlLabels[i].setJustificationType (juce::Justification::centred);
        controlLabels[i].setTooltip (controlTooltips[i]);
        addAndMakeVisible (controlLabels[i]);

        configureSlider (*controlSliders[i], parameterIDs[i], minima[i], maxima[i], intervals[i],
                         controlTooltips[i]);
        addAndMakeVisible (*controlSliders[i]);
    }

    // Attachments must be created after the sliders have their basic geometry,
    // and must be destroyed before the sliders. The APVTS remains the authoritative state.
    for (size_t i = 0; i < controlSliders.size(); ++i)
        sliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            ritualProcessor.parameters, parameterIDs[i], *controlSliders[i]);

    configureDisplayFormatting();
    for (auto* slider : controlSliders)
        slider->updateText();

    bypassButton.setButtonText ("BYPASS");
    bypassButton.setTooltip ("Bypass processing while retaining Ritual Codec's fixed latency.");
    bypassButton.setColour (juce::ToggleButton::textColourId, textColour);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        ritualProcessor.parameters, "bypass", bypassButton);

    for (size_t i = 0; i < presetButtons.size(); ++i)
    {
        presetButtons[i].setButtonText (presets[i].name);
        presetButtons[i].setTooltip (juce::String ("Load the ") + presets[i].name
                                     + " Ritual Codec starting point.");
        presetButtons[i].setClickingTogglesState (false);
        presetButtons[i].onClick = [this, i] { applyPreset ((int) i); };
        addAndMakeVisible (presetButtons[i]);
    }

    meter->setTooltip ("Input and post-effect peak levels. Meter values are shown in dBFS.");
    addAndMakeVisible (*meter);

    // Resize constraints can synchronously call resized(); all control pointers
    // and child components must be initialized before enabling them.
    setResizable (true, true);
    setResizeLimits (820, 540, 1640, 1080);
    setSize (1000, 650);
    updateMetersAndStatus();
    startTimerHz (30);
}

RitualAudioProcessorEditor::~RitualAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    tooltips.setLookAndFeel (nullptr);
}

void RitualAudioProcessorEditor::buildPaperTexture()
{
    // This image is generated once per editor. Keeping the seed and tile size
    // fixed makes the paper feel deterministic and keeps paint() inexpensive.
    paperTexture = juce::Image (juce::Image::ARGB, 192, 192, true);
    for (int y = 0; y < paperTexture.getHeight(); ++y)
        for (int x = 0; x < paperTexture.getWidth(); ++x)
        {
            const auto hash = textureHash (x, y);
            const auto grain = (hash & 0xffu);
            if (grain < 95u)
            {
                const auto alpha = 0.07f + (float) ((hash >> 8) & 0x1fu) / 180.0f;
                paperTexture.setPixelAt (x, y, accentBrightColour.withAlpha (alpha));
            }
            else if (grain > 185u)
            {
                paperTexture.setPixelAt (x, y, shadowColour.withAlpha (0.23f));
            }
        }

    juce::Graphics textureGraphics (paperTexture);
    textureGraphics.setColour (lineColour.withAlpha (0.025f));
    for (int y = 16; y < paperTexture.getHeight(); y += 29)
        textureGraphics.drawHorizontalLine (y, 0.0f, (float) paperTexture.getWidth());
}

void RitualAudioProcessorEditor::configureSlider (juce::Slider& slider,
                                                  const juce::String& parameterID,
                                                  double minimum, double maximum,
                                                  double interval,
                                                  const juce::String& tooltip)
{
    slider.setComponentID (parameterID);
    slider.setName (parameterID);
    const auto linear = parameterID == "mix" || parameterID == "output";
    slider.setSliderStyle (linear ? juce::Slider::LinearHorizontal
                                  : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 24);
    slider.setRange (minimum, maximum, interval);
    slider.setNumDecimalPlacesToDisplay (interval >= 1.0 ? 0 : (parameterID == "output" ? 1 : 2));
    slider.setTooltip (tooltip);
    slider.setColour (juce::Slider::backgroundColourId, trackColour);
    slider.setColour (juce::Slider::trackColourId, linear ? accentColour : copperColour);
    slider.setColour (juce::Slider::thumbColourId, linear ? accentBrightColour : copperBrightColour);
    slider.setColour (juce::Slider::rotarySliderFillColourId, copperColour);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, trackColour);
    slider.setColour (juce::Slider::textBoxTextColourId, textColour);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, panelRaisedColour);
    slider.setColour (juce::Slider::textBoxOutlineColourId, lineColour);
    slider.setColour (juce::Slider::textBoxHighlightColourId, accentColour.withAlpha (0.45f));
    slider.setScrollWheelEnabled (true);
}

void RitualAudioProcessorEditor::configureDisplayFormatting()
{
    const auto formatNormalised = [] (double value) { return juce::String (value, 2); };
    const auto parseNormalised = [] (const juce::String& text) { return text.getDoubleValue(); };

    for (size_t i = 0; i < 5; ++i)
    {
        controlSliders[i]->textFromValueFunction = formatNormalised;
        controlSliders[i]->valueFromTextFunction = parseNormalised;
    }

    controlSliders[5]->textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value));
    };
    controlSliders[5]->valueFromTextFunction = [] (const juce::String& text)
    {
        return (double) juce::roundToInt (text.getDoubleValue());
    };
    controlSliders[6]->textFromValueFunction = controlSliders[5]->textFromValueFunction;
    controlSliders[6]->valueFromTextFunction = controlSliders[5]->valueFromTextFunction;

    mixSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value * 100.0)) + "%";
    };
    mixSlider.valueFromTextFunction = [] (const juce::String& input)
    {
        auto text = input.trim();
        if (text.endsWith ("%"))
            return text.dropLastCharacters (1).getDoubleValue() / 100.0;
        return text.getDoubleValue();
    };

    outputSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " dB";
    };
    outputSlider.valueFromTextFunction = [] (const juce::String& input)
    {
        auto text = input.trim();
        if (text.endsWithIgnoreCase ("db"))
            text = text.dropLastCharacters (2).trim();
        return text.getDoubleValue();
    };
}

void RitualAudioProcessorEditor::layoutControl (juce::Label& label,
                                                juce::Slider& slider,
                                                juce::Rectangle<int> area)
{
    area = area.reduced (7, 10);
    label.setBounds (area.removeFromTop (21));
    if (slider.getSliderStyle() == juce::Slider::LinearHorizontal)
    {
        const int height = juce::jmin (86, area.getHeight());
        slider.setBounds (area.withSizeKeepingCentre (area.getWidth(), height).reduced (2, 0));
    }
    else
        slider.setBounds (area.withSizeKeepingCentre (area.getWidth(),
                          juce::jmin (158, area.getHeight())).reduced (2, 0));
}

void RitualAudioProcessorEditor::setParameterValue (const char* parameterID, float value)
{
    if (auto* parameter = ritualProcessor.parameters.getParameter (parameterID))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

bool RitualAudioProcessorEditor::parameterMatches (const char* parameterID, float value) const
{
    if (auto* parameter = ritualProcessor.parameters.getParameter (parameterID))
        return std::abs (parameter->convertFrom0to1 (parameter->getValue()) - value) < 0.0006f;

    return false;
}

bool RitualAudioProcessorEditor::presetMatches (const Preset& preset) const
{
    static constexpr std::array<const char*, 10> ids {
        "budget", "competition", "memory", "punch", "crunch",
        "clock", "downsample", "mix", "output", "bypass"
    };

    for (size_t i = 0; i < ids.size(); ++i)
        if (! parameterMatches (ids[i], preset.values[i]))
            return false;

    return true;
}

void RitualAudioProcessorEditor::applyPreset (int presetIndex)
{
    if (! juce::isPositiveAndBelow (presetIndex, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) presetIndex];
    static constexpr std::array<const char*, 10> ids {
        "budget", "competition", "memory", "punch", "crunch",
        "clock", "downsample", "mix", "output", "bypass"
    };

    for (size_t i = 0; i < ids.size(); ++i)
        setParameterValue (ids[i], preset.values[i]);

    activePreset = presetIndex;
    presetStatusLabel.setText (preset.name, juce::dontSendNotification);
    presetStatusLabel.setColour (juce::Label::textColourId, copperBrightColour);
}

void RitualAudioProcessorEditor::refreshPresetIndicator()
{
    if (ritualProcessor.parameters.getRawParameterValue ("bitstream")->load() >= 0.5f)
    {
        presetStatusLabel.setText ("RIT EXP", juce::dontSendNotification);
        return;
    }
    if (activePreset >= 0 && ! presetMatches (presets[(size_t) activePreset]))
    {
        activePreset = -1;
        presetStatusLabel.setText ("CUSTOM", juce::dontSendNotification);
        presetStatusLabel.setColour (juce::Label::textColourId, copperBrightColour);
    }
    presetStatusLabel.setText (activePreset >= 0 ? presets[(size_t) activePreset].name : "CUSTOM",
                              juce::dontSendNotification);
}

void RitualAudioProcessorEditor::updateMetersAndStatus()
{
    const bool streaming = ritualProcessor.parameters.getRawParameterValue ("bitstream")->load() >= 0.5f;
    codecButton.setButtonText (streaming ? "CODEC: RIT" : "CODEC: CLASSIC");
    for (size_t i = 1; i <= 6; ++i)
    {
        const bool enabled = ! streaming || i <= 4;
        controlSliders[i]->setEnabled (enabled);
        controlLabels[i].setAlpha (enabled ? 1.0f : 0.4f);
    }
    for (auto& button : presetButtons) button.setEnabled (! streaming);
    budgetSlider.setTooltip (streaming
        ? "Encoded packet budget. Actual kbps is shown in the header and depends on sample rate. Transient energy and sparse attacks are carried in the packets."
        : "Spectral precision budget: lower values create stronger digital artifacts.");
    controlLabels[0].setTooltip (budgetSlider.getTooltip());
    const std::array<const char*, 4> ritTips {
        "RIT: favour strong spectral components at high values; balance selection across frequency bands at low values.",
        "RIT: retain previous spectral shapes in the encoded audio. Higher values smear and prolong timbres.",
        "RIT: preserve sparse attacks and release spectral memory on transients. No original PCM bypass is stored.",
        "RIT: coarsen encoded coefficient values for digital grain. Packet bitrate stays fixed."
    };
    const std::array<const char*, 4> classicTips {
        "Lets energetic bands win more of the available precision, making dense material fight for focus.",
        "How long each band keeps its spectral shape. High memory creates stale, smeared colour.",
        "Transient rescue: blends delayed original PCM into detected attacks so kick and clap edges stay crisp.",
        "Amplitude quantisation. Higher values add stepped levels and brittle digital grain."
    };
    for (size_t i = 0; i < ritTips.size(); ++i)
    {
        const auto text = streaming ? ritTips[i] : classicTips[i];
        controlSliders[i + 1]->setTooltip (text);
        controlLabels[i + 1].setTooltip (text);
    }
    if (meter != nullptr)
    {
        meter->setLevels (ritualProcessor.inputPeak.load (std::memory_order_relaxed),
                          ritualProcessor.outputPeak.load (std::memory_order_relaxed));
    }

    const bool bypassed = ritualProcessor.parameters.getRawParameterValue ("bypass")->load() >= 0.5f;
    statusLabel.setText (bypassed ? "BYPASSED / LATENCY ALIGNED" :
                        streaming ? "RIT / " + juce::String (ritualProcessor.streamBitrateKbps.load (std::memory_order_relaxed), 1) + " kbps" : "LIVE / SPECTRAL DAMAGE",
                         juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           bypassed ? mutedTextColour : accentBrightColour);
    const auto sampleRate = ritualProcessor.getSampleRate();
    juce::String latencyText ("FIXED LATENCY  /  256 samples");
    if (std::isfinite (sampleRate) && sampleRate > 0.0)
        latencyText << "  /  " << juce::String (256000.0 / sampleRate, 2) << " ms";
    latencyLabel.setText (latencyText, juce::dontSendNotification);
}

void RitualAudioProcessorEditor::timerCallback()
{
    updateMetersAndStatus();
    refreshPresetIndicator();
}

void RitualAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto whole = getLocalBounds();
    juce::ColourGradient backgroundGradient (backgroundColour, 0.0f, 0.0f,
                                             paperColour, 0.0f,
                                             (float) whole.getBottom(), false);
    g.setGradientFill (backgroundGradient);
    g.fillAll();

    const auto frame = whole.toFloat().reduced (10.0f);
    g.setColour (lineColour.withAlpha (0.78f));
    drawInkBorder (g, frame, 1.0f);
    g.setColour (accentColour.withAlpha (0.38f));
    drawInkBorder (g, frame.reduced (4.0f), 0.7f);
    g.setColour (accentBrightColour.withAlpha (0.68f));
    g.fillRect (frame.getX() + 16.0f, frame.getY() - 1.0f, 18.0f, 2.0f);
    g.fillRect (frame.getRight() - 34.0f, frame.getBottom() - 1.0f, 18.0f, 2.0f);

    auto content = whole.reduced (20);
    auto header = content.removeFromTop (84).toFloat();
    auto footer = content.removeFromBottom (50).toFloat();
    content.removeFromTop (12);
    auto body = content.toFloat();
    const auto signalWidth = juce::jlimit (168.0f, 260.0f, body.getWidth() * 0.23f);
    auto signal = body.removeFromLeft (signalWidth);
    body.removeFromLeft (12.0f);
    const auto main = body;

    // Header plaque and title circuitry.
    g.setColour (panelColour.withAlpha (0.82f));
    g.fillRect (header);
    g.setColour (lineColour.withAlpha (0.84f));
    drawInkBorder (g, header.reduced (0.5f), 1.0f);
    // Continuous full-width routing behind the transparent header labels.
    // Clipping out label bounds creates conspicuous rectangular gaps in the traces.
    g.saveState();
    g.reduceClipRegion (header.toNearestInt());
    const auto circuitArea = header.reduced (4.0f, 2.0f);
    for (int i = 0; i < 5; ++i)
    {
        drawCircuitTrace (g, circuitArea, i, i % 2 != 0);
        const float x = header.getX() + header.getWidth() * (0.12f + 0.17f * (float) i);
        const float y = header.getY() + 12.0f + (float) (i % 3) * 22.0f;
        juce::Path branch;
        branch.startNewSubPath (x - 28.0f, y);
        branch.lineTo (x - 10.0f, y);
        branch.lineTo (x, y + 10.0f);
        branch.lineTo (x + 26.0f, y + 10.0f);
        g.setColour (accentColour.withAlpha (0.5f));
        g.strokePath (branch, juce::PathStrokeType (0.8f));
        if (i % 2 == 0)
        {
            juce::Path diode;
            diode.addTriangle (x + 5.0f, y + 6.0f, x + 5.0f, y + 14.0f,
                               x + 13.0f, y + 10.0f);
            g.strokePath (diode, juce::PathStrokeType (0.8f));
            g.drawLine (x + 14.0f, y + 6.0f, x + 14.0f, y + 14.0f, 0.8f);
        }
        else
            g.drawEllipse (x + 5.0f, y + 6.0f, 8.0f, 8.0f, 0.8f);
        g.drawEllipse (x + 24.0f, y + 8.0f, 4.0f, 4.0f, 0.8f);
    }
    g.restoreState();
    const auto pulseCentre = juce::Point<float> (header.getRight() - 165.0f,
                                                  header.getCentreY() - 2.0f);
    g.setColour (lineColour.withAlpha (0.85f));
    g.drawEllipse (juce::Rectangle<float> (50.0f, 50.0f).withCentre (pulseCentre), 1.0f);
    g.drawEllipse (juce::Rectangle<float> (42.0f, 42.0f).withCentre (pulseCentre), 0.7f);
    juce::Path pulse;
    pulse.startNewSubPath (pulseCentre.x - 20.0f, pulseCentre.y);
    pulse.lineTo (pulseCentre.x - 11.0f, pulseCentre.y);
    pulse.lineTo (pulseCentre.x - 5.0f, pulseCentre.y - 13.0f);
    pulse.lineTo (pulseCentre.x + 1.0f, pulseCentre.y + 14.0f);
    pulse.lineTo (pulseCentre.x + 7.0f, pulseCentre.y - 7.0f);
    pulse.lineTo (pulseCentre.x + 12.0f, pulseCentre.y);
    pulse.lineTo (pulseCentre.x + 20.0f, pulseCentre.y);
    g.setColour (accentBrightColour.withAlpha (0.9f));
    g.strokePath (pulse, { 1.25f, juce::PathStrokeType::mitered,
                           juce::PathStrokeType::butt });

    // Main body panels.
    g.setColour (panelColour.withAlpha (0.9f));
    g.fillRect (signal);
    g.fillRect (main);
    g.setColour (lineColour.withAlpha (0.5f));
    drawInkBorder (g, signal.reduced (0.5f), 1.0f);
    drawInkBorder (g, main.reduced (0.5f), 1.0f);
    



    auto sectionBody = main;
    sectionBody.removeFromTop (36);
    const auto rowOneHeight = (sectionBody.getHeight() - 34) / 2;
    sectionBody.removeFromTop (rowOneHeight);
    const auto timingBand = sectionBody.removeFromTop (34).toFloat();
    g.setColour (lineColour.withAlpha (0.72f));
    g.drawLine (main.getX() + 8.0f, timingBand.getY(), main.getRight() - 8.0f,
                timingBand.getY(), 0.8f);
    g.drawLine (main.getX() + 8.0f, timingBand.getBottom(), main.getRight() - 8.0f,
                timingBand.getBottom(), 0.55f);
    // A few registration marks tie the two panels together.
    drawRegistrationMark (g, { main.getX() + 10.0f, main.getY() + 30.0f }, 5.0f,
                           accentColour, 0.3f);
    drawRegistrationMark (g, { main.getRight() - 10.0f, main.getBottom() - 26.0f }, 5.0f,
                           accentColour, 0.3f);


    // Footer rule and tiny calibration traces.
    g.setColour (lineColour.withAlpha (0.78f));
    g.drawLine (footer.getX(), footer.getY(), footer.getRight(), footer.getY(), 0.8f);

}

void RitualAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (! featheredTexture.isNull()) g.drawImageAt (featheredTexture, 0, 0);
}

void RitualAudioProcessorEditor::rebuildTextureMask()
{
    if (paperTexture.isNull() || getWidth() <= 0 || getHeight() <= 0) return;
    struct Region { juce::Rectangle<float> bounds; bool circle; };
    std::vector<Region> regions;
    const auto add = [&] (juce::Rectangle<float> r, bool circle = false)
    { if (! r.isEmpty()) regions.push_back ({r, circle}); };
    const auto addLabel = [&] (juce::Label& label)
    {
        auto text = label.getText();
        if (&label == &statusLabel) text = "BYPASSED / LATENCY ALIGNED";
        if (&label == &latencyLabel) text = "FIXED LATENCY  /  256 samples  /  5.33 ms";
        if (&label == &presetStatusLabel) text = "DAMAGED";
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (label.getFont(), text, 0, 0);
        const auto ink = glyphs.getBoundingBox (0, -1, true);
        const auto box = label.getBounds().toFloat().reduced (2.0f, 0.0f);
        const float w = juce::jmin (box.getWidth(), ink.getWidth() + 4.0f);
        const float h = juce::jmin (box.getHeight(), ink.getHeight() + 4.0f);
        const auto align = label.getJustificationType();
        const float x = align.testFlags (juce::Justification::right) ? box.getRight() - w
                      : align.testFlags (juce::Justification::horizontallyCentred)
                          ? box.getCentreX() - w * 0.5f : box.getX();
        add ({x, box.getCentreY() - h * 0.5f, w, h});
    };
    for (auto* label : { &titleLabel, &subtitleLabel, &statusLabel, &spectralHeading,
                         &timingHeading, &latencyLabel, &presetStatusLabel }) addLabel (*label);
    for (auto& label : controlLabels) addLabel (label);
    for (auto* slider : controlSliders)
    {
        const auto layout = slider->getLookAndFeel().getSliderLayout (*slider);
        const auto origin = slider->getPosition().toFloat();
        auto area = layout.sliderBounds.toFloat().translated (origin.x, origin.y);
        add (layout.textBoxBounds.toFloat().translated (origin.x, origin.y));
        if (slider->getSliderStyle() == juce::Slider::LinearHorizontal)
            add (area.withSizeKeepingCentre (area.getWidth(), 26.0f));
        else
        {
            area.reduce (8.0f, 4.0f);
            const float diameter = juce::jmin (area.getWidth(), area.getHeight()) + 6.0f;
            add (area.withSizeKeepingCentre (diameter, diameter), true);
        }
    }
    for (auto& button : presetButtons) add (button.getBounds().toFloat().reduced (1.0f));
    add (codecButton.getBounds().toFloat().reduced (1.0f));
    add (bypassButton.getBounds().toFloat().withSizeKeepingCentre
          (static_cast<float> (bypassButton.getWidth()) - 10.0f, 34.0f));
    // Meter graphics occupy just the top section; the analyzer has its own bounds.
    const auto m = meter->getBounds().toFloat();
    add ({m.getX() + 14, m.getY() + 13, 62, 19});
    add ({m.getX() + 14, m.getY() + 54, m.getWidth() - 28, 64});
    add ({m.getX() + 14, m.getY() + 126, m.getWidth() - 28, 64});
    add ({m.getX() + 12, m.getY() + 216, m.getWidth() - 24, m.getHeight() - 228});

    const int width = getWidth(), height = getHeight();
    std::vector<float> opacity ((size_t) width * (size_t) height, 0.60f);
    constexpr float feather = 16.0f;
    for (const auto& region : regions)
    {
        const auto pixels = region.bounds.expanded (feather).getSmallestIntegerContainer()
                               .getIntersection (getLocalBounds());
        for (int y = pixels.getY(); y < pixels.getBottom(); ++y)
            for (int x = pixels.getX(); x < pixels.getRight(); ++x)
            {
                const auto point = juce::Point<float> ((float) x + 0.5f, (float) y + 0.5f);
                float distance;
                if (region.circle)
                    distance = point.getDistanceFrom (region.bounds.getCentre()) - region.bounds.getWidth() * 0.5f;
                else
                {
                    const float dx = juce::jmax (region.bounds.getX() - point.x, 0.0f, point.x - region.bounds.getRight());
                    const float dy = juce::jmax (region.bounds.getY() - point.y, 0.0f, point.y - region.bounds.getBottom());
                    distance = std::sqrt (dx * dx + dy * dy);
                }
                const float t = juce::jlimit (0.0f, 1.0f, distance / feather);
                const float alpha = 0.12f + 0.48f * t * t * (3.0f - 2.0f * t);
                auto& value = opacity[(size_t) y * (size_t) width + (size_t) x];
                value = juce::jmin (value, alpha);
            }
    }
    featheredTexture = juce::Image (juce::Image::ARGB, width, height, true);
    juce::Image::BitmapData bitmap (featheredTexture, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            bitmap.setPixelColour (x, y, paperTexture.getPixelAt (x % paperTexture.getWidth(), y % paperTexture.getHeight())
                .withMultipliedAlpha (opacity[(size_t) y * (size_t) width + (size_t) x]));
}

void RitualAudioProcessorEditor::resized()
{
    auto content = getLocalBounds().reduced (20);
    auto header = content.removeFromTop (84);
    titleLabel.setBounds (header.getX() + 14, header.getY() + 3,
                          juce::jmax (180, header.getWidth() - 390), 43);
    subtitleLabel.setBounds (header.getX() + 15, header.getY() + 49,
                             juce::jmax (180, header.getWidth() - 410), 20);
    statusLabel.setBounds (header.getRight() - 238, header.getY() + 55, 226, 18);
    codecButton.setBounds (header.getRight() - 140, header.getY() + 8, 128, 24);

    auto footer = content.removeFromBottom (50);
    latencyLabel.setBounds (footer.getX(), footer.getCentreY() - 10,
                            246, 20);

    auto presetStatusArea = footer.removeFromRight (78);
    presetStatusLabel.setBounds (presetStatusArea.withSizeKeepingCentre (presetStatusArea.getWidth(), 20));
    footer.removeFromRight (8);
    footer.removeFromLeft (258);
    const auto gap = 7;
    const auto buttonWidth = juce::jmax (55, (footer.getWidth() - gap * 3) / 4);
    for (size_t i = 0; i < presetButtons.size(); ++i)
    {
        auto buttonArea = footer.removeFromLeft (buttonWidth);
        presetButtons[i].setBounds (buttonArea.withSizeKeepingCentre (buttonArea.getWidth(), 30));
        if (i + 1 < presetButtons.size())
            footer.removeFromLeft (gap);
    }

    content.removeFromTop (12);
    auto body = content;
    const auto signalWidth = juce::jlimit (168, 260,
                                           juce::roundToInt ((float) body.getWidth() * 0.23f));
    auto meterArea = body.removeFromLeft (signalWidth);
    meter->setBounds (meterArea);
    body.removeFromLeft (12);

    spectralHeading.setBounds (body.removeFromTop (36).reduced (18, 8));
    auto rowOne = body.removeFromTop ((body.getHeight() - 34) / 2);
    timingHeading.setBounds (body.removeFromTop (34).reduced (18, 7));
    auto rowTwo = body;
    rowOne.reduce (7, 0);
    rowTwo.reduce (7, 0);

    const auto rowOneWidth = rowOne.getWidth() / 5;
    for (size_t i = 0; i < 5; ++i)
    {
        const auto width = i + 1 == 5 ? rowOne.getWidth() : rowOneWidth;
        layoutControl (controlLabels[i], *controlSliders[i], rowOne.removeFromLeft (width));
    }

    const auto rowTwoWidth = rowTwo.getWidth() / 5;
    for (size_t i = 5; i < 9; ++i)
    {
        const auto width = rowTwoWidth;
        layoutControl (controlLabels[i], *controlSliders[i], rowTwo.removeFromLeft (width));
    }

    bypassButton.setBounds (rowTwo.reduced (8, 22));
    rebuildTextureMask();
}
