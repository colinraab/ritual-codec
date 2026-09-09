// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

class RitualAudioProcessor;

/** The editor for the Ritual Codec effect.

    The controls are attached directly to the ritualProcessor's APVTS parameters so
    automation, undo, and host state restoration remain the source of truth.
*/
class RitualAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit RitualAudioProcessorEditor (RitualAudioProcessor&);
    ~RitualAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class RitualLookAndFeel;
    class MeterComponent;

    struct Preset
    {
        const char* name;
        // budget, competition, memory, punch, crunch, clock, downsample,
        // mix, output (dB), bypass.
        std::array<float, 10> values;
    };

    void timerCallback() override;

    void configureSlider (juce::Slider&, const juce::String& parameterID,
                          double minimum, double maximum, double interval,
                          const juce::String& tooltip);
    void configureDisplayFormatting();
    void layoutControl (juce::Label&, juce::Slider&, juce::Rectangle<int>);
    void applyPreset (int presetIndex);
    void setParameterValue (const char* parameterID, float value);
    bool parameterMatches (const char* parameterID, float value) const;
    bool presetMatches (const Preset&) const;
    void refreshPresetIndicator();
    void updateMetersAndStatus();

    RitualAudioProcessor& ritualProcessor;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<RitualLookAndFeel> lookAndFeel;
    std::unique_ptr<MeterComponent> meter;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label spectralHeading;
    juce::Label timingHeading;
    juce::Label latencyLabel;
    juce::Label presetStatusLabel;

    juce::Slider budgetSlider;
    juce::Slider competitionSlider;
    juce::Slider memorySlider;
    juce::Slider punchSlider;
    juce::Slider crunchSlider;
    juce::Slider clockSlider;
    juce::Slider downsampleSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;
    juce::ToggleButton bypassButton;

    std::array<juce::Label, 9> controlLabels;
    std::array<juce::Slider*, 9> controlSliders {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 9>
        sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::array<juce::TextButton, 4> presetButtons;
    std::array<Preset, 4> presets;
    int activePreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualAudioProcessorEditor)
};

