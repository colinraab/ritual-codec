// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "SpectrumTap.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumDisplay final : public juce::Component, private juce::Timer {
public:
    SpectrumDisplay(SpectrumTap& source, juce::Font font) : tap(source), numericFont(std::move(font)) {
        setComponentID("spectrum");
        setAccessible(true);
        setTitle("Input and output spectrum");
        setDescription("Dim input and bright output; stereo energy, logarithmic frequency, minus 84 to plus 6 dBFS.");
        startTimerHz(30);
    }
    void refresh() { timerCallback(); }
    void paint(juce::Graphics& g) override {
        const juce::Colour dim(0xff987ca8), bright(0xffdea0ef);
        auto bounds = getLocalBounds().toFloat();
        g.setFont(numericFont.withHeight(15.0f));
        g.setColour(bright);
        g.drawText("SPECTRAL CHANGE", bounds.removeFromTop(18), juce::Justification::left);
        auto legend = bounds.removeFromTop(16);
        g.setFont(numericFont.withHeight(12.0f));
        g.setColour(dim); g.drawText("INPUT", legend.removeFromLeft(55), juce::Justification::left);
        g.setColour(bright); g.drawText("OUTPUT", legend, juce::Justification::left);
        auto plot = bounds.withTrimmedBottom(18).reduced(1.0f);
        g.setColour(juce::Colour(0xff100b15));g.fillRect(plot);
        g.setColour(dim.withAlpha(0.55f));g.drawRect(plot,0.7f);
        for (int line=1;line<4;++line) {
            auto y=plot.getY()+plot.getHeight()*static_cast<float>(line)/4.0f;
            g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());
        }
        const auto upper=static_cast<float>(std::min(20000.0, sampleRate*0.5));
        for (int channel=0;channel<2;++channel) {
            juce::Path curve;
            const int points=juce::jmax(2,juce::roundToInt(plot.getWidth()));
            for (int x=0;x<points;++x) {
                const auto t=static_cast<float>(x)/static_cast<float>(points-1);
                const auto freq=40.0f*std::pow(upper/40.0f,t);
                const auto nextFreq=40.0f*std::pow(upper/40.0f,std::min(1.0f,t+1.0f/static_cast<float>(points-1)));
                const auto lo=juce::jlimit(1,SpectrumTap::size/2,static_cast<int>(freq*SpectrumTap::size/sampleRate));
                const auto hi=juce::jlimit(lo,SpectrumTap::size/2,static_cast<int>(nextFreq*SpectrumTap::size/sampleRate));
                float magnitude=0.0f;
                for(int bin=lo;bin<=hi;++bin) magnitude=std::max(magnitude,levels[static_cast<size_t>(channel)][static_cast<size_t>(bin)]);
                const auto db=juce::Decibels::gainToDecibels(magnitude,-84.0f);
                const auto y=plot.getBottom()-juce::jlimit(0.0f,1.0f,(db+84.0f)/90.0f)*plot.getHeight();
                if(x==0) curve.startNewSubPath(plot.getX(),y);else curve.lineTo(plot.getX()+t*plot.getWidth(),y);
            }
            g.setColour(channel==0?dim:bright);
            g.strokePath(curve,juce::PathStrokeType(channel==0?1.0f:1.5f));
        }
        g.setColour(dim.brighter(0.2f));g.setFont(numericFont.withHeight(11.0f));
        auto footer=bounds.removeFromBottom(16);
        g.drawText("40 Hz",footer.removeFromLeft(45),juce::Justification::left);
        g.drawText(juce::String(upper/1000.0f,0)+" kHz",footer,juce::Justification::right);
    }
private:
    void timerCallback() override {
        if (tap.readLatest(frame)) {
            sampleRate=std::isfinite(frame.sampleRate)&&frame.sampleRate>=8000.0?frame.sampleRate:48000.0;
            for(size_t stream=0;stream<4;++stream) {
                work.fill(0.0f);
                std::copy(frame.samples[stream].begin(),frame.samples[stream].end(),work.begin());
                window.multiplyWithWindowingTable(work.data(),SpectrumTap::size);
                fft.performFrequencyOnlyForwardTransform(work.data());
                for(size_t bin=0;bin<power[0].size();++bin) {
                    const auto magnitude=work[bin]*(4.0f/static_cast<float>(SpectrumTap::size));
                    if(stream%2==0)power[stream/2][bin]=magnitude*magnitude*0.5f;
                    else power[stream/2][bin]+=magnitude*magnitude*0.5f;
                }
            }
            for(size_t c=0;c<2;++c)for(size_t bin=0;bin<levels[c].size();++bin) {
                const auto target=std::sqrt(power[c][bin]);
                levels[c][bin]+=(target-levels[c][bin])*(target>levels[c][bin]?0.65f:0.2f);
            }
        } else {
            for(auto& channel:levels)for(auto& level:channel)level*=0.92f;
        }
        repaint();
    }
    SpectrumTap& tap;
    juce::Font numericFont;
    SpectrumTap::Frame frame;
    juce::dsp::FFT fft { 10 };
    juce::dsp::WindowingFunction<float> window { SpectrumTap::size, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float,SpectrumTap::size*2> work {};
    std::array<std::array<float,SpectrumTap::size/2+1>,2> power {},levels {};
    double sampleRate=48000.0;
};
