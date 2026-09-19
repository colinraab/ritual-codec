// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "rit_stream.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <string>

static rit::Bytes readFile(const char* path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Cannot open input");
    const auto size=f.tellg();rit::require(size>0&&size<=512*1024*1024,"Input size outside reference CLI limit");
    rit::Bytes b(static_cast<size_t>(size));f.seekg(0);
    rit::require(bool(f.read(reinterpret_cast<char*>(b.data()),size)),"Read failed");return b;
}
static void writeFile(const char* path,const rit::Bytes& b) {
    std::ofstream f(path,std::ios::binary);rit::require(bool(f),"Cannot open output");
    rit::require(bool(f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()))),"Write failed");
}
static rit::Audio readWav(const rit::Bytes& b) {
    size_t p=0;rit::require(rit::get(b,p,4)==0x46464952,"Expected RIFF");
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
    rit::require((a.channels==1||a.channels==2)&&((fmt==1&&bits==16)||(fmt==3&&bits==32)),"Use PCM16 or float32 mono/stereo WAV");
    rit::require(align==a.channels*bits/8&&length>0&&length%align==0,"Invalid WAV sample count");
    p=begin;while(p<begin+length){float x=0;if(bits==16){int v=static_cast<int>(rit::get(b,p,2));if(v>=32768)v-=65536;x=static_cast<float>(v)/32768.f;}else{uint32_t v=static_cast<uint32_t>(rit::get(b,p,4));std::memcpy(&x,&v,4);}a.samples.push_back(x);}return a;
}
static rit::Bytes wav(const rit::Audio& a) {
    rit::Bytes b;rit::put(b,0x46464952,4);rit::put(b,48+a.samples.size()*4,4);rit::put(b,0x45564157,4);
    rit::put(b,0x20746d66,4);rit::put(b,16,4);rit::put(b,3,2);rit::put(b,a.channels,2);rit::put(b,a.rate,4);rit::put(b,a.rate*a.channels*4,4);rit::put(b,a.channels*4,2);rit::put(b,32,2);
    rit::put(b,0x74636166,4);rit::put(b,4,4);rit::put(b,a.samples.size()/a.channels,4);
    rit::put(b,0x61746164,4);rit::put(b,a.samples.size()*4,4);for(float x:a.samples){uint32_t v;std::memcpy(&v,&x,4);rit::put(b,v,4);}return b;
}
int main(int argc,char** argv)try {
    if(argc<4){std::cerr<<"Experimental Ritual bitstream\nritual-stream encode input.wav output.rit [kbps=128] [version=2]\nritual-stream decode input.rit output.wav\n";return 1;}
    const std::string mode=argv[1];rit::require(std::string(argv[2])!=argv[3],"Input and output must differ");
    if(mode=="encode"){
        rit::require(argc>=4&&argc<=6,"Unexpected arguments");unsigned kbps=128,version=2;
        if(argc>=5){std::string v=argv[4];size_t end=0;const auto n=std::stoul(v,&end);rit::require(end==v.size()&&n>=1&&n<=4096,"Invalid bitrate");kbps=static_cast<unsigned>(n);}
        if(argc==6){const std::string v=argv[5];rit::require(v=="1"||v=="2","Unsupported version");version=v=="1"?1u:2u;}
        const auto a=readWav(readFile(argv[2]));const auto b=rit::encode(a,kbps,version);writeFile(argv[3],b);
        const double seconds=static_cast<double>(a.samples.size())/(a.channels*a.rate);
        std::cout<<"{\"bytes\":"<<b.size()<<",\"version\":"<<version<<",\"requested_kbps\":"<<kbps<<",\"file_kbps\":"<<static_cast<double>(b.size())*8/seconds/1000<<",\"frame_samples\":128}\n";
    }else if(mode=="decode"){
        rit::require(argc==4,"Unexpected arguments");const auto a=rit::decode(readFile(argv[2]));writeFile(argv[3],wav(a));
        std::cout<<"{\"samples_per_channel\":"<<a.samples.size()/a.channels<<",\"sample_rate\":"<<a.rate<<"}\n";
    }else throw std::runtime_error("Expected encode or decode");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
