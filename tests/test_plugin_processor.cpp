// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "../plugin/PluginProcessor.h"
#include "../plugin/SpectrumDisplay.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <iostream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
// Copies, reset silence, and rejected state updates must preserve exact values.
// std::equal_to expresses that intent without relaxing these checks to a tolerance.
template <typename T>
bool exactlyEqual(T actual, T expected) {
    return std::equal_to<T>{}(actual, expected);
}
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(RitualAudioProcessor& p, const char* id, float value) {
    auto* v = p.parameters.getParameter(id); require(v != nullptr,"Missing parameter");
    v->setValueNotifyingHost(v->convertTo0to1(value));
}
void prepare(RitualAudioProcessor& p, int channels, double rate = 48000) {
    p.setPlayConfigDetails(channels,channels,rate,64);
    p.prepareToPlay(rate,64);
    require(p.getLatencySamples()==256,"Incorrect reported latency");
}
std::vector<float> render(RitualAudioProcessor& p,const std::vector<float>& input,int channels,
                          int block,bool hostBypass=false) {
    juce::AudioBuffer<float> buffer(channels,block);
    juce::MidiBuffer midi;
    std::vector<float> output(input.size());
    int frames=static_cast<int>(input.size())/channels;
    for(int start=0;start<frames;start+=block) {
        int count=std::min(block,frames-start);
        for(int ch=0;ch<channels;++ch)for(int i=0;i<count;++i)
            buffer.setSample(ch,i,input[static_cast<size_t>((start+i)*channels+ch)]);
        std::array<float*,2> pointers{buffer.getWritePointer(0),channels==2?buffer.getWritePointer(1):nullptr};
        juce::AudioBuffer<float> view(pointers.data(),channels,count);
        if(hostBypass)p.processBlockBypassed(view,midi);else p.processBlock(view,midi);
        for(int ch=0;ch<channels;++ch)for(int i=0;i<count;++i)
            output[static_cast<size_t>((start+i)*channels+ch)]=view.getSample(ch,i);
    }
    return output;
}
void compare(const std::vector<float>&a,const std::vector<float>&b,float tolerance,const char* what) {
    require(a.size()==b.size(),"Size mismatch");
    for(size_t i=0;i<a.size();++i) require(std::isfinite(a[i])&&std::abs(a[i]-b[i])<=tolerance,what);
}
void checkDelay(const std::vector<float>& input,const std::vector<float>& output,int channels) {
    for(size_t i=0;i<output.size();++i) {
        float wanted=i<256u*static_cast<unsigned>(channels)?0:input[i-256u*static_cast<unsigned>(channels)];
        require(std::abs(output[i]-wanted)<1e-6f,"Dry or bypass latency mismatch");
    }
}
void tests() {
    for(int channels:{1,2})for(double rate:{44100.,48000.,96000.}) {
        std::vector<float> input(static_cast<size_t>(4096*channels));
        for(int i=0;i<3500;++i)for(int ch=0;ch<channels;++ch)
            input[static_cast<size_t>(i*channels+ch)]=.2f*std::sin(static_cast<float>(i)*(.017f+static_cast<float>(ch)*.009f));
        for(int mode=0;mode<3;++mode) {
            RitualAudioProcessor p;
            if(mode==0)set(p,"mix",0);
            if(mode==1){set(p,"bypass",1);set(p,"output",12);}
            prepare(p,channels,rate);
            checkDelay(input,render(p,input,channels,127,mode==2),channels);
            require(p.getBypassParameter()==p.parameters.getParameter("bypass"),"Host bypass not registered");
        }
        RitualAudioProcessor a,b;
        prepare(a,channels,rate);prepare(b,channels,rate);
        auto x=render(a,input,channels,1),y=render(b,input,channels,1025);
        compare(x,y,0,"Plugin depends on host block size");
        // Compare wrapper to direct core after interleave conversion.
        classic::Engine core(static_cast<int>(rate),channels);
        std::vector<float> reference(input.size());core.process(input.data(),reference.data(),4096);
        compare(x,reference,2e-7f,"Default plugin differs from DSP core");
        // Runtime parameter change: smoothing must consume samples, not callbacks.
        set(a,"budget",.05f);set(a,"mix",.3f);set(a,"output",-8.f);
        set(b,"budget",.05f);set(b,"mix",.3f);set(b,"output",-8.f);
        compare(render(a,input,channels,1),render(b,input,channels,513),0,"Automation depends on host block size");
        a.releaseResources();prepare(a,channels,rate);
        auto reset=render(a,std::vector<float>(2048u*static_cast<unsigned>(channels)),channels,64);
        for(float v:reset)require(exactlyEqual(v,0.0f),"Reset leaked old signal");
    }
    RitualAudioProcessor state;
    set(state,"budget",.123f);set(state,"clock",17);set(state,"output",-7.3f);set(state,"bypass",1);
    set(state,"bitstream",1);
    juce::MemoryBlock bytes;state.getStateInformation(bytes);
    RitualAudioProcessor restored;restored.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
    for(auto* parameter:state.getParameters()) {
        auto* identified=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        require(identified!=nullptr,"Nonversioned parameter");
        auto* other=restored.parameters.getParameter(identified->paramID);
        require(other&&std::abs(other->getValue()-parameter->getValue())<1e-6f,"State roundtrip mismatch");
    }
    require(restored.parameters.getRawParameterValue("bitstream")->load() >= 0.5f,
            "Bitstream state did not roundtrip");
    const auto before=restored.parameters.getRawParameterValue("budget")->load();
    restored.setStateInformation("garbage",7);
    require(exactlyEqual(restored.parameters.getRawParameterValue("budget")->load(),before),"Invalid state changed values");
    juce::XmlElement invalid("PARAMETERS");invalid.setAttribute("stateVersion",1);
    juce::MemoryBlock invalidBytes;juce::AudioProcessor::copyXmlToBinary(invalid,invalidBytes);
    restored.setStateInformation(invalidBytes.getData(),static_cast<int>(invalidBytes.getSize()));
    require(exactlyEqual(restored.parameters.getRawParameterValue("budget")->load(),before),"Incomplete state changed values");

    // A complete pre-bitstream state is migrated by appending an explicit
    // false stream flag.  This also covers hosts that omitted stateVersion
    // from the original ten-parameter tree.
    RitualAudioProcessor legacySource;
    set(legacySource, "budget", .31f);
    auto legacyXml = legacySource.parameters.copyState().createXml();
    require(legacyXml != nullptr, "Could not create legacy source state");
    require(legacyXml->getNumChildElements() == 11, "Unexpected current state shape");
    // Remove the actual bitstream child regardless of APVTS child ordering.
    for (int index = legacyXml->getNumChildElements() - 1; index >= 0; --index)
    {
        if (auto* child = legacyXml->getChildElement (index);
            child != nullptr && child->getStringAttribute ("id") == "bitstream")
            legacyXml->removeChildElement (child, true);
    }
    legacyXml->setAttribute ("stateVersion", 1);
    juce::MemoryBlock legacyBytes;
    juce::AudioProcessor::copyXmlToBinary (*legacyXml, legacyBytes);
    RitualAudioProcessor legacyRestored;
    legacyRestored.setStateInformation (legacyBytes.getData(), static_cast<int> (legacyBytes.getSize()));
    require(legacyRestored.parameters.getRawParameterValue ("bitstream")->load() < 0.5f,
            "Legacy state enabled experimental stream");
    require(std::abs(legacyRestored.parameters.getRawParameterValue ("budget")->load() - .31f) < 1e-6f,
            "Legacy state did not preserve parameters");

    // The stream path is opt-in and keeps the established 256-sample host
    // latency.  It must remain finite, independent of callback block size,
    // and expose the packet rate chosen from the budget.
    RitualAudioProcessor streamA, streamB;
    set(streamA, "bitstream", 1.0f);
    set(streamB, "bitstream", 1.0f);
    prepare(streamA, 2, 48000.0);
    prepare(streamB, 2, 48000.0);
    std::vector<float> streamInput (8192u * 2u);
    for (size_t frame = 0; frame < streamInput.size() / 2u; ++frame)
    {
        const auto x = frame == 0 ? .8f : .22f * std::sin (static_cast<float> (frame) * .021f);
        streamInput[frame * 2u] = x;
        streamInput[frame * 2u + 1u] = -x;
    }
    const auto streamOutputA = render(streamA, streamInput, 2, 127);
    const auto streamOutputB = render(streamB, streamInput, 2, 513);
    compare(streamOutputA, streamOutputB, 0.0f, "Bitstream depends on host block size");
    require(streamA.streamBitrateKbps.load (std::memory_order_relaxed) > 0.0f,
            "Experimental stream did not publish packet bitrate");
    for (const auto value : streamOutputA)
        require(std::isfinite (value), "Bitstream output is nonfinite");

    // Once the switch has warmed and faded, the plugin must reproduce the
    // independently decoded packets with exactly 256 samples of alignment.
    const auto packetRate = static_cast<unsigned> (std::ceil (
        streamA.streamBitrateKbps.load (std::memory_order_relaxed)));
    const auto packetReference = rit::decode (rit::encode ({ 48000, 2, streamInput }, packetRate, 2, { .65f, .65f, .9f, .25f }));
    for (size_t frame = 1024; frame < streamInput.size() / 2; ++frame)
        for (size_t channel = 0; channel < 2; ++channel)
            require (std::abs (streamOutputA[frame * 2 + channel]
                - packetReference.samples[(frame - 256) * 2 + channel]) < 1.0e-6f,
                "Plugin bitstream differs from independent decoder or latency");

    // Automation and repeated switches must not reset output into dropouts.
    for (int iteration = 0; iteration < 12; ++iteration)
    {
        set (streamA, "budget", static_cast<float> (iteration % 5) / 4.0f);
        set (streamA, "bitstream", iteration % 3 == 0 ? 0.0f : 1.0f);
        const auto changed = render (streamA, streamInput, 2, 31);
        double energy = 0.0;
        for (const auto value : changed)
        {
            require (std::isfinite (value), "Codec switch produced nonfinite output");
            energy += static_cast<double> (value) * value;
        }
        require (energy > 0.01, "Codec automation lost audio");
    }

    RitualAudioProcessor streamDry;
    set(streamDry, "bitstream", 1.0f);
    set(streamDry, "mix", 0.0f);
    prepare(streamDry, 2, 48000.0);
    checkDelay(streamInput, render(streamDry, streamInput, 2, 127), 2);

    RitualAudioProcessor streamBypass;
    set(streamBypass, "bitstream", 1.0f);
    prepare(streamBypass, 2, 48000.0);
    checkDelay(streamInput, render(streamBypass, streamInput, 2, 127, true), 2);
    RitualAudioProcessor badRate;prepare(badRate,2,384000);
    std::vector<float> signal(2048);signal[0]=.5f;
    checkDelay(signal,render(badRate,signal,2,17),2);
    RitualAudioProcessor finite;prepare(finite,2);
    signal[0]=std::numeric_limits<float>::quiet_NaN();signal[1]=std::numeric_limits<float>::infinity();
    for(float v:render(finite,signal,2,31))require(std::isfinite(v),"Nonfinite output");
    // Editor construction/resizing catches re-entrant callbacks during setup.
    {
        auto tap=std::make_unique<SpectrumTap>();
        SpectrumTap::Frame captured;
        tap->prepare(96000.0);
        for(int i=0;i<SpectrumTap::size*10;++i)tap->push(.25f,-.25f,.125f,-.125f);
        require(tap->readLatest(captured),"Spectrum queue did not publish");
        require(exactlyEqual(captured.sampleRate,96000.0),"Spectrum rate lost");
        for(int i=0;i<SpectrumTap::size;++i) {
            const auto n=static_cast<size_t>(i);
            require(exactlyEqual(captured.samples[0][n],.25f) && exactlyEqual(captured.samples[1][n],-.25f),"Stereo input capture changed");
            require(exactlyEqual(captured.samples[2][n],.125f) && exactlyEqual(captured.samples[3][n],-.125f),"Stereo output capture changed");
        }
        require(!tap->readLatest(captured),"Spectrum queue failed to drain");
        tap->push(1,1,1,1);tap->prepare(48000);
        for(int i=0;i<SpectrumTap::size;++i)tap->push(0,0,0,0);
        require(tap->readLatest(captured) && exactlyEqual(captured.samples[0][0],0.0f),"Spectrum partial frame survived prepare");
    }
    {
        auto capturedProcessor=std::make_unique<RitualAudioProcessor>();
        set(*capturedProcessor,"mix",0.0f);prepare(*capturedProcessor,2);
        std::vector<float> tapSignal(4096);
        uint32_t noise = 0x12345678u;
        for(size_t i=0;i<tapSignal.size()/2;++i) {
            noise = noise * 1664525u + 1013904223u;
            tapSignal[i*2] = (static_cast<float>(noise >> 8) / 16777216.0f - .5f) * .4f;
            tapSignal[i*2+1] = -tapSignal[i*2];
        }
        render(*capturedProcessor,tapSignal,2,127);
        SpectrumTap::Frame frame;
        require(capturedProcessor->spectrumTap.readLatest(frame),"Processor spectrum tap is disconnected");
        for(size_t i=0;i<SpectrumTap::size;++i) {
            // Two 1024-sample frames were captured. The latest frame starts at
            // sample 1024; the input tap must include the fixed 256-sample delay.
            // Broadband samples make even a one-sample timing error conspicuous.
            const float expected = tapSignal[(1024 + i - 256) * 2];
            constexpr float tolerance = 1.0e-6f;
            const auto close = [=] (float actual, float reference) {
                return std::isfinite(actual) && std::abs(actual - reference) <= tolerance;
            };
            if (!close(frame.samples[0][i], expected)
                || !close(frame.samples[1][i], -expected)
                || !close(frame.samples[2][i], expected)
                || !close(frame.samples[3][i], -expected)) {
                std::cerr << "Spectrum mismatch at " << i << ": expected=" << expected
                          << " input=" << frame.samples[0][i] << " output=" << frame.samples[2][i]
                          << " output error=" << std::abs(frame.samples[2][i] - expected)
                          << " mix=" << capturedProcessor->parameters.getRawParameterValue("mix")->load()
                          << " output dB=" << capturedProcessor->parameters.getRawParameterValue("output")->load()
                          << '\n';
                require(false,"Spectrum input/output are not latency aligned");
            }
            require(exactlyEqual(frame.samples[0][i],-frame.samples[1][i]),"Spectrum tap lost opposite-phase stereo");
        }
    }
    RitualAudioProcessor ui;prepare(ui,2);
    std::unique_ptr<juce::AudioProcessorEditor> editor(ui.createEditor());
    require(editor!=nullptr,"Editor factory returned null");
    for (const auto size : {juce::Point<int>(820,540),juce::Point<int>(1200,800)}) {
        editor->setSize(size.x,size.y);
        int sliders=0;
        std::vector<juce::Rectangle<int>> controlBounds;
        for(auto* child:editor->getChildren()) if(auto* slider=dynamic_cast<juce::Slider*>(child)) {
            ++sliders;
            require(slider->isVisible(),"Hidden slider");
            require(slider->getComponentID().isNotEmpty(),"Missing slider identity");
            for (const auto& previous : controlBounds)
                require(!previous.intersects(slider->getBounds()),"Overlapping slider hit targets");
            controlBounds.push_back(slider->getBounds());
            require(editor->getLocalBounds().contains(slider->getBounds()),"Slider outside editor");
            require(slider->getWidth()>=76 && slider->getHeight()>=60,"Slider clipped at supported size");
        }
        require(sliders==9,"Missing editor controls");
        int buttons=0;
        for (auto* child : editor->getChildren()) if (auto* button=dynamic_cast<juce::Button*>(child)) {
            ++buttons;
            require(button->isVisible(),"Hidden button");
            require(editor->getLocalBounds().contains(button->getBounds()),"Button outside editor");
            require(button->getWidth()>=24 && button->getHeight()>=24,"Button hit target too small");
        }
        require(buttons==6,"Missing codec, preset, or bypass button");
    }
    set(ui, "bitstream", 1.0f);
    editor.reset();
    editor.reset(ui.createEditor());
    for (auto* child : editor->getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
        {
            const auto id = slider->getComponentID();
            const bool expected = id != "clock" && id != "downsample";
            require(slider->isEnabled() == expected, "Incorrect RIT control availability");
        }
    set(ui, "bitstream", 0.0f);
    editor.reset();
    editor.reset(ui.createEditor());
    bool presetFound=false;
    for(auto* child:editor->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child)) {
        if(button->getButtonText()=="DAMAGED") { button->onClick();presetFound=true; }
    }
    require(presetFound,"Missing Damaged preset");
    require(std::abs(ui.parameters.getRawParameterValue("budget")->load()-.22f)<1e-6f,"Preset did not set budget");
    require(exactlyEqual(ui.parameters.getRawParameterValue("clock")->load(),8.0f),"Preset did not set clock");
    std::cout<<"PASS: editor construction, two sizes, nine visible controls, preset parameter updates\n";
    std::cout<<"PASS: mono/stereo, 44.1/48/96 kHz, 256-sample dry/parameter/host bypass, "
                "1/127/513/1025-sample blocks, core equivalence, smoothed automation, "
                "state roundtrip/rejection, reset, unsupported rate, finite output\n";
}
}
int main(int argc,char** argv) try {
    juce::ScopedJuceInitialiser_GUI gui;
    tests();
    if(argc>=3&&std::string(argv[1])=="--screenshot") {
        const bool screenshotStream = argc >= 7 && std::string (argv[6]) == "rit";
        RitualAudioProcessor p;
        if (screenshotStream)
            set (p, "bitstream", 1.0f);
        prepare(p,2);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        require(editor!=nullptr,"Missing editor");
        if (argc >= 5) editor->setSize(std::stoi(argv[3]), std::stoi(argv[4]));
        if (argc >= 6) {
            juce::AudioFormatManager formats;formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[5])));
            require(reader!=nullptr,"Cannot read preview audio");
            prepare(p,2,reader->sampleRate);
            for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
                if(button->getButtonText()=="DAMAGED")button->onClick();
            juce::AudioBuffer<float> audio(2,1024);juce::MidiBuffer midi;
            const auto length=std::min(reader->lengthInSamples,static_cast<juce::int64>(reader->sampleRate*3));
            for(juce::int64 offset=0;offset+1024<=length;offset+=1024) {
                reader->read(&audio,0,1024,offset,true,true);p.processBlock(audio,midi);
                for(auto* child:editor->getChildren())for(auto* sub:child->getChildren())
                    if(auto* spectrum=dynamic_cast<SpectrumDisplay*>(sub))spectrum->refresh();
            }
        }
        juce::Thread::sleep(40);
        juce::Timer::callPendingTimersSynchronously();
        const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[2]));
        auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot write screenshot");
        require(stream->setPosition(0),"Cannot rewind screenshot");
        require(stream->truncate().wasOk(),"Cannot truncate screenshot");
        juce::PNGImageFormat png;require(png.writeImageToStream(image,*stream),"PNG write failed");
        std::cout<<"Screenshot (" << (screenshotStream ? "RIT" : "classic")
                  << "): "<<file.getFullPathName()<<'\n';
    }
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
