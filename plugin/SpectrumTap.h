// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <array>

// Single audio-thread producer, single editor-thread consumer. Full queues drop
// visualization frames; neither thread waits, and audio processing never allocates.
class SpectrumTap {
public:
    static constexpr int size = 1024;
    struct Frame {
        std::array<std::array<float, size>, 4> samples {};
        double sampleRate = 48000.0;
    };
    void prepare(double rate) noexcept { rateHz = rate; position = 0; }
    void push(float inLeft, float inRight, float outLeft, float outRight) noexcept {
        const auto i = static_cast<size_t>(position++);
        pending.samples[0][i] = juce::jlimit(-32.0f,32.0f,inLeft); pending.samples[1][i] = juce::jlimit(-32.0f,32.0f,inRight);
        pending.samples[2][i] = juce::jlimit(-32.0f,32.0f,outLeft); pending.samples[3][i] = juce::jlimit(-32.0f,32.0f,outRight);
        if (position != size) return;
        position = 0;
        int a, n, b, m;
        fifo.prepareToWrite(1, a, n, b, m);
        if (n > 0) {
            pending.sampleRate = rateHz;
            frames[static_cast<size_t>(a)] = pending;
            fifo.finishedWrite(1);
        }
    }
    bool readLatest(Frame& result) noexcept {
        int a, n, b, m;
        fifo.prepareToRead(fifo.getNumReady(), a, n, b, m);
        if (n + m == 0) return false;
        result = frames[static_cast<size_t>(m > 0 ? b + m - 1 : a + n - 1)];
        fifo.finishedRead(n + m);
        return true;
    }
private:
    juce::AbstractFifo fifo { 4 };
    std::array<Frame, 4> frames {};
    Frame pending;
    int position = 0;
    double rateHz = 48000.0;
};
