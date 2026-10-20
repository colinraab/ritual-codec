// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "Render.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>

class FilePlayer final : public juce::AudioAppComponent,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    FilePlayer() : editor (processor)
    {
        setLookAndFeel (lookAndFeel.get());
        juce::PropertiesFile::Options preferences;
        preferences.applicationName = "Ritual Codec";
        preferences.filenameSuffix = ".settings";
        preferences.osxLibrarySubFolder = "Application Support";
        settings.setStorageParameters (preferences);
        auto* saved = settings.getUserSettings();
        juce::MemoryBlock restored;
        if (restored.fromBase64Encoding (saved->getValue ("effectState")))
            processor.setStateInformation (restored.getData(), static_cast<int> (restored.getSize()));
        formats.registerBasicFormats();
        readThread.startThread();
        for (auto* button : { &open, &play, &stop, &exportButton, &audio }) addAndMakeVisible (*button);
        for (auto* component : std::array<juce::Component*, 5> { &filename, &status, &time, &position, &editor })
            addAndMakeVisible (*component);
        filename.setText ("Open or drop an audio file", juce::dontSendNotification);
        status.setText ("", juce::dontSendNotification);
        filename.setFont (RitualAudioProcessorEditor::displayFont (19.0f));
        status.setFont (RitualAudioProcessorEditor::monoFont (11.0f));
        time.setFont (RitualAudioProcessorEditor::monoFont (12.0f));
        time.setJustificationType (juce::Justification::centredRight);
        time.setText ("0:00 / 0:00", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, juce::Colour (0xffc493cd));
        time.setColour (juce::Label::textColourId, juce::Colour (0xffc493cd));
        position.setTitle ("Playback position");
        position.setTooltip ("Seek through the audio file");
        play.setTooltip ("Play or pause the audio file");
        stop.setTooltip ("Stop and return to the beginning");
        position.setRange (0, 1, 0.001);
        position.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        position.onValueChange = [this] { transport.setPosition (position.getValue()); };
        position.onDragStart = [this] { seeking = true; };
        position.onDragEnd = [this] { transport.setPosition (position.getValue()); seeking = false; };
        open.onClick = [this] { chooseSource(); };
        play.onClick = [this]
        {
            if (transport.isPlaying()) transport.stop();
            else { if (transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.01)
                       transport.setPosition (0); transport.start(); }
        };
        stop.onClick = [this] { transport.stop(); transport.setPosition (0); };
        exportButton.onClick = [this] { chooseExport(); };
        audio.onClick = [this]
        {
            if (audioWindow) { audioWindow->toFront (true); return; }
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned (new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, false, false, true, false));
            options.content->setLookAndFeel (lookAndFeel.get());
            styleDeviceLabels (*options.content);
            options.content->setSize (520, 360);
            options.dialogTitle = "Audio output";
            options.dialogBackgroundColour = juce::Colour (0xff0d0911);
            options.useNativeTitleBar = true;
            audioWindow = options.launchAsync();
        };
        setSize (1000, 790);
        auto deviceState = juce::parseXML (saved->getValue ("audioDevice"));
        setAudioChannels (0, 2, deviceState.get());
        if (deviceManager.getCurrentAudioDevice() == nullptr)
            status.setText ("No audio output. Choose Audio output to select a device.", juce::dontSendNotification);
        const juce::File lastFile (saved->getValue ("lastFile"));
        if (lastFile.existsAsFile()) load (lastFile);
        startTimerHz (15);
    }

    ~FilePlayer() override
    {
        stopTimer();
        cancel.store (true);
        if (worker.joinable()) worker.join();
        delete audioWindow.getComponent();
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto* saved = settings.getUserSettings();
        saved->setValue ("effectState", state.toBase64Encoding());
        saved->setValue ("lastFile", source.getFullPathName());
        if (auto deviceState = deviceManager.createStateXml())
            saved->setValue ("audioDevice", deviceState->toString());
        settings.saveIfNeeded();
        shutdownAudio();
        transport.setSource (nullptr);
        readerSource.reset();
        readThread.stopThread (5000);
        setLookAndFeel (nullptr);
    }

    void prepareToPlay (int blockSize, double rate) override
    {
        transport.prepareToPlay (blockSize, rate);
        processor.setRateAndBufferSizeDetails (rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
        processingBuffer.setSize (previewChannels, blockSize);
    }
    void releaseResources() override { transport.releaseResources(); processor.releaseResources(); }
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        // AudioAppComponent may supply a nonzero start offset. Never process the
        // surrounding device buffer or include channels outside the stereo bus.
        processingBuffer.setSize (previewChannels, info.numSamples, false, false, true);
        transport.getNextAudioBlock ({ &processingBuffer, 0, info.numSamples });
        midi.clear();
        processor.processBlock (processingBuffer, midi);
        for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
            info.buffer->copyFrom (channel, info.startSample, processingBuffer,
                                   juce::jmin (channel, previewChannels - 1), 0, info.numSamples);
    }
    bool isInterestedInFileDrag (const juce::StringArray&) override { return ! exporting; }
    void filesDropped (const juce::StringArray& files, int, int) override
    { if (! files.isEmpty() && ! exporting) load (juce::File (files[0])); }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0911), 0.0f, 0.0f,
                                                juce::Colour (0xff15101a), 0.0f,
                                                static_cast<float> (getHeight()), false));
        g.fillAll();
        const auto panel = getLocalBounds().removeFromTop (104).reduced (10).toFloat();
        g.setColour (juce::Colour (0xff1a121f));
        g.fillRect (panel);
        g.setColour (juce::Colour (0xff754183).withAlpha (0.78f));
        g.drawRect (panel, 1.0f);
        g.setColour (juce::Colour (0xffad62c4).withAlpha (0.38f));
        g.drawRect (panel.reduced (4.0f), 0.7f);
        g.setColour (juce::Colour (0xffd89ae7).withAlpha (0.68f));
        g.fillRect (panel.getX() + 16.0f, panel.getY() - 1.0f, 18.0f, 2.0f);
    }
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto panel = bounds.removeFromTop (104).reduced (22, 16);
        auto top = panel.removeFromTop (32);
        open.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (12);
        exportButton.setBounds (top.removeFromRight (112));
        top.removeFromRight (8);
        audio.setBounds (top.removeFromRight (112));
        filename.setBounds (top.reduced (8, 0));
        panel.removeFromTop (8);
        play.setBounds (panel.removeFromLeft (80));
        panel.removeFromLeft (8);
        stop.setBounds (panel.removeFromLeft (70));
        panel.removeFromLeft (12);
        time.setBounds (panel.removeFromRight (120));
        panel.removeFromRight (12);
        position.setBounds (panel);
        status.setBounds (bounds.removeFromBottom (28).reduced (22, 0));
        editor.setBounds (bounds);
    }
