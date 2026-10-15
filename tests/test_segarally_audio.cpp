// Pinned SDL3 dummy playback exercises the actual stream/lifecycle functions.
#include "app/segarally_audio.h"
#include "app/segarally_audio_capture.h"
#include <cassert>
#include <iterator>
#include <climits>
#include <vector>
static void queue_policy() {
    rally::AudioQueuePolicy policy;
    assert(policy.target_frames==2646 && policy.burst_frames==0);
    policy.configure(44100,1024);assert(policy.burst_frames==1024 && policy.target_frames==3670);
    policy.configure(48000,1024);assert(policy.burst_frames==941 && policy.target_frames==3587);
    policy.configure(96000,2048);assert(policy.burst_frames==941);
    policy.configure(44100,2646);assert(policy.target_frames==5292);
    for(const auto [rate,frames]:{std::pair{0,1024},std::pair{44100,0},std::pair{-1,1024},
                                 std::pair{44100,-1},std::pair{44100,2647},std::pair{1,INT_MAX}}) {
        bool rejected=false;try{policy.configure(rate,frames);}catch(const std::runtime_error &){rejected=true;}
        assert(rejected && policy.target_frames==5292);
    }
    assert(policy.ratio(policy.target_frames)==1.f);
    assert(policy.ratio(0)==.995f && policy.ratio(rally::AudioQueuePolicy::kMaximumFrames)==1.005f);
    // With either block below target, their first threshold-crossing sum cannot
    // hit the backlog-clear condition. This guards large-period startup deadlock.
    assert(2*(policy.target_frames-1)<rally::AudioQueuePolicy::kMaximumFrames);
    const auto shortage=[](bool device_headroom) {
        rally::AudioQueuePolicy p;if(device_headroom)p.configure(44100,1024);
        SDL_AudioSpec format{SDL_AUDIO_F32,2,44100};auto *s=SDL_CreateAudioStream(&format,&format);assert(s);
        const auto push=[&](unsigned frames) {
            std::vector<float> native(frames*2,.25f);assert(SDL_PutAudioStreamData(s,native.data(),int(native.size()*4)));
            assert(SDL_SetAudioStreamFrequencyRatio(s,p.ratio(unsigned(SDL_GetAudioStreamQueued(s))/8)));
        };
        unsigned missing=0;
        const auto pull=[&] {
            std::vector<float> output(1024*2);const int bytes=SDL_GetAudioStreamData(s,output.data(),int(output.size()*4));
            assert(bytes>=0 && bytes<=8192 && bytes%8==0);missing+=1024-unsigned(bytes)/8;
            for(int i=0;i<bytes/4;++i)assert(output[i]>.2f && output[i]<.3f);
        };
        // A recorded53ms producer pause spans two1024-frame callbacks. A late
        //370-frame push precedes the third callback. The current60ms policy
        //runs short; device-period headroom covers this same demand sequence.
        push(p.target_frames-229);pull();pull();push(370);pull();
        SDL_DestroyAudioStream(s);return missing;
    };
    const auto old_missing=shortage(false),new_missing=shortage(true);
    assert(old_missing>0 && old_missing<300 && new_missing==0);
    std::printf("SDL queue policy: baseline missing=%u new missing=%u; rate/bounds/startup checks passed\n",old_missing,new_missing);
}
int main(int argc,char **argv) {
    assert(argc==2);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    rally::AudioOutput audio;
    audio.enable_diagnostics(true);
    audio.open();
    queue_policy();
    assert(audio.target_frames()>rally::AudioOutput::kPrimeFrames);
    assert(audio.target_frames()<=rally::AudioOutput::kMaximumFrames/2);
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
    assert(audio.queued_frames()<=audio.target_frames());
    assert(audio.dropped_frames()==large.size()/2-audio.target_frames());
    assert(audio.submitted_frames()==100+audio.target_frames());
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
