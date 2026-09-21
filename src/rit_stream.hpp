// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// RIT1 is deliberately small and experimental. Version 1 is kept byte
// compatible with the original reference encoder. Version 2 adds a sparse
// time-domain packet representation for blocks whose energy is concentrated in
// a few samples (transients), while retaining spectral packets for ordinary
// material.
namespace rit {

constexpr unsigned block = 128;
constexpr unsigned maxPacketBytes = 4096;
using Bytes = std::vector<uint8_t>;

inline void require (bool ok, const char* message)
{
    if (! ok)
        throw std::runtime_error (message);
}

inline void put (Bytes& b, uint64_t v, unsigned n)
{
    for (unsigned i = 0; i < n; ++i)
        b.push_back (static_cast<uint8_t> (v >> (8 * i)));
}

inline uint64_t get (const Bytes& b, size_t& p, unsigned n)
{
    require (p <= b.size() && n <= b.size() - p, "Truncated RIT data");
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v |= uint64_t (b[p++]) << (8 * i);
    return v;
}

inline uint32_t crc (const uint8_t* p, size_t n)
{
    uint32_t v = 0xffffffffu;
    for (size_t i = 0; i < n; ++i)
    {
        v ^= p[i];
        for (unsigned j = 0; j < 8; ++j)
            v = (v >> 1) ^ ((v & 1) ? 0xedb88320u : 0u);
    }
    return ~v;
}

struct Audio
{
    unsigned rate = 48000;
    unsigned channels = 2;
    std::vector<float> samples;
};

// Optional encoder shaping controls.  They intentionally live outside the
// packet format: the decoder only sees the ordinary v1/v2 records, while the
// encoder uses these values to choose and quantize those records.
struct EncoderControls
{
    float competition = 1.0f; // 1: strongest coefficients, 0: balanced bands
    float memory = 0.0f;      // spectral persistence from the preceding block
    float punch = 1.0f;       // sparse transient selection/rescue
    float crunch = 0.0f;      // coarsen coefficient quantization
};

inline const auto& basis()
{
    static const auto table = [] {
        std::array<std::array<double, block>, block> t{};
        for (unsigned k = 0; k < block; ++k)
            for (unsigned n = 0; n < block; ++n)
                t[k][n] = std::sqrt ((k ? 2.0 : 1.0) / block)
                        * std::cos (3.14159265358979323846 * (n + .5) * k / block);
        return t;
    }();
    return table;
}

namespace detail {

inline float clampControl (float value) noexcept
{
    if (! std::isfinite (value))
        return 0.0f;
    return std::max (0.0f, std::min (1.0f, value));
}

inline EncoderControls clampControls (EncoderControls controls) noexcept
{
    controls.competition = clampControl (controls.competition);
    controls.memory = clampControl (controls.memory);
    controls.punch = clampControl (controls.punch);
    controls.crunch = clampControl (controls.crunch);
    return controls;
}

inline bool neutralControls (const EncoderControls& controls) noexcept
{
    return controls.competition >= 1.0f && controls.memory <= 0.0f
        && controls.punch >= 1.0f && controls.crunch <= 0.0f;
}

// State is deliberately fixed-size so it can be owned by Streaming and
// mutated from the callback without an allocation.  The offline encoder uses
// the same state and packet planner, which keeps packet choices identical.
struct EncoderState
{
    std::array<std::array<double, block>, 2> previousSpectrum{};
    std::array<double, 2> previousEnergy{};
    std::array<bool, 2> initialized{};

