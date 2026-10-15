// Opt-in logical-device postmix tap. The callback only copies into bounded SPSC storage.
#pragma once
#include "app/segarally_audio_capture.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cstring>
#include <exception>
#include <iomanip>
#include <memory>
#include <vector>

namespace rally {
class AudioOutputCapture {
public:
    enum class Failure : unsigned { None, Overflow, FormatChanged, InvalidBuffer };
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    AudioOutputCapture(const std::filesystem::path &path, const SDL_AudioSpec &device_spec,
                       int device_frames, const char *driver, const char *device_name,
                       size_t sample_capacity=1024*1024, size_t packet_capacity=4096)
        : rate_(device_spec.freq), channels_(device_spec.channels),
          samples_(checked_capacity(sample_capacity,4*1024*1024)),
          packets_(checked_capacity(packet_capacity,65536)),
          wav_(path,rate_,channels_), events_(path.string()+".events.txt",std::ios::trunc) {
        if(!events_) throw std::runtime_error("Cannot open output audio capture events");
        events_<<"segarally-output-capture-v1\n"
               <<"scope logical_device_postmix_before_physical_backend\n"
               <<"driver "<<std::quoted(driver?driver:"")<<"\ndevice "<<std::quoted(device_name?device_name:"")<<'\n'
               <<"device_format "<<device_spec.format<<' '<<rate_<<' '<<channels_<<' '<<device_frames<<'\n'
               <<"wav_format float32 "<<rate_<<' '<<channels_<<"\ncapacity "<<samples_.size()<<' '<<packets_.size()
               <<"\nopened "<<SDL_GetTicksNS()<<'\n';
    }
    ~AudioOutputCapture() { try { finish(); } catch(...) {} }
    AudioOutputCapture(const AudioOutputCapture &)=delete;
    AudioOutputCapture &operator=(const AudioOutputCapture &)=delete;

