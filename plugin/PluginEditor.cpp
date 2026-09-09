// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "PluginEditor.h"

#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour backgroundColour (0xff100d16);
const juce::Colour panelColour (0xff1a1622);
const juce::Colour panelRaisedColour (0xff211c2b);
const juce::Colour trackColour (0xff342b3d);
const juce::Colour lineColour (0xff46384e);
const juce::Colour accentColour (0xff936db9);
const juce::Colour accentBrightColour (0xffb995d7);
const juce::Colour copperColour (0xffc77a58);
const juce::Colour copperBrightColour (0xffe1a078);
const juce::Colour textColour (0xfff1e9e4);
const juce::Colour mutedTextColour (0xffaa9daa);

juce::Font makeFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions { height, style });
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
                              .reduced (8.0f);
        const auto diameter = juce::jmin (area.getWidth(), area.getHeight());

        if (diameter <= 1.0f)
            return;

        const auto dial = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre());
        const auto centre = dial.getCentre();
        const auto radius = diameter * 0.5f;
        const auto stroke = juce::jlimit (2.5f, 6.0f, diameter * 0.085f);
        const auto alpha = slider.isEnabled() ? 1.0f : 0.4f;

        // A quiet outer halo makes the control legible against the raised card.
        g.setColour (lineColour.withMultipliedAlpha (0.55f * alpha));
        g.fillEllipse (dial.expanded (1.5f));
        g.setColour (panelRaisedColour.withMultipliedAlpha (alpha));
        g.fillEllipse (dial.reduced (2.0f));

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - stroke * 0.75f,
                             radius - stroke * 0.75f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId)
                         .withMultipliedAlpha (alpha));
        g.strokePath (track, { stroke, juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded });

        if (sliderPosProportional > 0.0001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius - stroke * 0.75f,
                                 radius - stroke * 0.75f, 0.0f,
                                 rotaryStartAngle,
                                 rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle),
                                 true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                             .withMultipliedAlpha (alpha));
            g.strokePath (value, { stroke, juce::PathStrokeType::curved,
                                   juce::PathStrokeType::rounded });
        }

        const auto pointerAngle = rotaryStartAngle
                                  + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto pointerLength = radius * 0.68f;
        const auto pointerWidth = juce::jmax (1.5f, stroke * 0.55f);
        juce::Path pointer;
        pointer.startNewSubPath (centre.x + std::cos (pointerAngle) * radius * 0.20f,
                                 centre.y + std::sin (pointerAngle) * radius * 0.20f);
        pointer.lineTo (centre.x + std::cos (pointerAngle) * pointerLength,
                        centre.y + std::sin (pointerAngle) * pointerLength);
        g.setColour (slider.findColour (juce::Slider::thumbColourId)
                         .withMultipliedAlpha (alpha));
        g.strokePath (pointer, { pointerWidth, juce::PathStrokeType::curved,
                                 juce::PathStrokeType::rounded });
        g.fillEllipse (juce::Rectangle<float> (pointerWidth * 1.6f, pointerWidth * 1.6f)
                           .withCentre (centre));
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        const auto alpha = slider.isEnabled() ? 1.0f : 0.4f;
        const auto horizontal = style == juce::Slider::LinearHorizontal
                                || style == juce::Slider::LinearBar
                                || style == juce::Slider::TwoValueHorizontal
                                || style == juce::Slider::ThreeValueHorizontal;
        const auto thickness = juce::jlimit (3.0f, 7.0f,
                                             horizontal ? (float) height * 0.18f
                                                        : (float) width * 0.18f);

        juce::Point<float> start;
        juce::Point<float> end;
        juce::Point<float> thumb;
        if (horizontal)
        {
            start = { (float) x + 2.0f, (float) y + (float) height * 0.5f };
            end = { (float) x + (float) width - 2.0f, start.y };
            thumb = { juce::jlimit (start.x, end.x, sliderPos), start.y };
        }
        else
        {
            start = { (float) x + (float) width * 0.5f, (float) y + (float) height - 2.0f };
            end = { start.x, (float) y + 2.0f };
            thumb = { start.x, juce::jlimit (end.y, start.y, sliderPos) };
        }

        juce::Path background;
        background.startNewSubPath (start);
        background.lineTo (end);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId)
                         .withMultipliedAlpha (alpha));
        g.strokePath (background, { thickness, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded });

        juce::Path value;
        value.startNewSubPath (start);
        value.lineTo (thumb);
        g.setColour (slider.findColour (juce::Slider::trackColourId)
                         .withMultipliedAlpha (alpha));
        g.strokePath (value, { thickness, juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded });

        const auto thumbRadius = juce::jmax (5.0f, thickness * 1.25f);
        g.setColour (slider.findColour (juce::Slider::thumbColourId)
                         .withMultipliedAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (thumbRadius * 2.0f, thumbRadius * 2.0f)
                           .withCentre (thumb));

        juce::ignoreUnused (minSliderPos, maxSliderPos);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& background,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto fill = background;
        if (down)
            fill = fill.brighter (0.13f);
        else if (highlighted)
            fill = fill.brighter (0.07f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (lineColour.withAlpha (highlighted ? 0.9f : 0.65f));
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (6.0f, 5.0f);
        const auto switchBounds = juce::Rectangle<float> (42.0f, 22.0f)
                                      .withY (bounds.getCentreY() - 11.0f)
                                      .withX (bounds.getX());
        const auto on = button.getToggleState();
        auto switchColour = on ? accentColour : trackColour;
        if (highlighted)
            switchColour = switchColour.brighter (0.08f);
        if (down)
            switchColour = switchColour.brighter (0.12f);

        g.setColour (switchColour);
        g.fillRoundedRectangle (switchBounds, 11.0f);
        g.setColour (lineColour.withAlpha (0.9f));
        g.drawRoundedRectangle (switchBounds, 11.0f, 1.0f);
        g.setColour (on ? copperBrightColour : mutedTextColour);
        const auto knob = juce::Rectangle<float> (16.0f, 16.0f)
                              .withCentre ({ on ? switchBounds.getRight() - 11.0f
                                                  : switchBounds.getX() + 11.0f,
                                             switchBounds.getCentreY() });
        g.fillEllipse (knob);

        g.setColour (button.findColour (juce::ToggleButton::textColourId)
                         .withAlpha (button.isEnabled() ? 1.0f : 0.45f));
        g.setFont (makeFont (12.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), juce::Rectangle<float> (switchBounds.getRight() + 9.0f,
                    bounds.getY(), juce::jmax (1.0f, bounds.getRight() - switchBounds.getRight() - 9.0f),
                    bounds.getHeight()), juce::Justification::centredLeft, false);
    }
};