    void reset() noexcept
    {
        previousSpectrum = {};
        previousEnergy = {};
        initialized = {};
    }
};

inline unsigned packetBytesFor (unsigned rate, unsigned channels, unsigned kbps)
{
    require (rate >= 8000 && rate <= 192000 && (channels == 1 || channels == 2),
             "Unsupported RIT rate/channels");
    require (kbps >= 1 && kbps <= 4096, "Bitrate outside 1..4096 kbps");
    const auto packetBytes = static_cast<unsigned> (uint64_t (kbps) * 1000 * block
                                                    / (8 * rate));
    require (packetBytes >= 6 + channels * 7 && packetBytes <= maxPacketBytes,
             "Bitrate cannot fit packet overhead, or exceeds packet limit");
    return packetBytes;
}

inline unsigned packetBytesForNoexcept (unsigned rate, unsigned channels, unsigned kbps) noexcept
{
    if (rate < 8000 || rate > 192000 || (channels != 1 && channels != 2)
        || kbps < 1 || kbps > 4096)
        return 0;
    const auto packetBytes = static_cast<unsigned> (uint64_t (kbps) * 1000 * block
                                                    / (8 * rate));
    return packetBytes >= 6 + channels * 7 && packetBytes <= maxPacketBytes
             ? packetBytes : 0;
}

inline unsigned qFor (double value, double scale) noexcept
{
    if (scale <= 0.0)
        return 127u;
    auto q = static_cast<int> (std::lround (value / scale * 127.0));
    q = std::max (-127, std::min (127, q));
    return static_cast<unsigned> (q + 127);
}

inline int qExactFor (double value, double scale) noexcept
{
    return scale > 0.0 ? static_cast<int> (std::lround (value / scale * 127.0)) : 0;
}

inline unsigned rmsQuantized (double energy) noexcept
{
    return static_cast<unsigned> (std::lround (std::sqrt (energy / block) * 65535.0 / 4.0));
}

inline unsigned scaleQuantized (double peak) noexcept
{
    return static_cast<unsigned> (std::ceil (std::abs (peak) * 65535.0 / 64.0));
}

inline void sortedByAbs (const std::array<double, block>& values,
                         std::array<unsigned, block>& order) noexcept
{
    for (unsigned k = 0; k < block; ++k)
        order[k] = k;
    std::sort (order.begin(), order.end(), [&] (unsigned x, unsigned y) {
        const auto ax = std::abs (values[x]);
        const auto ay = std::abs (values[y]);
        return std::equal_to<>{} (ax, ay) ? x < y : ax > ay;
    });
}

inline void sortedByAbs (const std::array<float, block>& values,
                         std::array<unsigned, block>& order) noexcept
{
    for (unsigned k = 0; k < block; ++k)
        order[k] = k;
    std::sort (order.begin(), order.end(), [&] (unsigned x, unsigned y) {
        const auto ax = std::abs (values[x]);
        const auto ay = std::abs (values[y]);
        return std::equal_to<>{} (ax, ay) ? x < y : ax > ay;
    });
}

// A sparse packet is reserved for a transient-like block. Limiting it to 32
// samples prevents a dense sustained waveform from turning into a packet full
// of time-domain holes merely because its bitrate is high.
inline unsigned sparseCount (const std::array<float, block>& values,
                             unsigned maximum, float punch = 1.0f) noexcept
{
    double total = 0.0;
    for (const auto x : values)
        total += double (x) * x;
    if (total <= 0.0 || maximum == 0 || punch <= 0.0f)
        return 0;

    std::array<unsigned, block> order{};
    sortedByAbs (values, order);
    const auto limit = std::min ({ maximum, block - 1, 32u });
    double captured = 0.0;
    for (unsigned count = 1; count <= limit; ++count)
    {
        const auto x = values[order[count - 1]];
        captured += double (x) * x;
        if (captured >= total * (1.0 - 0.05 * static_cast<double>(punch)))
            return count;
    }
    return 0;
}

// Preserve normalized magnitudes while following the current block's signs.
// All state is encoder-only: the resulting coefficients are stored in packets.
inline void shapeSpectrum(std::array<double, block>& coeff, double energy,
                          unsigned channel, EncoderState& state,
                          const EncoderControls& controls) noexcept
{
    const double norm = std::sqrt(energy);
    const double old = state.previousEnergy[channel];
    const double attack = energy > 0 ? std::max(0.0, (energy-old*1.6)/(energy+1e-20)) : 0;
    const double retain = state.initialized[channel] ? .98*controls.memory*(1.0-controls.punch*attack) : 0;
    for(unsigned k=0;k<block;++k) {
        const double fresh = norm > 0 ? std::abs(coeff[k])/norm : 0;
        const double magnitude = (1-retain)*fresh + retain*state.previousSpectrum[channel][k];
        if(controls.memory > 0 && norm > 0)
            coeff[k] = std::copysign(magnitude*norm, coeff[k]);
        state.previousSpectrum[channel][k] = magnitude;
    }
    state.previousEnergy[channel] = energy;
    state.initialized[channel] = energy > 0;
}
inline void rankSpectrum(const std::array<double, block>& coeff,
                         std::array<unsigned, block>& order, float competition) noexcept
{
    if(competition >= 1) { sortedByAbs(coeff,order); return; }
    std::array<double, 8> energy{};
    for(unsigned k=0;k<block;++k) energy[k/16] += coeff[k]*coeff[k];
    std::array<double,block> score{};
    for(unsigned k=0;k<block;++k) {
        order[k]=k;
        score[k]=std::abs(coeff[k])/std::pow(std::sqrt(energy[k/16])+1e-20,1.0-competition);
    }
    std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){
        return std::equal_to<>{}(score[a],score[b]) ? a<b : score[a]>score[b];
    });
}
inline int crunchQuantized(int q, float crunch) noexcept
{
    if(crunch <= 0) return q;
    const double step=std::pow(2.0, static_cast<double>(crunch)*6.0);
    return std::clamp(static_cast<int>(std::lround(std::round(q/step)*step)),-127,127);
}