    // SDL serializes callbacks on this logical device. No allocation, I/O, mutex,
    // SDL stream call, sample mutation or exception is allowed on this thread.
    static void SDLCALL callback(void *context,const SDL_AudioSpec *spec,float *buffer,int bytes) noexcept {
        auto &self=*static_cast<AudioOutputCapture *>(context);
        self.observe(spec,buffer,bytes,SDL_GetTicksNS());
    }
    void drain() {
        if(finished_) {check_failure();return;}
        const uint64_t end=written_.load(std::memory_order_acquire);
        while(read_<end) {
            const Packet packet=packets_[read_%packets_.size()];
            const size_t offset=packet.begin%samples_.size();
            const size_t first=std::min(size_t(packet.count),samples_.size()-offset);
            // Each range must retain complete frames, including at ring wrap.
            // The writer accepts interleaved samples; copy wrap into a main-thread
            // scratch packet when the split would fall inside a multichannel frame.
            if(first==packet.count) wav_.push(std::span(samples_).subspan(offset,first));
            else {
                scratch_.resize(packet.count);
                std::memcpy(scratch_.data(),samples_.data()+offset,first*sizeof(float));
                std::memcpy(scratch_.data()+first,samples_.data(),(packet.count-first)*sizeof(float));
                wav_.push(scratch_);
            }
            events_<<"packet "<<packet.at<<' '<<wav_.frames()-packet.count/channels_<<' '<<packet.count/channels_<<'\n';
            sample_read_.store(packet.begin+packet.count,std::memory_order_release);
            consumed_.store(++read_,std::memory_order_release);
        }
        if(!events_) throw std::runtime_error("Cannot write output audio capture events");
        check_failure();
    }
    void producer(uint64_t received,uint64_t submitted,unsigned before,unsigned after,float ratio) {
        if(finished_) throw std::logic_error("Output audio capture already finished");
        events_<<"push "<<SDL_GetTicksNS()<<' '<<received<<' '<<submitted<<' '<<before<<' '<<after<<' '
               <<std::bit_cast<uint32_t>(ratio)<<'\n';
        if(!events_) throw std::runtime_error("Cannot write output audio capture producer event");
    }
    void stopped(uint64_t received,uint64_t submitted,unsigned queued) {
        events_<<"stopped "<<SDL_GetTicksNS()<<' '<<received<<' '<<submitted<<' '<<queued<<'\n';
    }
    // Caller must detach the SDL callback (which synchronizes it) before finish.
    void finish() {
        if(finished_) {check_failure();return;}
        std::exception_ptr error;
        try {drain();} catch(...) {error=std::current_exception();}
        wav_.finish();
        events_<<"end "<<wav_.frames()<<' '<<read_<<' '<<dropped_packets_.load()<<' '<<dropped_samples_.load()<<' '
               <<failure_.load()<<' '<<bad_format_<<' '<<bad_rate_<<' '<<bad_channels_<<' '<<bad_bytes_<<'\n';
        events_.flush();finished_=true;
        if(!events_) throw std::runtime_error("Cannot finalize output audio capture events");
        if(error) std::rethrow_exception(error);
    }
    uint64_t frames() const {return wav_.frames();}
    Failure failure() const {return Failure(failure_.load(std::memory_order_acquire));}
private:
    struct Packet {uint64_t begin,at;size_t count;};
    static size_t checked_capacity(size_t size,size_t limit) {
        if(!size||size>limit) throw std::invalid_argument("Invalid output audio capture capacity");return size;
    }
    void observe(const SDL_AudioSpec *spec,const float *data,int bytes,uint64_t at) noexcept {
        Failure fail=Failure::None;
        if(!spec||!data||bytes<=0||channels_<=0||bytes%(channels_*int(sizeof(float)))) fail=Failure::InvalidBuffer;
        else if(spec->format!=SDL_AUDIO_F32||spec->freq!=rate_||spec->channels!=channels_) fail=Failure::FormatChanged;
        const size_t count=bytes>0?size_t(bytes)/sizeof(float):0;
        if(failure_.load(std::memory_order_relaxed)!=0) {
            dropped_packets_.fetch_add(1,std::memory_order_relaxed);dropped_samples_.fetch_add(count,std::memory_order_relaxed);return;
        }
        if(fail==Failure::None && (count>samples_.size() || sample_write_-sample_read_.load(std::memory_order_acquire)>samples_.size()-count ||
            write_-consumed_.load(std::memory_order_acquire)>=packets_.size())) fail=Failure::Overflow;
        if(fail!=Failure::None) {
            bad_format_=spec?spec->format:SDL_AUDIO_UNKNOWN;bad_rate_=spec?spec->freq:0;bad_channels_=spec?spec->channels:0;bad_bytes_=bytes;
            dropped_packets_.fetch_add(1,std::memory_order_relaxed);dropped_samples_.fetch_add(count,std::memory_order_relaxed);
            failure_.store(unsigned(fail),std::memory_order_release);return;
        }
        const size_t offset=sample_write_%samples_.size(),first=std::min(count,samples_.size()-offset);
        std::memcpy(samples_.data()+offset,data,first*sizeof(float));
        if(first<count) std::memcpy(samples_.data(),data+first,(count-first)*sizeof(float));
        packets_[write_%packets_.size()]={sample_write_,at,count};sample_write_+=count;
        written_.store(++write_,std::memory_order_release);
    }
    void check_failure() const {
        switch(failure()) {
        case Failure::None:return;
        case Failure::Overflow:throw std::runtime_error("Output audio capture overflow; artifact incomplete");
        case Failure::FormatChanged:throw std::runtime_error("Output audio capture format changed; artifact incomplete");
        case Failure::InvalidBuffer:throw std::runtime_error("Output audio capture invalid callback buffer; artifact incomplete");
        }
    }
    int rate_,channels_;
    std::vector<float> samples_,scratch_;
    std::vector<Packet> packets_;
    AudioCapture wav_;
    std::ofstream events_;
    std::atomic<uint64_t> written_{0},consumed_{0},sample_read_{0},dropped_packets_{0},dropped_samples_{0};
    std::atomic<unsigned> failure_{0};
    uint64_t write_=0,sample_write_=0,read_=0;
    SDL_AudioFormat bad_format_=SDL_AUDIO_UNKNOWN;
    int bad_rate_=0,bad_channels_=0,bad_bytes_=0;
    bool finished_=false;
};
} // namespace rally
