// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once
#include <array>
#include <complex>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

namespace classic
{
constexpr int N = 256, H = 64, B = 16, K = N / 2 + 1, R = 1024;
constexpr float pi = 3.14159265358979323846f;
struct Params
{
    float budget = .4f, competition = .65f, memory = .65f, punch = .9f, crunch = .25f;
    int clock = 4, downsample = 1;
    bool transparent = false;
};
// Fixed storage; constructor prepares all coefficients. No allocation, locks or I/O in process().
class Engine
{
    using C = std::complex<float>;
    Params p;
    int sr, channels;
    uint64_t tick = 0, frames = 0;
    std::array<float, N> win{};
    std::array<C, N / 2> tw{};
    std::array<std::size_t, K> band{};
    std::array<int, B> counts{};
    std::array<std::array<float, R>, 2> input{}, output{};
    std::array<float, R> protect{};
    std::array<std::array<C, N>, 2> spec{};
    std::array<std::array<float, K>, 2> shape{};
    std::array<float, B> oldEnergy{}, attackBand{};
    std::array<float, 2> held{};
    float fast = 0, slow = 0, attack = 0, af, as, release;
    // Unsigned wrap is intentional when looking behind the start of the stream.
    static std::size_t ringIndex (uint64_t position) noexcept
    {
        return static_cast<std::size_t> (position & (R - 1));
    }
    void fft (std::array<C, N>& a, bool inverse)
    {
        for (std::size_t i = 1, j = 0; i < N; i++)
        {
            std::size_t b = N >> 1;
            for (; j & b; b >>= 1)
                j ^= b;
            j ^= b;
            if (i < j)
                std::swap (a[i], a[j]);
        }
        for (std::size_t len = 2; len <= N; len <<= 1)
            for (std::size_t i = 0; i < N; i += len)
                for (std::size_t j = 0; j < len / 2; j++)
                {
                    C w = tw[j * N / len];
                    if (inverse)
                        w = std::conj (w);
                    C u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                }
        if (inverse)
            for (auto& x : a)
                x /= float (N);
    }
    void frame()
    {
        std::array<std::array<float, B>, 2> energy{};
        std::array<float, B> linked{}, weight{}, bits{};
        for (std::size_t c = 0; c < static_cast<std::size_t> (channels); c++)
        {
            for (std::size_t i = 0; i < N; i++)
                spec[c][i] =
                    input[c][ringIndex (tick + 1 - N + static_cast<uint64_t> (i))] * win[i];
            fft (spec[c], false);
            for (std::size_t k = 1; k < K; k++)
                energy[c][band[k]] += std::norm (spec[c][k]);
            for (std::size_t b = 0; b < B; b++)
                linked[b] += energy[c][b];
        }
        float totalWeight = 0;
        for (std::size_t b = 0; b < B; b++)
        {
            float novelty =
                std::clamp ((linked[b] - oldEnergy[b] * 1.6f) / (linked[b] + 1e-8f), 0.f, 1.f);
            attackBand[b] = std::max (novelty, attackBand[b] * .72f);
            oldEnergy[b] = .55f * oldEnergy[b] + .45f * linked[b];
            // Weight per coefficient; competition=0 allocates uniformly.
            weight[b] = static_cast<float> (counts[b]) *
                        std::pow (linked[b] / static_cast<float> (std::max (1, counts[b])) + 1e-8f,
                                  p.competition * .5f);
            totalWeight += weight[b];
        }
        float totalBudget = (K - 1) * (1.f + 9.f * p.budget);
        for (std::size_t b = 0; b < B; b++)
            bits[b] = std::clamp (totalBudget * weight[b] / (totalWeight + 1e-12f) /
                                      static_cast<float> (std::max (1, counts[b])),
                                  0.f, 12.f);
        for (std::size_t c = 0; c < static_cast<std::size_t> (channels); c++)
        {
            if (! p.transparent)
                for (std::size_t b = 0; b < B; b++)
                {
                    float gain = std::sqrt (energy[c][b]);
                    float rescue = p.punch * attackBand[b];
                    int precision = std::clamp (int (std::floor (bits[b] + rescue * 4.f)), 0, 12);
                    float q = precision ? float ((1 << precision) - 1) : 0;
                    float norm = 0;
                    bool refresh = frames % static_cast<uint64_t> (p.clock) == 0 || rescue > .18f ||
                                   frames == 0;
                    for (std::size_t k = 1; k < K; k++)
                        if (band[k] == b)
                        {
                            float fresh = std::abs (spec[c][k]) / (gain + 1e-12f);
                            if (refresh)
                                shape[c][k] = (p.memory * (1 - rescue)) * shape[c][k] +
                                              (1 - p.memory * (1 - rescue)) * fresh;
                            norm += shape[c][k] * shape[c][k];
                        }
                    norm = std::sqrt (norm);
                    for (std::size_t k = 1; k < K; k++)
                        if (band[k] == b)
                        {
                            float mag = std::abs (spec[c][k]);
                            C phase = mag > 1e-12f ? spec[c][k] / mag : C (1, 0);
                            C value = phase * (shape[c][k] / (norm + 1e-12f));
                            // Complex coefficient quantization changes both amplitude and phase.
                            value = (q > 0.f) ? C (std::round (value.real() * q) / q,
                                                   std::round (value.imag() * q) / q)
                                              : C (0, 0);
                            spec[c][k] = value * gain;
                        }
                }
            spec[c][0] = C (spec[c][0].real(), 0);
            spec[c][N / 2] = C (spec[c][N / 2].real(), 0);
            for (std::size_t k = 1; k < N / 2; k++)
                spec[c][N - k] = std::conj (spec[c][k]);
            fft (spec[c], true);
            for (std::size_t i = 0; i < N; i++)
                output[c][ringIndex (tick + 1 + static_cast<uint64_t> (i))] +=
                    spec[c][i].real() * win[i] * .5f;
        }
        frames++;
    }

public:
    static constexpr int latencySamples() { return N; }
    Engine (int sampleRate, int ch, Params params = {}) : p (params), sr (sampleRate), channels (ch)
    {
        if (sr < 8000 || sr > 192000 || ch < 1 || ch > 2)
            throw std::invalid_argument ("Unsupported rate/channels");
        setParams (params);
        af = std::exp (-1.f / (static_cast<float> (sr) * .0002f));
        as = std::exp (-1.f / (static_cast<float> (sr) * .012f));
        release = std::exp (-1.f / (static_cast<float> (sr) * .004f));
        for (std::size_t i = 0; i < N; i++)
            win[i] = std::sin (pi * static_cast<float> (i) / N);
        for (std::size_t i = 0; i < N / 2; i++)
            tw[i] = std::polar (1.f, -2 * pi * static_cast<float> (i) / N);
        for (std::size_t k = 1; k < K; k++)
        {
            band[k] = static_cast<std::size_t> (
                std::min (B - 1, int (std::log (float (k)) / std::log (float (K)) * B)));
            counts[band[k]]++;
        }
    }
    // Call on the audio thread between blocks; no concurrent mutation supported.
    // Abrupt changes are allowed for glitching; automation smoothing is future work.
    void setParams (Params v)
    {
        for (float x : {v.budget, v.competition, v.memory, v.punch, v.crunch})
            if (! std::isfinite (x) || x < 0 || x > 1)
                throw std::invalid_argument ("Parameters must be finite and in [0,1]");
        if (v.clock < 1 || v.clock > 64 || v.downsample < 1 || v.downsample > 32)
            throw std::invalid_argument ("Invalid clock/downsample");
        p = v;
    }
    void process (const float* in, float* out, int samples)
    {
        for (int j = 0; j < samples; j++, tick++)
        {
            const std::size_t at = ringIndex (tick);
            float level = 0;
            for (std::size_t c = 0; c < static_cast<std::size_t> (channels); c++)
            {
                float x =
                    in[static_cast<std::size_t> (j) * static_cast<std::size_t> (channels) + c];
                if (! std::isfinite (x))
                    x = 0;
                input[c][at] = x;
                level = std::max (level, std::abs (x));
            }
            fast = af * fast + (1 - af) * level;
            slow = as * slow + (1 - as) * level;
            float novelty = std::clamp ((fast - 1.7f * slow) / (fast + 1e-8f) * 2.f, 0.f, 1.f);
            attack = std::max (novelty, attack * release);
            protect[at] = attack;
            for (std::size_t c = 0; c < static_cast<std::size_t> (channels); c++)
            {
                float wet = output[c][at];
                output[c][at] = 0;
                if (! p.transparent)
                {
                    if (tick % static_cast<uint64_t> (p.downsample) == 0)
                        held[c] = wet;
                    wet = held[c];
                    if (p.crunch > 0)
                    {
                        float q = std::pow (2.f, 15.f - 12.f * p.crunch);
                        wet = std::round (wet * q) / q;
                    }
                    float a = p.punch * protect[ringIndex (tick - N)];
                    wet = (1 - a) * wet + a * input[c][ringIndex (tick - N)];
                }
                out[static_cast<std::size_t> (j) * static_cast<std::size_t> (channels) + c] = wet;
            }
            if ((tick + 1) % H == 0)
                frame();
        }
    }
};
} // namespace classic
