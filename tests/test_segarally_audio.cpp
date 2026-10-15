// Pinned SDL3 dummy playback exercises the actual stream/lifecycle functions.
#include "app/segarally_audio.h"
#include "app/segarally_audio_capture.h"
#include <cassert>
#include <iterator>
#include <vector>
int main(int argc,char **argv) {
    assert(argc==2);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    rally::AudioOutput audio;
    audio.enable_diagnostics(true);
    audio.open();
    assert(audio.demand_snapshot().last_shortfall_ns==0);
    const auto observed_start=SDL_GetTicksNS();
    assert(!audio.started() && audio.queued_frames()==0);
    const std::vector<float> small(100*2,0.125f);
    audio.push(small);assert(audio.queued_frames()==100 && !audio.started());
    audio.clear();assert(audio.queued_frames()==0 && !audio.started());
    bool failed=false;
    try {audio.push(std::span(small).first(1));} catch(const std::invalid_argument &) {failed=true;}
    assert(failed);
    std::vector<float> large((rally::AudioOutput::kMaximumFrames+1)*2,0.25f);
    audio.push(large);
    assert(audio.started());
    assert(audio.queued_frames()<=rally::AudioOutput::kPrimeFrames);
    assert(audio.dropped_frames()==large.size()/2-rally::AudioOutput::kPrimeFrames);
    assert(audio.submitted_frames()==100+rally::AudioOutput::kPrimeFrames);
    SDL_Delay(180); // Let the dummy device consume the queue without a producer.
    assert(audio.shortfall_requests()>0 && audio.requested_extra_bytes()>0);
    const auto demand=audio.demand_snapshot();
    assert(demand.requests>0 && demand.extra_input_bytes>0);
    assert(demand.last_shortfall_ns>=observed_start && demand.last_shortfall_ns<=SDL_GetTicksNS());
    SDL_Delay(30);
    assert(audio.demand_snapshot().last_shortfall_ns>=demand.last_shortfall_ns);
    audio.clear();assert(audio.queued_frames()==0 && !audio.started());
    audio.push(small);assert(audio.queued_frames()==100);
    audio.close();assert(audio.queued_frames()==0 && !audio.started());
    // Timestamp work is opt-in even when positive demand is observed.
    rally::AudioOutput ordinary;ordinary.open();ordinary.push(large);SDL_Delay(180);
    assert(ordinary.shortfall_requests()>0 && ordinary.demand_snapshot().last_shortfall_ns==0);
    ordinary.close();
    const std::vector<float> pcm{0.125f,-0.25f,0.5f,-1.0f};
    {
        rally::AudioCapture capture(argv[1]);capture.push(pcm);assert(capture.frames()==2);
        capture.finish();capture.finish();
    }
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    assert(bytes.size()==60);
    auto word=[&](size_t i) {return uint32_t(bytes[i])|uint32_t(bytes[i+1])<<8|
        uint32_t(bytes[i+2])<<16|uint32_t(bytes[i+3])<<24;};
    assert(word(4)==52 && word(24)==44100 && word(40)==16 && bytes[20]==3);
    for(size_t i=0;i<pcm.size();++i) assert(word(44+i*4)==std::bit_cast<uint32_t>(pcm[i]));
    std::puts("SDL3 audio: priming, bounds, lifecycle, stereo validation and exact float WAV passed");
}