inline unsigned rawCount (unsigned count, bool sparse, unsigned version) noexcept
{
    if (version == 2 && ! sparse && count == block)
        return 0; // v2: low seven bits zero means 128 spectral terms.
    return sparse ? count | 0x80u : count;
}

} // namespace detail

// Kept as a separate helper because this byte sequence is the v1 compatibility
// reference used by the release reports and old .rit fixtures.
inline Bytes encodeVersion1 (const Audio& a, unsigned kbps)
{
    require (a.rate >= 8000 && a.rate <= 192000 && (a.channels == 1 || a.channels == 2),
             "Unsupported RIT rate/channels");
    require (! a.samples.empty() && a.samples.size() % a.channels == 0,
             "Invalid audio length");
    require (a.samples.size() <= 64 * 1024 * 1024, "Audio exceeds reference codec limit");
    require (kbps >= 1 && kbps <= 4096, "Bitrate outside 1..4096 kbps");
    const unsigned packetBytes = static_cast<unsigned> (uint64_t (kbps) * 1000 * block
                                                        / (8 * a.rate));
    require (packetBytes >= 6 + a.channels * 7 && packetBytes <= maxPacketBytes,
             "Bitrate cannot fit packet overhead, or exceeds packet limit");
    for (const float x : a.samples)
        require (std::isfinite (x) && std::abs (x) <= 4.f,
                 "Input must be finite and within +/-4");

    const size_t frames = a.samples.size() / a.channels;
    Bytes out { 'R', 'I', 'T', '1' };
    put (out, 1, 2); put (out, a.channels, 1); put (out, 7, 1);
    put (out, a.rate, 4); put (out, frames, 8); put (out, packetBytes, 2); put (out, 0, 2);

    const auto& t = basis();
    for (size_t start = 0; start < frames; start += block)
    {
        Bytes payload;
        const unsigned channelBytes = (packetBytes - 6) / a.channels;
        const unsigned count = std::min (block, (channelBytes - 5) * 8 / 15);
        for (unsigned c = 0; c < a.channels; ++c)
        {
            std::array<double, block> coeff{};
            double energy = 0;
            for (unsigned n = 0; n < block; ++n)
            {
                const double x = start + n < frames
                                   ? a.samples[(start + n) * a.channels + c] : 0;
                energy += x * x;
                for (unsigned k = 0; k < block; ++k)
                    coeff[k] += x * t[k][n];
            }
            std::array<unsigned, block> order{};
            for (unsigned k = 0; k < block; ++k) order[k] = k;
            std::sort (order.begin(), order.end(), [&] (unsigned x, unsigned y) {
                return std::equal_to<>{} (std::abs (coeff[x]), std::abs (coeff[y]))
                         ? x < y : std::abs (coeff[x]) > std::abs (coeff[y]);
            });
            const auto rms = static_cast<unsigned> (std::lround (
                std::sqrt (energy / block) * 65535 / 4));
            const auto scale = static_cast<unsigned> (std::ceil (
                std::abs (coeff[order[0]]) * 65535 / 64));
            put (payload, count, 1); put (payload, rms, 2); put (payload, scale, 2);
            uint32_t bits = 0; unsigned used = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                const unsigned k = order[j];
                const double s = scale * 64.0 / 65535;
                const int q = s > 0 ? static_cast<int> (std::lround (coeff[k] / s * 127)) : 0;
                const uint32_t record = k | (static_cast<uint32_t> (q + 127) << 7);
                bits |= record << used; used += 15;
                while (used >= 8)
                {
                    payload.push_back (static_cast<uint8_t> (bits));
                    bits >>= 8; used -= 8;
                }
            }
            if (used) payload.push_back (static_cast<uint8_t> (bits));
        }
        payload.resize (packetBytes - 6, 0);
        put (out, packetBytes, 2); put (out, crc (payload.data(), payload.size()), 4);
        out.insert (out.end(), payload.begin(), payload.end());
    }
    return out;
}

