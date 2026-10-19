// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "../src/classic_engine.hpp"
#include <vector>
#include <iostream>
#include <random>
#include <cstdlib>
#include <new>
#include <chrono>
static bool watch = false;
static size_t allocations = 0;
void* operator new (std::size_t n)
{
    if (watch)
        allocations++;
    if (void* p = std::malloc (n))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept
{
    std::free (p);
}
void operator delete (void* p, std::size_t) noexcept
{
    std::free (p);
}
void require (bool ok, const char* s)
{
    if (! ok)
        throw std::runtime_error (s);
}
std::vector<float> run (const std::vector<float>& in, classic::Params p, int ch, int block,
                        int sr = 48000)
{
    classic::Engine e (sr, ch, p);
    std::vector<float> out (in.size());
    for (int i = 0; i < int (in.size()) / ch; i += block)
        e.process (in.data() + i * ch, out.data() + i * ch,
                   std::min (block, int (in.size()) / ch - i));
    return out;
}
int main()
try
{
    std::mt19937 gen (42);
    std::uniform_real_distribution<float> d (-.4, .4);
    float maxNull = 0, minAttackRatio = 1;
    double worstFrame = 0;
    for (int ch : {1, 2})
        for (int sr : {44100, 48000, 96000})
        {
            std::vector<float> x ((6000 + classic::N) * ch);
            for (int i = 0; i < 6000 * ch; i++)
                x[i] = d (gen);
            classic::Params p;
            p.transparent = true;
            auto y = run (x, p, ch, 17, sr);
            for (int i = 0; i < 6000 * ch; i++)
                maxNull = std::max (maxNull, std::abs (x[i] - y[i + classic::N * ch]));
            require (maxNull < 2e-6, "Transparent reconstruction failed");
            p.transparent = false;
            p.budget = .1;
            p.memory = .9;
            p.clock = 12;
            p.downsample = 7;
            p.crunch = .8;
            auto a = run (x, p, ch, 1, sr), b = run (x, p, ch, 127, sr);
            require (a == b, "Host-block dependence");
            for (float v : a)
                require (std::isfinite (v) && std::abs (v) < 8, "Unbounded output");
        }
    for (int offset = 0; offset < classic::H; offset++)
    {
        std::vector<float> x (4096);
        x[1024 + offset] = .8;
        classic::Params p;
        p.budget = .08;
        p.punch = 1;
        p.memory = .95;
        p.crunch = .9;
        p.downsample = 8;
        auto y = run (x, p, 1, 31);
        minAttackRatio = std::min (minAttackRatio, std::abs (y[1024 + offset + classic::N]) / .8f);
        require (std::abs (y[1024 + offset + classic::N] - .8f) < 1e-5,
                 "Protected impulse not preserved");
    }
    for (float budget : {0.f, .25f, 1.f})
        for (float punch : {0.f, 1.f})
        {
            classic::Params p;
            p.budget = budget;
            p.punch = punch;
            p.memory = 1;
            p.clock = 64;
            p.downsample = 32;
            p.crunch = 1;
            std::vector<float> z (4096);
            auto y = run (z, p, 1, 64);
            for (float v : y)
                require (v == 0, "Silence generated output");
        }
    classic::Engine e (48000, 2);
    std::array<float, 128> in{}, out{};
    for (auto& v : in)
        v = d (gen);
    watch = true;
    for (int i = 0; i < 1000; i++)
    {
        auto t = std::chrono::steady_clock::now();
        e.process (in.data(), out.data(), 64);
        double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t)
                        .count();
        worstFrame = std::max (worstFrame, us);
    }
    watch = false;
    require (allocations == 0, "Heap allocation in process");
    bool rejected = false;
    try
    {
        classic::Params bad;
        bad.budget = NAN;
        classic::Engine invalid (48000, 2, bad);
    }
    catch (...)
    {
        rejected = true;
    }
    require (rejected, "NaN parameter accepted");
    std::cout << "{\"passed\":true,\"transparent_max_error\":" << maxNull
              << ",\"protected_impulse_min_peak_ratio\":" << minAttackRatio
              << ",\"tested_impulse_offsets\":64,\"processing_allocations\":" << allocations
              << ",\"observed_max_64_sample_call_us\":" << worstFrame << "}\n";
}
catch (const std::exception& e)
{
    std::cerr << e.what() << "\n";
    return 1;
}