//==============================================================================
class RitualAudioProcessorEditor::MeterComponent final : public juce::Component,
                                                         public juce::SettableTooltipClient
{
public:
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
        auto area = getLocalBounds().toFloat().reduced (15.0f, 14.0f);
        g.setColour (mutedTextColour);
        g.setFont (makeFont (10.0f, juce::Font::bold));
        g.drawText ("SIGNAL", area.removeFromTop (18.0f), juce::Justification::left, false);

        g.setColour (lineColour.withAlpha (0.55f));
        g.fillRect (area.removeFromTop (1.0f));
        area.removeFromTop (18.0f);

        drawMeter (g, area.removeFromTop (86.0f), "INPUT", displayedInput, accentColour);
        area.removeFromTop (18.0f);
        drawMeter (g, area.removeFromTop (86.0f), "OUTPUT", displayedOutput, copperColour);

        auto note = area.removeFromTop (48.0f).withTrimmedTop (22.0f);
        g.setColour (mutedTextColour.withAlpha (0.78f));
        g.setFont (makeFont (10.0f));
        g.drawFittedText ("PEAK / POST EFFECT", note.toNearestInt(), juce::Justification::left, 2);
    }

private:
    static void drawMeter (juce::Graphics& g, juce::Rectangle<float> area,
                           const juce::String& name, float decibels,
                           juce::Colour fillColour)
    {
        g.setColour (mutedTextColour);
        g.setFont (makeFont (10.0f, juce::Font::bold));
        g.drawText (name, area.removeFromTop (17.0f), juce::Justification::left, false);
        g.drawText (juce::String (decibels, 1) + " dB", area.removeFromTop (17.0f),
                    juce::Justification::right, false);

        auto track = area.removeFromTop (13.0f);
        g.setColour (trackColour);
        g.fillRoundedRectangle (track, 6.0f);
        const auto fillWidth = track.getWidth() * normaliseMeter (decibels);
        if (fillWidth > 0.0f)
        {
            juce::ColourGradient gradient (fillColour.brighter (0.15f), track.getX(), 0.0f,
                                           fillColour.darker (0.12f), track.getRight(), 0.0f, false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (track.withWidth (fillWidth), 6.0f);
        }

        const auto scaleY = track.getBottom() + 10.0f;
        g.setColour (mutedTextColour.withAlpha (0.6f));
        g.setFont (makeFont (8.0f));
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
      meter (std::make_unique<MeterComponent>()),
      presets {{
          { "SUBTLE",    { 0.65f, 0.40f, 0.35f, 0.90f, 0.15f,  2.0f, 1.0f, 1.0f, 0.0f, 0.0f } },
          { "DAMAGED",  { 0.22f, 0.80f, 0.78f, 1.00f, 0.65f,  8.0f, 3.0f, 1.0f, 0.0f, 0.0f } },
          { "NO PUNCH", { 0.22f, 0.80f, 0.78f, 0.00f, 0.65f,  8.0f, 3.0f, 1.0f, 0.0f, 0.0f } },
          { "EXTREME",   { 0.05f, 1.00f, 0.94f, 1.00f, 0.90f, 16.0f, 7.0f, 1.0f, 0.0f, 0.0f } }
      }}
{
    setOpaque (true);
    setLookAndFeel (lookAndFeel.get());

    titleLabel.setText ("Ritual Codec", juce::dontSendNotification);
    titleLabel.setFont (makeFont (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, textColour);
    titleLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("spectral decay / transient ceremony", juce::dontSendNotification);
    subtitleLabel.setFont (makeFont (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, mutedTextColour);
    subtitleLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (subtitleLabel);

    statusLabel.setText ("LIVE  /  256 SAMPLE PATH", juce::dontSendNotification);
    statusLabel.setFont (makeFont (10.0f, juce::Font::bold));
    statusLabel.setColour (juce::Label::textColourId, accentBrightColour);
    statusLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (statusLabel);

    spectralHeading.setText ("SPECTRAL DAMAGE", juce::dontSendNotification);
    timingHeading.setText ("TIME + OUTPUT", juce::dontSendNotification);
    for (auto* heading : { &spectralHeading, &timingHeading })
    {
        heading->setFont (makeFont (10.0f, juce::Font::bold));
        heading->setColour (juce::Label::textColourId, mutedTextColour);
        heading->setJustificationType (juce::Justification::left);
        addAndMakeVisible (*heading);
    }

    latencyLabel.setFont (makeFont (10.0f, juce::Font::bold));
    latencyLabel.setColour (juce::Label::textColourId, mutedTextColour);
    latencyLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (latencyLabel);

    presetStatusLabel.setText ("CUSTOM", juce::dontSendNotification);
    presetStatusLabel.setFont (makeFont (10.0f, juce::Font::bold));
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
        controlLabels[i].setFont (makeFont (10.0f, juce::Font::bold));
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
    setSize (820, 540);
    updateMetersAndStatus();
    startTimerHz (30);
}

RitualAudioProcessorEditor::~RitualAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 22);
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
    area = area.reduced (4, 0);
    label.setBounds (area.removeFromTop (20));
    slider.setBounds (area.reduced (2, 0));
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
    if (activePreset >= 0 && ! presetMatches (presets[(size_t) activePreset]))
    {
        activePreset = -1;
        presetStatusLabel.setText ("CUSTOM", juce::dontSendNotification);
        presetStatusLabel.setColour (juce::Label::textColourId, copperBrightColour);
    }
}

void RitualAudioProcessorEditor::updateMetersAndStatus()
{
    if (meter != nullptr)
    {
        meter->setLevels (ritualProcessor.inputPeak.load (std::memory_order_relaxed),
                          ritualProcessor.outputPeak.load (std::memory_order_relaxed));
    }

    const bool bypassed = ritualProcessor.parameters.getRawParameterValue ("bypass")->load() >= 0.5f;
    statusLabel.setText (bypassed ? "BYPASSED / LATENCY ALIGNED" : "LIVE / SPECTRAL DAMAGE",
                         juce::dontSendNotification);
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
                                             juce::Colour (0xff171020), 0.0f,
                                             (float) whole.getBottom(), false);
    g.setGradientFill (backgroundGradient);
    g.fillAll();

    auto bounds = whole.reduced (22);
    auto header = bounds.removeFromTop (72);
    g.setColour (accentColour.withAlpha (0.7f));
    g.fillRect (bounds.getX(), header.getBottom() + 4, bounds.getWidth(), 1);

    auto footer = bounds.removeFromBottom (52);
    juce::ignoreUnused (footer);

    auto meterBounds = bounds.removeFromLeft (138);
    auto controlsBounds = bounds.withTrimmedLeft (18);

    g.setColour (panelColour);
    g.fillRoundedRectangle (meterBounds.toFloat(), 12.0f);
    g.setColour (lineColour.withAlpha (0.75f));
    g.drawRoundedRectangle (meterBounds.toFloat().reduced (0.5f), 12.0f, 1.0f);

    g.setColour (panelColour.withAlpha (0.92f));
    g.fillRoundedRectangle (controlsBounds.toFloat(), 12.0f);
    g.setColour (lineColour.withAlpha (0.7f));
    g.drawRoundedRectangle (controlsBounds.toFloat().reduced (0.5f), 12.0f, 1.0f);

    g.setColour (copperColour.withAlpha (0.8f));
    g.fillRect (controlsBounds.getX(), controlsBounds.getY(), 3, controlsBounds.getHeight());

    // Small registration marks reinforce the instrument-panel feel without
    // competing with the values and meters.
    g.setColour (accentBrightColour.withAlpha (0.32f));
    g.fillEllipse (header.getRight() - 18.0f, header.getY() + 15.0f, 5.0f, 5.0f);
    g.fillEllipse (header.getRight() - 9.0f, header.getY() + 15.0f, 5.0f, 5.0f);
}

void RitualAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (22);
    auto header = bounds.removeFromTop (72);
    titleLabel.setBounds (header.removeFromTop (39));
    subtitleLabel.setBounds (header.removeFromTop (21));
    statusLabel.setBounds (bounds.getRight() - 210, bounds.getY() - 53, 210, 20);

    auto footer = bounds.removeFromBottom (52);
    latencyLabel.setBounds (footer.removeFromLeft (250));
    footer.removeFromLeft (10);

    presetStatusLabel.setBounds (footer.removeFromRight (84));
    footer.removeFromRight (10);

    const auto gap = 7;
    const auto buttonWidth = juce::jmax (55, (footer.getWidth() - gap * 3) / 4);
    for (size_t i = 0; i < presetButtons.size(); ++i)
    {
        auto buttonArea = footer.removeFromLeft (buttonWidth);
        presetButtons[i].setBounds (buttonArea.withTrimmedTop (7).withTrimmedBottom (7));
        if (i + 1 < presetButtons.size())
            footer.removeFromLeft (gap);
    }

    auto body = bounds;
    auto meterArea = body.removeFromLeft (138);
    meter->setBounds (meterArea);
    body.removeFromLeft (18);

    spectralHeading.setBounds (body.removeFromTop (20).reduced (8, 0));
    auto rowOne = body.removeFromTop (body.getHeight() / 2);
    timingHeading.setBounds (body.removeFromTop (20).reduced (8, 0));
    auto rowTwo = body;

    const auto rowOneWidth = rowOne.getWidth() / 5;
    for (size_t i = 0; i < 5; ++i)
    {
        const auto width = rowOneWidth;
        layoutControl (controlLabels[i], *controlSliders[i], rowOne.removeFromLeft (width));
    }

    const auto rowTwoWidth = rowTwo.getWidth() / 5;
    for (size_t i = 5; i < 9; ++i)
    {
        const auto width = rowTwoWidth;
        layoutControl (controlLabels[i], *controlSliders[i], rowTwo.removeFromLeft (width));
    }

    auto bypassArea = rowTwo;
    bypassArea.removeFromLeft (4);
    bypassButton.setBounds (bypassArea.reduced (0, 22));
}