inline Bytes encodeVersion2 (const Audio& a, unsigned kbps)
{
    require (a.rate >= 8000 && a.rate <= 192000 && (a.channels == 1 || a.channels == 2),
             "Unsupported RIT rate/channels");
    require (! a.samples.empty() && a.samples.size() % a.channels == 0,
             "Invalid audio length");
    require (a.samples.size() <= 64 * 1024 * 1024, "Audio exceeds reference codec limit");
    const unsigned packetBytes = detail::packetBytesFor (a.rate, a.channels, kbps);
    for (const float x : a.samples)
        require (std::isfinite (x) && std::abs (x) <= 4.f,
                 "Input must be finite and within +/-4");

    const size_t frames = a.samples.size() / a.channels;
    Bytes out { 'R', 'I', 'T', '1' };
    put (out, 2, 2); put (out, a.channels, 1); put (out, 7, 1);
    put (out, a.rate, 4); put (out, frames, 8); put (out, packetBytes, 2); put (out, 0, 2);

    const auto& t = basis();
    for (size_t start = 0; start < frames; start += block)
    {
        Bytes payload;
        const unsigned channelBytes = (packetBytes - 6) / a.channels;
        const unsigned capacity = std::min (block, (channelBytes - 5) * 8 / 15);
        for (unsigned c = 0; c < a.channels; ++c)
        {
            std::array<float, block> values{};
            for (unsigned n = 0; n < block; ++n)
                values[n] = start + n < frames ? a.samples[(start + n) * a.channels + c] : 0.0f;

            const unsigned sparseCount = detail::sparseCount (values, capacity);
            const bool useSparse = sparseCount != 0;
            const unsigned count = useSparse ? sparseCount : capacity;
            std::array<double, block> coeff{};
            double energy = 0.0;
            for (unsigned n = 0; n < block; ++n)
            {
                const double x = values[n]; energy += x * x;
                for (unsigned k = 0; k < block; ++k) coeff[k] += x * t[k][n];
            }
            std::array<unsigned, block> order{};
            detail::sortedByAbs (coeff, order);
            std::array<unsigned, block> timeOrder{};
            if (useSparse) detail::sortedByAbs (values, timeOrder);
            const auto rms = detail::rmsQuantized (energy);
            const double peak = useSparse ? std::abs (values[timeOrder[0]])
                                          : std::abs (coeff[order[0]]);
            const auto scale = detail::scaleQuantized (peak);
            put (payload, detail::rawCount (count, useSparse, 2), 1);
            put (payload, rms, 2); put (payload, scale, 2);
            uint32_t bits = 0; unsigned used = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                const unsigned index = useSparse ? timeOrder[j] : order[j];
                const double source = useSparse ? values[index] : coeff[index];
                const double s = scale * 64.0 / 65535;
                const uint32_t record = index | (uint32_t (detail::qFor (source, s)) << 7);
                bits |= record << used; used += 15;
                while (used >= 8)
                {
                    payload.push_back (static_cast<uint8_t> (bits));
                    bits >>= 8; used -= 8;
                }
            }
            if (used) payload.push_back (static_cast<uint8_t> (bits));
        }
        payload.resize (packetBytes - 6, 0);
        put (out, packetBytes, 2); put (out, crc (payload.data(), payload.size()), 4);
        out.insert (out.end(), payload.begin(), payload.end());
    }
    return out;
}