private:
    static void styleDeviceLabels (juce::Component& component)
    {
        if (auto* label = dynamic_cast<juce::Label*> (&component))
            if (dynamic_cast<juce::ComboBox*> (label->getParentComponent()) == nullptr)
                label->setFont (RitualAudioProcessorEditor::displayFont (17.0f));
        for (auto* child : component.getChildren())
            styleDeviceLabels (*child);
    }

    void load (const juce::File& file)
    {
        try
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (! reader) throw std::runtime_error ("Cannot read this file. Try WAV, AIFF, FLAC or MP3.");
            ritual_app::validateInput (*reader);
            auto next = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
            const juce::ScopedLock audioLock (deviceManager.getAudioCallbackLock());
            transport.stop();
            transport.setSource (nullptr);
            previewChannels = static_cast<int> (next->getAudioFormatReader()->numChannels);
            const auto layout = previewChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
            processor.releaseResources();
            processor.setBusesLayout ({ { layout }, { layout } });
            if (auto* device = deviceManager.getCurrentAudioDevice())
            {
                const auto rate = device->getCurrentSampleRate();
                const auto blockSize = device->getCurrentBufferSizeSamples();
                processor.setRateAndBufferSizeDetails (rate, blockSize);
                processor.prepareToPlay (rate, blockSize);
                processingBuffer.setSize (previewChannels, blockSize);
            }
            readerSource = std::move (next);
            transport.setSource (readerSource.get(), 32768, &readThread, readerSource->getAudioFormatReader()->sampleRate);
            source = file;
            filename.setText (file.getFileName(), juce::dontSendNotification);
            filename.setTooltip (file.getFullPathName());
            position.setRange (0, transport.getLengthInSeconds(), 0.001);
            status.setText (deviceManager.getCurrentAudioDevice() ? "Ready"
                : "No audio output. Choose Audio output to select a device.", juce::dontSendNotification);
        }
        catch (const std::exception& error) { status.setText (error.what(), juce::dontSendNotification); }
    }
    void chooseSource()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open audio", source, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<FilePlayer> (this)] (const juce::FileChooser& result)
            { if (safe && result.getResult().existsAsFile()) safe->load (result.getResult()); });
    }
    void chooseExport()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export processed WAV",
            source.getSiblingFile (source.getFileNameWithoutExtension() + "-ritual.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe = juce::Component::SafePointer<FilePlayer> (this)] (const juce::FileChooser& result)
            {
                if (! safe || result.getResult() == juce::File()) return;
                const auto selected = result.getResult();
                const auto destination = selected.withFileExtension ("wav");
                // The native chooser confirmed only the filename it returned.
                if (destination != selected && destination.exists())
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                        "Replace existing WAV?", "Replace " + destination.getFileName() + "? The previous file will move to Trash.",
                        "Replace", "Cancel", safe.getComponent(),
                        juce::ModalCallbackFunction::create ([safe, destination] (int confirmed)
                        { if (safe && confirmed != 0) safe->startExport (destination); }));
                else safe->startExport (destination);
            });
    }
    void startExport (const juce::File& destination)
    {
        if (destination == source)
        { status.setText ("Choose a different filename from the source.", juce::dontSendNotification); return; }
        if (worker.joinable()) worker.join();
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        const auto input = source;
        exporting = true;
        progress.store (0);
        cancel.store (false);
        // Retain an existing destination until the complete new WAV is ready.
        const auto safe = juce::Component::SafePointer<FilePlayer> (this);
        worker = std::thread ([this, safe, input, destination, state]
        {
            const auto staging = destination.getSiblingFile (destination.getFileNameWithoutExtension()
                + ".rendering-" + juce::Uuid().toString() + ".wav");
            juce::String message;
            try
            {
                ritual_app::renderWav (input, staging, state, cancel, progress);
                if (cancel.load()) throw std::runtime_error ("Export cancelled.");
                if (destination.exists() && ! destination.moveToTrash())
                    throw std::runtime_error ("Cannot move the previous export to Trash.");
                if (! staging.moveFileTo (destination))
                    throw std::runtime_error ("Cannot move the completed export to its destination.");
                message = "Exported " + destination.getFileName() + " (32-bit float WAV)";
            }
            catch (const std::exception& error)
            {
                message = error.what();
                if (staging.exists() && ! staging.moveToTrash())
                    message += " Partial file retained: " + staging.getFullPathName();
            }
            juce::MessageManager::callAsync ([safe, message]
            { if (safe) { safe->exporting = false; safe->status.setText (message, juce::dontSendNotification); } });
        });
    }
    void timerCallback() override
    {
        const bool loaded = readerSource != nullptr;
        const bool outputAvailable = deviceManager.getCurrentAudioDevice() != nullptr;
        if (! outputAvailable)
        {
            transport.stop();
            if (! exporting) status.setText ("No audio output. Choose Audio output to select a device.", juce::dontSendNotification);
        }
        else if (missingOutput && ! exporting)
            status.setText (loaded ? "Ready" : "Open or drop an audio file", juce::dontSendNotification);
        missingOutput = ! outputAvailable;
        play.setEnabled (loaded && outputAvailable);
        stop.setEnabled (loaded);
        position.setEnabled (loaded);
        open.setEnabled (! exporting);
        exportButton.setEnabled (loaded && ! exporting);
        play.setButtonText (transport.isPlaying() ? "Pause" : "Play");
        play.setToggleState (transport.isPlaying(), juce::dontSendNotification);
        const auto clock = [] (double seconds)
        {
            const auto total = static_cast<int> (juce::jmax (0.0, seconds));
            return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
        };
        time.setText (clock (seeking ? position.getValue() : transport.getCurrentPosition())
                          + " / " + clock (transport.getLengthInSeconds()), juce::dontSendNotification);
        if (! seeking) position.setValue (transport.getCurrentPosition(), juce::dontSendNotification);
        if (exporting) status.setText ("Exporting " + juce::String (juce::roundToInt (progress.load() * 100)) + "%", juce::dontSendNotification);
    }
    std::unique_ptr<juce::LookAndFeel> lookAndFeel = RitualAudioProcessorEditor::createLookAndFeel();
    juce::TooltipWindow tooltips { this, 600 };
    juce::ApplicationProperties settings;
    juce::Component::SafePointer<juce::DialogWindow> audioWindow;
    RitualAudioProcessor processor;
    RitualAudioProcessorEditor editor;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readThread { "Audio file reader" };
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioBuffer<float> processingBuffer;
    juce::MidiBuffer midi;
    juce::File source;
    juce::TextButton open { "Open audio" }, play { "Play" }, stop { "Stop" },
                     exportButton { "Export WAV" }, audio { "Audio output" };
    juce::Label filename, status, time;
    juce::Slider position;
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread worker;
    std::atomic<bool> cancel { false };
    std::atomic<float> progress { 0 };
    int previewChannels = 2;
    bool exporting = false, seeking = false, missingOutput = false;
};

class RitualApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Ritual Codec"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    void initialise (const juce::String&) override
    {
        window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow ("Ritual Codec", juce::Colour (0xff0d0911), allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new FilePlayer(), true);
            setResizable (true, false);
            setResizeLimits (820, 680, 1640, 1220);
            centreWithSize (1000, 790);
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION (RitualApplication)
