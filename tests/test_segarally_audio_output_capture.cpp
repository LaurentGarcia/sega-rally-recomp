// ROM-free: direct bounded postmix packets plus the actual SDL dummy logical-device lifecycle.
#include "app/segarally_audio.h"
#include <cassert>
#include <cmath>
#include <iterator>
#include <thread>

using rally::AudioOutputCapture;
static unsigned checks=0;
#define CHECK(x) do {++checks;assert(x);} while(0)
static std::vector<uint8_t> read(const std::filesystem::path &p) {
    std::ifstream f(p,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};
}
static uint32_t word(const std::vector<uint8_t> &b,size_t p) {
    CHECK(p+4<=b.size());return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;
}
static std::vector<float> pcm(const std::filesystem::path &p,unsigned rate,unsigned channels) {
    const auto b=read(p);CHECK(b.size()>=44);CHECK(word(b,4)==b.size()-8);
    CHECK(word(b,24)==rate);CHECK(b[22]==channels);CHECK(b[20]==3);CHECK(word(b,40)==b.size()-44);
    std::vector<float> a;for(size_t i=44;i<b.size();i+=4)a.push_back(std::bit_cast<float>(word(b,i)));return a;
}
static void expect_failure(AudioOutputCapture &c,AudioOutputCapture::Failure f) {
    CHECK(c.failure()==f);bool failed=false;try{c.finish();}catch(const std::runtime_error &){failed=true;}CHECK(failed);
}
int main(int argc,char **argv) {
    CHECK(argc==2);const std::filesystem::path base=argv[1];
    const auto path=[&](const char *s){return std::filesystem::path(base.string()+s+".wav");};
    CHECK(SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE));
    // Three channels and a non-frame-aligned ring capacity exercise wrap safely.
    SDL_AudioSpec spec{SDL_AUDIO_F32,3,48000};
    std::vector<float> first{.125f,-.25f,.5f,.25f,-.5f,.75f},second{-.125f,.25f,-.5f,.375f,-.75f,.125f};
    {
        AudioOutputCapture c(path("-wrap"),spec,256,"direct","test",9,2);
        AudioOutputCapture::callback(&c,&spec,first.data(),int(first.size()*4));c.drain();
        AudioOutputCapture::callback(&c,&spec,second.data(),int(second.size()*4));c.drain();
        AudioOutputCapture::callback(&c,&spec,first.data(),int(first.size()*4));c.finish();c.finish();
        CHECK(c.frames()==6);CHECK(c.failure()==AudioOutputCapture::Failure::None);
    }
    auto expected=first;expected.insert(expected.end(),second.begin(),second.end());expected.insert(expected.end(),first.begin(),first.end());CHECK(pcm(path("-wrap"),48000,3)==expected);
    {
        AudioOutputCapture c(path("-sample-overflow"),spec,256,"direct","test",6,4);
        AudioOutputCapture::callback(&c,&spec,first.data(),int(first.size()*4));
        AudioOutputCapture::callback(&c,&spec,second.data(),int(second.size()*4));
        expect_failure(c,AudioOutputCapture::Failure::Overflow);CHECK(c.frames()==2);
    }
    CHECK(pcm(path("-sample-overflow"),48000,3)==first);
    {
        const auto text=read(path("-sample-overflow").string()+".events.txt");const std::string events(text.begin(),text.end());
        CHECK(events.find("end 2 1 1 6 1 ")!=std::string::npos); // Exact accepted prefix and explicit discarded packet/sample count.
    }
    {
        AudioOutputCapture c(path("-packet-overflow"),spec,256,"direct","test",60,1);
        AudioOutputCapture::callback(&c,&spec,first.data(),int(first.size()*4));
        AudioOutputCapture::callback(&c,&spec,second.data(),int(second.size()*4));
        expect_failure(c,AudioOutputCapture::Failure::Overflow);
    }
    for(unsigned field=0;field<3;++field) {
        const auto name=path(("-format"+std::to_string(field)).c_str());
        AudioOutputCapture c(name,spec,256,"direct","test",60,4);auto bad=spec;
        if(field==0)bad.freq=44100;else if(field==1)bad.channels=2;else bad.format=SDL_AUDIO_S16;
        AudioOutputCapture::callback(&c,&bad,first.data(),int(first.size()*4));
        expect_failure(c,AudioOutputCapture::Failure::FormatChanged);CHECK(c.frames()==0);
    }
    {
        AudioOutputCapture c(path("-invalid"),spec,256,"direct","test",60,4);
        AudioOutputCapture::callback(&c,&spec,first.data(),1);expect_failure(c,AudioOutputCapture::Failure::InvalidBuffer);
    }
    // Concurrent producer/consumer uses the same callback entry point and tests
    // exact packet order during concurrent draining; no SDL device is involved.
    {
        AudioOutputCapture c(path("-concurrent"),spec,256,"direct","test",12288,2048);
        std::atomic<bool> done=false;
        std::thread producer([&]{for(unsigned i=0;i<1000;++i)AudioOutputCapture::callback(&c,&spec,first.data(),int(first.size()*4));done.store(true,std::memory_order_release);});
        while(!done.load(std::memory_order_acquire)) {c.drain();std::this_thread::yield();}
        producer.join();c.finish();CHECK(c.frames()==2000);
    }
    auto concurrent=pcm(path("-concurrent"),48000,3);CHECK(concurrent.size()==6000);
    for(size_t i=0;i<concurrent.size();++i)CHECK(concurrent[i]==first[i%first.size()]);
    {
        rally::AudioOutput a,b;a.open();b.open();a.start_output_capture(path("-dummy"));
        std::vector<float> left(4000*2,.125f),other(4000*2,.75f);
        a.push(left);b.push(other);SDL_Delay(45);a.finish_output_capture();
        const auto frozen=read(path("-dummy"));
        a.push(first);SDL_Delay(30);a.close();b.close();CHECK(read(path("-dummy"))==frozen);
        const auto out=pcm(path("-dummy"),44100,2);CHECK(!out.empty());
        bool present=false;for(float v:out){CHECK(std::isfinite(v));CHECK(std::abs(v)<.3f);if(std::abs(v-.125f)<.00001f)present=true;}CHECK(present);
        const auto text=read(path("-dummy").string()+".events.txt");std::string events(text.begin(),text.end());
        CHECK(events.find("driver \"dummy\"")!=std::string::npos);CHECK(events.find("packet ")!=std::string::npos);
        CHECK(events.find("push ")!=std::string::npos);CHECK(events.find("end ")!=std::string::npos);
        CHECK(events.find("stopped ")!=std::string::npos);
    }
    std::printf("SDL postmix capture: %u checks passed (bounds, format, exact wrap, concurrency, logical isolation, detach)\n",checks);
}