inline Bytes encode (const Audio& a, unsigned kbps, unsigned version = 1)
{
    require (version == 1 || version == 2, "Unsupported RIT version");
    return version == 1 ? encodeVersion1 (a, kbps) : encodeVersion2 (a, kbps);
}

inline Audio decode (const Bytes& data)
{
    size_t p = 0;
    require (get (data, p, 4) == 0x31544952u, "Not a RIT1 stream");
    const auto version = static_cast<unsigned> (get (data, p, 2));
    require (version == 1 || version == 2, "Unsupported RIT version");
    Audio a;
    a.channels = static_cast<unsigned> (get (data, p, 1));
    require (get (data, p, 1) == 7, "Unsupported frame size");
    a.rate = static_cast<unsigned> (get (data, p, 4));
    const uint64_t frames = get (data, p, 8);
    const auto packetBytes = static_cast<unsigned> (get (data, p, 2));
    require (get (data, p, 2) == 0, "Unsupported flags");
    require (a.rate >= 8000 && a.rate <= 192000 && (a.channels == 1 || a.channels == 2),
             "Invalid rate/channels");
    require (frames > 0 && frames <= 64 * 1024 * 1024 / a.channels,
             "Decoded audio exceeds reference decoder limit");
    require (packetBytes >= 6 + a.channels * 7 && packetBytes <= maxPacketBytes,
             "Invalid packet size");
    const uint64_t packets = (frames + block - 1) / block;
    require (packets <= (data.size() - p) / packetBytes
                 && packets * packetBytes == data.size() - p,
             "Incorrect file length or trailing data");

    a.samples.reserve (static_cast<size_t> (frames) * a.channels);
    const auto& t = basis();
    for (uint64_t start = 0; start < frames; start += block)
    {
        require (get (data, p, 2) == packetBytes, "Packet length mismatch");
        const auto check = get (data, p, 4);
        const size_t end = p + packetBytes - 6;
        require (crc (data.data() + p, packetBytes - 6) == check,
                 "Packet checksum mismatch");
        std::array<std::array<double, block>, 2> decoded{};
        for (unsigned c = 0; c < a.channels; ++c)
        {
            require (end - p >= 5, "Truncated channel header");
            const auto rawCount = static_cast<unsigned> (get (data, p, 1));
            const bool sparse = version == 2 && (rawCount & 0x80u) != 0;
            const unsigned count = version == 2
                                     ? (sparse ? rawCount & 0x7fu
                                               : (rawCount == 0 ? block : rawCount))
                                     : rawCount;
            require (count > 0 && count <= block, "Invalid coefficient count");
            if (sparse) require (count <= block - 1, "Invalid sparse sample count");
            const double rms = static_cast<double> (get (data, p, 2)) * 4 / 65535;
            const double scale = static_cast<double> (get (data, p, 2)) * 64 / 65535;
            const unsigned bytes = (count * 15 + 7) / 8;
            require (bytes <= end - p, "Truncated coefficient data");
            const size_t recordEnd = p + bytes;
            std::array<bool, block> seen{};
            uint32_t bits = 0; unsigned used = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                while (used < 15)
                {
                    bits |= uint32_t (data[p++]) << used;
                    used += 8;
                }
                const auto record = bits & 32767u; bits >>= 15; used -= 15;
                const unsigned index = record & 127u;
                const int q = static_cast<int> (record >> 7) - 127;
                require (! seen[index] && q >= -127 && q <= 127,
                         "Invalid/duplicate coefficient");
                seen[index] = true;
                if (sparse)
                    decoded[c][index] += q * scale / 127;
                else
                    for (unsigned n = 0; n < block; ++n)
                        decoded[c][n] += q * scale / 127 * t[index][n];
            }
            require (p == recordEnd && bits == 0, "Nonzero bit padding");
            double energy = 0;
            for (const double x : decoded[c]) energy += x * x;
            require (energy > 0 || rms <= 0, "Nonzero energy with empty spectrum");
            const double gain = energy > 0 ? rms / std::sqrt (energy / block) : 0;
            for (double& x : decoded[c]) x *= gain;
        }
        while (p < end) require (data[p++] == 0, "Nonzero packet padding");
        for (unsigned n = 0; n < block && start + n < frames; ++n)
            for (unsigned c = 0; c < a.channels; ++c)
                a.samples.push_back (static_cast<float> (decoded[c][n]));
    }
    return a;
}

