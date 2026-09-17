// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Experimental format 1: independently decodable 128-sample spectral packets.
// Offline reference implementation; not yet an audio-thread API.
namespace rit {
constexpr unsigned block = 128;
using Bytes = std::vector<uint8_t>;
inline void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
inline void put(Bytes& b, uint64_t v, unsigned n) {
    for (unsigned i=0;i<n;++i) b.push_back(static_cast<uint8_t>(v>>(8*i)));
}
inline uint64_t get(const Bytes& b, size_t& p, unsigned n) {
    require(p<=b.size() && n<=b.size()-p,"Truncated RIT data");
    uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p++])<<(8*i);return v;
}
inline uint32_t crc(const uint8_t* p,size_t n) {
    uint32_t v=0xffffffffu;
    for(size_t i=0;i<n;++i){v^=p[i];for(unsigned j=0;j<8;++j)v=(v>>1)^((v&1)?0xedb88320u:0u);}
    return ~v;
}
struct Audio { unsigned rate=48000, channels=2; std::vector<float> samples; };
inline const auto& basis() {
    static const auto table=[] {
        std::array<std::array<double,block>,block> t{};
        for(unsigned k=0;k<block;++k)for(unsigned n=0;n<block;++n)
            t[k][n]=std::sqrt((k?2.0:1.0)/block)*std::cos(3.14159265358979323846*(n+.5)*k/block);
        return t;
    }();return table;
}
inline Bytes encode(const Audio& a,unsigned kbps) {
    require(a.rate>=8000 && a.rate<=192000 && (a.channels==1||a.channels==2),"Unsupported RIT rate/channels");
    require(!a.samples.empty() && a.samples.size()%a.channels==0,"Invalid audio length");
    require(a.samples.size()<=64*1024*1024,"Audio exceeds reference codec limit");
    require(kbps>=1 && kbps<=4096,"Bitrate outside 1..4096 kbps");
    // Every packet has exactly this many bytes, including its length and CRC.
    const unsigned packetBytes=static_cast<unsigned>(uint64_t(kbps)*1000*block/(8*a.rate));
    require(packetBytes>=6+a.channels*7 && packetBytes<=4096,"Bitrate cannot fit packet overhead, or exceeds packet limit");
    for(float x:a.samples)require(std::isfinite(x) && std::abs(x)<=4.f,"Input must be finite and within +/-4");
    const size_t frames=a.samples.size()/a.channels;
    Bytes out{'R','I','T','1'};put(out,1,2);put(out,a.channels,1);put(out,7,1);
    put(out,a.rate,4);put(out,frames,8);put(out,packetBytes,2);put(out,0,2);
    const auto& t=basis();
    for(size_t start=0;start<frames;start+=block) {
        Bytes payload;
        const unsigned channelBytes=(packetBytes-6)/a.channels;
        const unsigned count=std::min(block,(channelBytes-5)*8/15);
        for(unsigned c=0;c<a.channels;++c) {
            std::array<double,block> coeff{};double energy=0;
            for(unsigned n=0;n<block;++n){const double x=start+n<frames?a.samples[(start+n)*a.channels+c]:0;energy+=x*x;
                for(unsigned k=0;k<block;++k)coeff[k]+=x*t[k][n];}
            std::array<unsigned,block> order{};for(unsigned k=0;k<block;++k)order[k]=k;
            std::sort(order.begin(),order.end(),[&](unsigned x,unsigned y){return std::abs(coeff[x])==std::abs(coeff[y])?x<y:std::abs(coeff[x])>std::abs(coeff[y]);});
            // Transmit actual block energy, so preserving attack energy costs bits.
            const auto rms=static_cast<unsigned>(std::lround(std::sqrt(energy/block)*65535/4));
            const auto scale=static_cast<unsigned>(std::ceil(std::abs(coeff[order[0]])*65535/64));
            put(payload,count,1);put(payload,rms,2);put(payload,scale,2);
            uint32_t bits=0;unsigned used=0;
            for(unsigned j=0;j<count;++j) {
                const unsigned k=order[j];const double s=scale*64.0/65535;
                const int q=s>0?static_cast<int>(std::lround(coeff[k]/s*127)):0;
                const uint32_t record=k|(static_cast<uint32_t>(q+127)<<7);
                bits|=record<<used;used+=15;
                while(used>=8){payload.push_back(static_cast<uint8_t>(bits));bits>>=8;used-=8;}
            }
            if(used)payload.push_back(static_cast<uint8_t>(bits));
        }
        payload.resize(packetBytes-6,0);
        put(out,packetBytes,2);put(out,crc(payload.data(),payload.size()),4);
        out.insert(out.end(),payload.begin(),payload.end());
    }
    return out;
}
inline Audio decode(const Bytes& data) {
    size_t p=0;require(get(data,p,4)==0x31544952u,"Not a RIT1 stream");
    require(get(data,p,2)==1,"Unsupported RIT version");Audio a;
    a.channels=static_cast<unsigned>(get(data,p,1));require(get(data,p,1)==7,"Unsupported frame size");
    a.rate=static_cast<unsigned>(get(data,p,4));const uint64_t frames=get(data,p,8);
    const auto packetBytes=static_cast<unsigned>(get(data,p,2));require(get(data,p,2)==0,"Unsupported flags");
    require(a.rate>=8000&&a.rate<=192000&&(a.channels==1||a.channels==2),"Invalid rate/channels");
    require(frames>0&&frames<=64*1024*1024/a.channels,"Decoded audio exceeds reference decoder limit");
    require(packetBytes>=6+a.channels*7&&packetBytes<=4096,"Invalid packet size");
    const uint64_t packets=(frames+block-1)/block;
    require(packets*packetBytes==data.size()-p,"Incorrect file length or trailing data");
    a.samples.reserve(static_cast<size_t>(frames)*a.channels);const auto& t=basis();
    for(uint64_t start=0;start<frames;start+=block) {
        require(get(data,p,2)==packetBytes,"Packet length mismatch");const auto check=get(data,p,4);
        const size_t end=p+packetBytes-6;require(crc(data.data()+p,packetBytes-6)==check,"Packet checksum mismatch");
        std::array<std::array<double,block>,2> decoded{};
        for(unsigned c=0;c<a.channels;++c) {
            require(end-p>=5,"Truncated channel header");const auto count=static_cast<unsigned>(get(data,p,1));
            require(count>0&&count<=block,"Invalid coefficient count");
            const double rms=static_cast<double>(get(data,p,2))*4/65535;
            const double scale=static_cast<double>(get(data,p,2))*64/65535;
            const unsigned bytes=(count*15+7)/8;require(bytes<=end-p,"Truncated coefficient data");
            const size_t recordEnd=p+bytes;std::array<bool,block> seen{};
            uint32_t bits=0;unsigned used=0;
            for(unsigned j=0;j<count;++j) {
                while(used<15){bits|=uint32_t(data[p++])<<used;used+=8;}
                const auto record=bits&32767u;bits>>=15;used-=15;
                const unsigned k=record&127u;const int q=static_cast<int>(record>>7)-127;
                require(!seen[k]&&q<=127,"Invalid/duplicate coefficient");seen[k]=true;
                for(unsigned n=0;n<block;++n)decoded[c][n]+=q*scale/127*t[k][n];
            }
            require(p==recordEnd&&bits==0,"Nonzero bit padding");
            double energy=0;for(double x:decoded[c])energy+=x*x;
            require(energy>0||rms==0,"Nonzero energy with empty spectrum");
            const double gain=energy>0?rms/std::sqrt(energy/block):0;
            for(double& x:decoded[c])x*=gain;
        }
        while(p<end)require(data[p++]==0,"Nonzero packet padding");
        for(unsigned n=0;n<block&&start+n<frames;++n)for(unsigned c=0;c<a.channels;++c)
            a.samples.push_back(static_cast<float>(decoded[c][n]));
    }
    return a;
}
}
