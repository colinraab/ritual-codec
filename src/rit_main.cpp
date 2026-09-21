// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "audio_input.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <string>

static void writeFile(const char* path,const rit::Bytes& b) {
    std::ofstream f(path,std::ios::binary);rit::require(bool(f),"Cannot open output");
    rit::require(bool(f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()))),"Write failed");
}
static rit::Bytes wav(const rit::Audio& a) {
    rit::Bytes b;rit::put(b,0x46464952,4);rit::put(b,48+a.samples.size()*4,4);rit::put(b,0x45564157,4);
    rit::put(b,0x20746d66,4);rit::put(b,16,4);rit::put(b,3,2);rit::put(b,a.channels,2);rit::put(b,a.rate,4);rit::put(b,a.rate*a.channels*4,4);rit::put(b,a.channels*4,2);rit::put(b,32,2);
    rit::put(b,0x74636166,4);rit::put(b,4,4);rit::put(b,a.samples.size()/a.channels,4);
    rit::put(b,0x61746164,4);rit::put(b,a.samples.size()*4,4);for(float x:a.samples){uint32_t v;std::memcpy(&v,&x,4);rit::put(b,v,4);}return b;
}
int main(int argc,char** argv)try {
    if(argc<4){std::cerr<<"Experimental Ritual bitstream\nritual-stream encode input-audio output.rit [kbps=128] [version=2]\nritual-stream decode input.rit output.wav\n";return 1;}
    ritual_input::differentFiles(argv[2],argv[3]);
    const std::string mode=argv[1];rit::require(std::string(argv[2])!=argv[3],"Input and output must differ");
    if(mode=="encode"){
        rit::require(argc>=4&&argc<=6,"Unexpected arguments");unsigned kbps=128,version=2;
        if(argc>=5){std::string v=argv[4];size_t end=0;const auto n=std::stoul(v,&end);rit::require(end==v.size()&&n>=1&&n<=4096,"Invalid bitrate");kbps=static_cast<unsigned>(n);}
        if(argc==6){const std::string v=argv[5];rit::require(v=="1"||v=="2","Unsupported version");version=v=="1"?1u:2u;}
        const auto a=ritual_input::read(argv[2]);const auto b=rit::encode(a,kbps,version);writeFile(argv[3],b);
        const double seconds=static_cast<double>(a.samples.size())/(a.channels*a.rate);
        std::cout<<"{\"bytes\":"<<b.size()<<",\"version\":"<<version<<",\"requested_kbps\":"<<kbps<<",\"file_kbps\":"<<static_cast<double>(b.size())*8/seconds/1000<<",\"frame_samples\":128}\n";
    }else if(mode=="decode"){
        rit::require(argc==4,"Unexpected arguments");const auto a=rit::decode(ritual_input::readFile(argv[2]));writeFile(argv[3],wav(a));
        std::cout<<"{\"samples_per_channel\":"<<a.samples.size()/a.channels<<",\"sample_rate\":"<<a.rate<<"}\n";
    }else throw std::runtime_error("Expected encode or decode");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