/** Fixed-storage encoder/decoder for the audio callback. */
class Streaming
{
public:
    explicit Streaming (unsigned rate, unsigned channels, unsigned kbps = 128,
                        unsigned version = 2) noexcept
        : rate_ (rate), channels_ (channels), version_ (version)
    {
        static_cast<void> (basis());
        valid_ = version_ == 1 || version_ == 2;
        packetBytes_ = detail::packetBytesForNoexcept (rate_, channels_, kbps);
        valid_ = valid_ && packetBytes_ != 0;
        kbps_ = packetBytes_ != 0 ? kbps : 0;
        reset();
    }

    bool setBitrate (unsigned kbps) noexcept
    {
        const auto bytes = detail::packetBytesForNoexcept (rate_, channels_, kbps);
        if (bytes == 0 || (version_ != 1 && version_ != 2)) return false;
        // The host may call this while a packet is being assembled. Keep the
        // delay line and partial packet intact; the new budget takes effect at
        // the next packet boundary.
        packetBytes_ = bytes; kbps_ = kbps; valid_ = true; return true;
    }

    void setControls(EncoderControls controls) noexcept { controls_ = detail::clampControls(controls); }
    const std::array<uint8_t,maxPacketBytes>& lastPacket() const noexcept { return packet_; }

    void reset() noexcept
    {
        encoderState_.reset();
        inputBlock_.fill (0.0f); delay_.fill (0.0f);
        blockFill_ = 0; samplePosition_ = 0;
    }

    void process (const float* input, float* output, int frames) noexcept
    {
        if (output == nullptr || frames <= 0 || (channels_ != 1 && channels_ != 2)) return;
        if (! valid_ || input == nullptr)
        {
            for (size_t i = 0; i < static_cast<size_t> (frames) * channels_; ++i) output[i] = 0.0f;
            return;
        }
        for (int frame = 0; frame < frames; ++frame)
        {
            const auto slot = static_cast<unsigned> (samplePosition_ & (block - 1));
            std::array<float, 2> current{};
            for (unsigned c = 0; c < channels_; ++c)
            {
                auto value = input[static_cast<size_t> (frame) * channels_ + c];
                if (! std::isfinite (value)) value = 0.0f;
                value = std::max (-4.0f, std::min (4.0f, value));
                current[c] = value;
            }
            for (unsigned c = 0; c < channels_; ++c)
            {
                output[static_cast<size_t> (frame) * channels_ + c]
                    = delay_[slot * 2 + c];
                inputBlock_[blockFill_ * 2 + c] = current[c];
            }
            ++blockFill_;
            if (blockFill_ == block) { renderBlock(); blockFill_ = 0; }
            ++samplePosition_;
        }
    }

