// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once
#include "rit_stream.hpp"
#include <fstream>
#include <cstring>
#include <string>
#include <memory>
#include <filesystem>
#ifdef RITUAL_CLI_SNDFILE
#include <sndfile.h>
#endif
namespace ritual_input {
inline rit::Bytes readFile(const char* path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Cannot open input");
    const auto size=f.tellg();rit::require(size>0&&size<=512*1024*1024,"Input size outside reference CLI limit");
    rit::Bytes b(static_cast<size_t>(size));f.seekg(0);
    rit::require(bool(f.read(reinterpret_cast<char*>(b.data()),size)),"Read failed");return b;
}
inline rit::Audio readWav(const rit::Bytes& b) {
    size_t p=0;rit::require(rit::get(b,p,4)==0x46464952,"This build reads PCM16/24 or float32 WAV only. For MP3/FLAC, install libsndfile and rebuild with -DRITUAL_CLI_SNDFILE=ON (see docs/BUILDING.md), or convert the input to WAV first.");
    const auto riff=rit::get(b,p,4);rit::require(riff+8==b.size(),"Invalid RIFF length");
    rit::require(rit::get(b,p,4)==0x45564157,"Expected WAVE");
    rit::Audio a;unsigned fmt=0,bits=0,align=0;size_t begin=0,length=0;
    while(p<b.size()) {
        const auto tag=rit::get(b,p,4),size=rit::get(b,p,4);const size_t start=p;
        rit::require(size<=b.size()-p,"Truncated WAV chunk");
        if(tag==0x20746d66){rit::require(size>=16&&fmt==0,"Invalid/duplicate fmt");fmt=static_cast<unsigned>(rit::get(b,p,2));a.channels=static_cast<unsigned>(rit::get(b,p,2));a.rate=static_cast<unsigned>(rit::get(b,p,4));rit::get(b,p,4);align=static_cast<unsigned>(rit::get(b,p,2));bits=static_cast<unsigned>(rit::get(b,p,2));}
        if(tag==0x61746164){rit::require(begin==0,"Duplicate data chunk");begin=start;length=static_cast<size_t>(size);}
        p=start+static_cast<size_t>(size)+(size&1);rit::require(p<=b.size(),"Missing WAV pad byte");
    }
    rit::require((a.channels==1||a.channels==2)&&((fmt==1&&(bits==16||bits==24))||(fmt==3&&bits==32)),"Use PCM16/24 or float32 WAV, or build with RITUAL_CLI_SNDFILE=ON for MP3/FLAC and extended WAV");
    rit::require(align==a.channels*bits/8&&length>0&&length%align==0,"Invalid WAV sample count");
    rit::require(length/(bits/8)<=64u*1024u*1024u,"Decoded audio exceeds sample limit");
    a.samples.reserve(length/(bits/8));
    p=begin;while(p<begin+length){float x=0;if(fmt==1){const int half=1<<(bits-1);int v=static_cast<int>(rit::get(b,p,bits/8));if(v>=half)v-=half*2;x=static_cast<float>(v)/static_cast<float>(half);}else{uint32_t v=static_cast<uint32_t>(rit::get(b,p,4));std::memcpy(&x,&v,4);}a.samples.push_back(x);}return a;
}

inline rit::Audio read(const char* path) {
#ifdef RITUAL_CLI_SNDFILE
    const auto bytes = std::filesystem::file_size(path);
    rit::require(bytes > 0 && bytes <= 512u*1024u*1024u, "Input size outside reference CLI limit");
    SF_INFO info{};
    std::unique_ptr<SNDFILE, decltype(&sf_close)> file(sf_open(path, SFM_READ, &info), sf_close);
    if (!file) throw std::runtime_error(std::string("Audio input: ") + sf_strerror(nullptr));
    rit::require(info.channels == 1 || info.channels == 2, "Input must be mono or stereo; no automatic downmix");
    rit::require(info.samplerate >= 8000 && info.samplerate <= 192000, "Input rate must be 8–192 kHz; no automatic resampling");
    rit::Audio a; a.channels = static_cast<unsigned>(info.channels); a.rate = static_cast<unsigned>(info.samplerate);
    constexpr size_t limit = 64u*1024u*1024u;
    rit::require(info.frames >= 0 && static_cast<uint64_t>(info.frames) <= limit/a.channels, "Decoded audio exceeds sample limit");
    std::array<float, 8192> buffer{};
    for (;;) {
        const auto frames = sf_readf_float(file.get(), buffer.data(), 4096);
        if (sf_error(file.get()) != SF_ERR_NO_ERROR) throw std::runtime_error(sf_strerror(file.get()));
        if (frames == 0) break;
        rit::require(frames > 0 && frames <= 4096, "Invalid decoder frame count");
        const size_t count = static_cast<size_t>(frames)*a.channels;
        rit::require(count <= limit-a.samples.size(), "Decoded audio exceeds sample limit");
        a.samples.insert(a.samples.end(), buffer.begin(), buffer.begin()+static_cast<std::ptrdiff_t>(count));
    }
    rit::require(!a.samples.empty(), "Empty audio input");
    return a;
#else
    return readWav(readFile(path));
#endif
}
inline void differentFiles(const char* input, const char* output) {
    const auto in = std::filesystem::weakly_canonical(input);
    const auto out = std::filesystem::weakly_canonical(output);
    rit::require(in != out && !(std::filesystem::exists(out) && std::filesystem::equivalent(in,out)), "Input and output must differ");
}
} // namespace ritual_input
