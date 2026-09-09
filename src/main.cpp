// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "bitfamine.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
static uint32_t read32(std::istream& s){unsigned char b[4]{};if(!s.read((char*)b,4))throw std::runtime_error("Truncated WAV header");return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
static uint16_t read16(std::istream& s){unsigned char b[2]{};if(!s.read((char*)b,2))throw std::runtime_error("Truncated WAV header");return b[0]|(b[1]<<8);}
static void w32(std::ostream& s,uint32_t x){for(int i=0;i<4;i++)s.put(char(x>>(8*i)));}
static void w16(std::ostream& s,uint16_t x){s.put(char(x));s.put(char(x>>8));}
int main(int argc,char** argv)try{
 if(argc<3){std::cerr<<"Usage: ritual-codec INPUT.wav OUTPUT.wav [budget competition memory punch crunch clock downsample transparent]\nInput: PCM16 or float32 mono/stereo WAV; output float32. Delay trimmed, tail flushed.\n";return 1;}
 bf::Params p;
 if(argc>3)p.budget=std::stof(argv[3]);if(argc>4)p.competition=std::stof(argv[4]);if(argc>5)p.memory=std::stof(argv[5]);if(argc>6)p.punch=std::stof(argv[6]);if(argc>7)p.crunch=std::stof(argv[7]);if(argc>8)p.clock=std::stoi(argv[8]);if(argc>9)p.downsample=std::stoi(argv[9]);if(argc>10)p.transparent=std::stoi(argv[10])!=0;
 std::ifstream f(argv[1],std::ios::binary);if(!f)throw std::runtime_error("Cannot open input");
 char tag[4];f.read(tag,4);if(std::memcmp(tag,"RIFF",4))throw std::runtime_error("Expected RIFF");read32(f);f.read(tag,4);if(std::memcmp(tag,"WAVE",4))throw std::runtime_error("Expected WAVE");
 int format=0,ch=0,sr=0,bits=0;std::vector<char> data;
 while(f.read(tag,4)) {uint32_t size=read32(f);if(!f||size>512*1024*1024)throw std::runtime_error("Invalid chunk");auto start=f.tellg();
 if(!std::memcmp(tag,"fmt ",4)){if(size<16)throw std::runtime_error("Invalid fmt");format=read16(f);ch=read16(f);sr=read32(f);read32(f);read16(f);bits=read16(f);}
 else if(!std::memcmp(tag,"data",4)){data.resize(size);f.read(data.data(),size);if(!f)throw std::runtime_error("Truncated data");}
 f.seekg(start+std::streamoff(size+(size&1)));}
 if(ch<1||ch>2||!((format==1&&bits==16)||(format==3&&bits==32))||data.empty()||data.size()%(ch*bits/8))throw std::runtime_error("Use PCM16 or IEEE float32 mono/stereo WAV");
 size_t n=data.size()/(bits/8),count=n/ch;std::vector<float> input(n+bf::N*ch,0),output(input.size());
 for(size_t i=0;i<n;i++){if(bits==16){int16_t v=int16_t(uint8_t(data[2*i])|(uint16_t(uint8_t(data[2*i+1]))<<8));input[i]=v/32768.f;}else {uint32_t v=0;for(int b=0;b<4;b++)v|=uint32_t(uint8_t(data[4*i+b]))<<(8*b);std::memcpy(&input[i],&v,4);}}
 bf::Engine engine(sr,ch,p);auto begin=std::chrono::steady_clock::now();
 for(size_t i=0;i<count+bf::N;i+=64){int block=std::min<size_t>(64,count+bf::N-i);engine.process(input.data()+i*ch,output.data()+i*ch,block);}
 double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
 std::ofstream o(argv[2],std::ios::binary);if(!o)throw std::runtime_error("Cannot open output");o.write("RIFF",4);w32(o,48+n*4);o.write("WAVEfmt ",8);w32(o,16);w16(o,3);w16(o,ch);w32(o,sr);w32(o,sr*ch*4);w16(o,ch*4);w16(o,32);o.write("fact",4);w32(o,4);w32(o,count);o.write("data",4);w32(o,n*4);
 float peak=0;for(size_t i=bf::N*ch;i<output.size();i++){float v=output[i];if(!std::isfinite(v))throw std::runtime_error("Nonfinite output");peak=std::max(peak,std::abs(v));uint32_t u;std::memcpy(&u,&v,4);w32(o,u);}if(!o)throw std::runtime_error("Write failed");
 std::cout<<"{\"latency_samples\":"<<bf::N<<",\"latency_ms\":"<<1000.*bf::N/sr<<",\"seconds\":"<<elapsed<<",\"realtime_factor\":"<<elapsed/(double(count)/sr)<<",\"peak\":"<<peak<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