    static constexpr unsigned latencySamples() noexcept { return block; }
    constexpr unsigned rate() const noexcept { return rate_; }
    constexpr unsigned channels() const noexcept { return channels_; }
    constexpr unsigned version() const noexcept { return version_; }
    constexpr unsigned bitrate() const noexcept { return kbps_; }
    constexpr unsigned packetBytes() const noexcept { return packetBytes_; }
    constexpr bool isValid() const noexcept { return valid_; }

private:
    void renderBlock() noexcept
    {
        const auto& t = basis();
        const unsigned channelBytes = (packetBytes_ - 6) / channels_;
        const unsigned capacity = std::min (block, (channelBytes - 5) * 8 / 15);

        // Build the actual bounded packet first. This keeps the callback path
        // faithful to the file codec while using only fixed storage.
        size_t cursor = 6;
        for (unsigned c = 0; c < channels_; ++c)
        {
            std::array<float, block> values{};
            std::array<double, block> coeff{};
            double energy = 0.0;
            for (unsigned n = 0; n < block; ++n)
            {
                const auto value = inputBlock_[n * 2 + c]; values[n] = value;
                energy += double (value) * value;
                for (unsigned k = 0; k < block; ++k) coeff[k] += value * t[k][n];
            }
            detail::shapeSpectrum(coeff, energy, c, encoderState_, controls_);
            const unsigned sparseN = version_ == 2 ? detail::sparseCount (values, capacity, controls_.punch) : 0;
            const bool sparse = sparseN != 0;
            const unsigned count = sparse ? sparseN : capacity;
            std::array<unsigned, block> order{};
            detail::rankSpectrum (coeff, order, controls_.competition);
            std::array<unsigned, block> timeOrder{};
            if (sparse) detail::sortedByAbs (values, timeOrder);
            const auto rms = detail::rmsQuantized (energy);
            double peak = sparse ? std::abs (values[timeOrder[0]]) : 0;
            if (!sparse) for(unsigned j=0;j<count;++j) peak=std::max(peak,std::abs(coeff[order[j]]));
            const auto scaleQ = detail::scaleQuantized (peak);
            const double scale = scaleQ * 64.0 / 65535;

            packet_[cursor++] = static_cast<uint8_t> (detail::rawCount (count, sparse, version_));
            packet_[cursor++] = static_cast<uint8_t> (rms);
            packet_[cursor++] = static_cast<uint8_t> (rms >> 8);
            packet_[cursor++] = static_cast<uint8_t> (scaleQ);
            packet_[cursor++] = static_cast<uint8_t> (scaleQ >> 8);
            uint32_t bits = 0;
            unsigned used = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                const unsigned index = sparse ? timeOrder[j] : order[j];
                const double source = sparse ? values[index] : coeff[index];
                const int baseQ = version_ == 1
                                      ? detail::qExactFor (source, scale)
                                      : static_cast<int> (detail::qFor (source, scale)) - 127;
                int signedQ = detail::crunchQuantized(baseQ,controls_.crunch);
                // Very quiet nonzero blocks must retain a nonzero record for RMS reconstruction.
                if (rms != 0 && signedQ == 0 && std::equal_to<>{}(std::abs(source),peak)
                    && detail::crunchQuantized(static_cast<int>(detail::qFor(peak,scale))-127,controls_.crunch)==0)
                    signedQ = source < 0 ? -1 : 1;
                const uint32_t record = index | (uint32_t (signedQ + 127) << 7);
                bits |= record << used;
                used += 15;
                while (used >= 8)
                {
                    packet_[cursor++] = static_cast<uint8_t> (bits);
                    bits >>= 8;
                    used -= 8;
                }
            }
            if (used != 0)
            {
                packet_[cursor++] = static_cast<uint8_t> (bits);
            }
        }

        while (cursor < packetBytes_) packet_[cursor++] = 0;
        packet_[0] = static_cast<uint8_t> (packetBytes_);
        packet_[1] = static_cast<uint8_t> (packetBytes_ >> 8);
        const auto checksum = crc (packet_.data() + 6, packetBytes_ - 6);
        packet_[2] = static_cast<uint8_t> (checksum);
        packet_[3] = static_cast<uint8_t> (checksum >> 8);
        packet_[4] = static_cast<uint8_t> (checksum >> 16);
        packet_[5] = static_cast<uint8_t> (checksum >> 24);

        for (unsigned n = 0; n < block; ++n)
            for (unsigned c = 0; c < channels_; ++c)
                decodedBlock_[n * 2 + c] = 0.0;

        // Decode the packet from the fixed byte buffer. All values are known to
        // be valid because the encoder above bounded them, so this path has no
        // throwing validation branches.
        cursor = 6;
        for (unsigned c = 0; c < channels_; ++c)
        {
            const auto rawCount = static_cast<unsigned> (packet_[cursor++]);
            const bool sparse = version_ == 2 && (rawCount & 0x80u) != 0;
            const unsigned count = version_ == 2
                                     ? (sparse ? rawCount & 0x7fu
                                               : (rawCount == 0 ? block : rawCount))
                                     : rawCount;
            const auto rmsQ = static_cast<unsigned> (packet_[cursor])
                            | (static_cast<unsigned> (packet_[cursor + 1]) << 8);
            cursor += 2;
            const auto scaleQ = static_cast<unsigned> (packet_[cursor])
                              | (static_cast<unsigned> (packet_[cursor + 1]) << 8);
            cursor += 2;
            const double scale = scaleQ * 64.0 / 65535;
            uint32_t bits = 0;
            unsigned used = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                while (used < 15)
                {
                    bits |= uint32_t (packet_[cursor++]) << used;
                    used += 8;
                }
                const auto record = bits & 32767u;
                bits >>= 15;
                used -= 15;
                const unsigned index = record & 127u;
                const int q = static_cast<int> (record >> 7) - 127;
                if (sparse)
                    decodedBlock_[index * 2 + c] += q * scale / 127;
                else
                    for (unsigned n = 0; n < block; ++n)
                        decodedBlock_[n * 2 + c] += q * scale / 127 * t[index][n];
            }
            double decodedEnergy = 0.0;
            for (unsigned n = 0; n < block; ++n)
            {
                const auto x = decodedBlock_[n * 2 + c]; decodedEnergy += x * x;
            }
            const double desiredRms = static_cast<double> (rmsQ) * 4.0 / 65535;
            const double gain = decodedEnergy > 0.0
                                  ? desiredRms / std::sqrt (decodedEnergy / block) : 0.0;
            for (unsigned n = 0; n < block; ++n)
            {
                auto x = static_cast<float> (decodedBlock_[n * 2 + c] * gain);
                if (! std::isfinite (x)) x = 0.0f;
                delay_[n * 2 + c] = x;
            }
        }
    }

    EncoderControls controls_{};
    detail::EncoderState encoderState_{};
    unsigned rate_ = 0, channels_ = 0, version_ = 0, kbps_ = 0, packetBytes_ = 0;
    bool valid_ = false;
    unsigned blockFill_ = 0;
    uint64_t samplePosition_ = 0;
    std::array<float, block * 2> inputBlock_{};
    std::array<float, block * 2> delay_{};
    std::array<double, block * 2> decodedBlock_{};
    std::array<uint8_t, maxPacketBytes> packet_{};
};

// File encoding uses the same fixed packet encoder as realtime processing.
inline Bytes encode(const Audio& a, unsigned kbps, unsigned version, EncoderControls controls)
{
    controls=detail::clampControls(controls);
    if(detail::neutralControls(controls)) return encode(a,kbps,version);
    require(version==1 || version==2,"Unsupported RIT version");
    const auto packetSize=detail::packetBytesFor(a.rate,a.channels,kbps);
    require(!a.samples.empty() && a.samples.size()%a.channels==0 && a.samples.size()<=64u*1024u*1024u,"Invalid audio length");
    for(float x:a.samples) require(std::isfinite(x) && std::abs(x)<=4,"Input must be finite and within +/-4");
    const auto frames=a.samples.size()/a.channels;
    Bytes out{'R','I','T','1'};
    put(out,version,2);put(out,a.channels,1);put(out,7,1);put(out,a.rate,4);put(out,frames,8);put(out,packetSize,2);put(out,0,2);
    Streaming stream(a.rate,a.channels,kbps,version);stream.setControls(controls);
    std::array<float,block*2> input{},output{};
    for(size_t start=0;start<frames;start+=block){
        input.fill(0);
        const auto n=std::min(static_cast<size_t>(block),frames-start)*a.channels;
        std::copy_n(a.samples.data()+start*a.channels,n,input.data());
        stream.process(input.data(),output.data(),static_cast<int>(block));
        const auto& packet=stream.lastPacket();
        out.insert(out.end(),packet.begin(),packet.begin()+packetSize);
    }
    return out;
}

} // namespace rit
