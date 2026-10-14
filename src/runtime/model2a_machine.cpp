#include "runtime/model2a_machine.h"
#include "runtime/model2a_clock.h"
#include "runtime/gen_support.h"
#ifdef M2_SRALLY_CONTROL
#include "runtime/model2a_control.h"
#endif
#ifdef M2_SRALLY_COMM
#include "runtime/model2a_comm.h"
#endif
#ifdef M2_SRALLY_IO
#include "runtime/model2_eeprom.h"
#endif
#ifdef M2_SRALLY_UART
#include "runtime/model2a_uart.h"
#endif
#ifdef M2_SRALLY_TGP
#include "runtime/m2_tgp_board.h"
#endif
#ifdef M2_SRALLY_VIDEO
#include "runtime/model2a_video.h"
#include "runtime/geo.h"
#endif
#include <deque>
#include <limits>
#include <optional>

namespace rt {
#ifdef M2_SRALLY_TGP
namespace {
class NativeTgpModel final : public TgpBoardModel {
public:
    explicit NativeTgpModel(TgpBoard &board) : TgpBoardModel(board), board_(board) {}
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override {
        if (address >= 0x00900000 && address <= 0x0097ffff) board_.sync();
        return TgpBoardModel::read(address, mask, value);
    }
private:
    TgpBoard &board_;
};
}
#endif

struct Model2AMachine::Impl {
    Options options;
    Model2AClock clock; // Outlives every callback and attached device.
    Model2ABus bus;
    Cpu cpu;
    Lockstep schedule;
    gen::Env env;
    bool syncing = false;
    std::deque<std::pair<unsigned, bool>> irq_edges;
    std::optional<RunResult> stopped;
    uint32_t previous_dispatch = 0;
    uint64_t frames_rendered = 0, frames_parsed = 0;
    uint64_t frames_with_geometry = 0, first_geometry_frame = 0;
#ifdef M2_SRALLY_CONTROL
    std::unique_ptr<Model2AControl> control;
#endif
#ifdef M2_SRALLY_SCREEN
    std::unique_ptr<Model2AScreen> screen;
#endif
#ifdef M2_SRALLY_UART
    std::unique_ptr<Model2UART> uart_chip;
    std::unique_ptr<Model2AUART> uart;
    bool uart_clock_level = false, tx_pin_level = true;
    uint64_t tx_pin_changes = 0;
    std::function<void(uint64_t)> uart_tick;
#endif
#ifdef M2_SRALLY_COMM
    std::unique_ptr<Model2AComm> comm;
#endif
#ifdef M2_SRALLY_IO
    std::unique_ptr<Model2Eeprom> eeprom;
    std::unique_ptr<Model2AIO> io;
#endif
#ifdef M2_SRALLY_TGP
    std::unique_ptr<TgpBoard> tgp;
    std::unique_ptr<NativeTgpModel> tgp_model;
#endif
#ifdef M2_SRALLY_VIDEO
    std::unique_ptr<Geo> geo;
    std::unique_ptr<Model2AVideo> video;
#endif

    Impl(Images images, Options opts) : options(opts),
        bus(std::move(images.program), std::move(images.main_data)),
        cpu(&bus), schedule(cpu), env{cpu, schedule}
    {
        if (options.video && !options.tgp) throw Fatal("Model2A video requires TGP");
        if (options.screen && !(options.video && options.control)) throw Fatal("Model2A screen requires video and control");
        if (options.uart && !options.control) throw Fatal("Model2A UART requires control");
        if (options.external_video && !options.video) throw Fatal("external renderer requires video");
        bus.set_before_access([this] { synchronize(); });
#ifdef M2_SRALLY_CONTROL
        if (options.control) {
            control = std::make_unique<Model2AControl>(clock);
            bus.add_model(control.get());
            control->on_irq_line = [this](unsigned line, bool asserted) { irq_edges.emplace_back(line, asserted); };
        }
#else
        if (options.control) throw Fatal("control support is not compiled");
#endif
#ifdef M2_SRALLY_SCREEN
        if (options.screen) screen = std::make_unique<Model2AScreen>(clock);
#else
        if (options.screen) throw Fatal("screen support is not compiled");
#endif
#ifdef M2_SRALLY_UART
        if (options.uart) {
            uart_chip = std::make_unique<Model2UART>();
            uart = std::make_unique<Model2AUART>(*uart_chip);
            Model2UART::Callbacks callbacks;
            callbacks.tx_ready = callbacks.rx_ready = [this](bool) {
                control->sound_ready(uart_chip->tx_ready(), uart_chip->rx_ready());
            };
            callbacks.txd = [this](bool level) {
                if (level != tx_pin_level) ++tx_pin_changes;
                tx_pin_level = level;
            };
            uart_chip->set_callbacks(std::move(callbacks));
            uart->reset();
            bus.add_model(uart.get());
            uart_tick = [this](uint64_t) {
                uart_clock_level = !uart_clock_level;
                uart->clock_edge(uart_clock_level);
                clock.schedule_after(1000, uart_tick);
            };
            clock.schedule_after(1000, uart_tick);
        }
#else
        if (options.uart) throw Fatal("UART support is not compiled");
#endif
#ifdef M2_SRALLY_COMM
        if (options.comm) { comm = std::make_unique<Model2AComm>(); bus.add_model(comm.get()); }
#else
        if (options.comm) throw Fatal("communication support is not compiled");
#endif
#ifdef M2_SRALLY_IO
        if (options.io) {
            eeprom = std::make_unique<Model2Eeprom>();
            eeprom->reset();
            io = std::make_unique<Model2AIO>(*eeprom, [this] { return clock.now_ns(); });
            bus.add_model(io.get());
        }
#else
        if (options.io) throw Fatal("I/O support is not compiled");
#endif
#ifdef M2_SRALLY_TGP
        if (options.tgp) {
            tgp = std::make_unique<TgpBoard>(images.tgp_tables, images.copro_data);
            tgp_model = std::make_unique<NativeTgpModel>(*tgp);
            bus.add_model(tgp_model.get());
        }
#else
        if (options.tgp) throw Fatal("TGP support is not compiled");
#endif
#ifdef M2_SRALLY_VIDEO
        if (options.video) {
            geo = std::make_unique<Geo>(images.polygons, images.textures, tgp->buffer_data());
            video = std::make_unique<Model2AVideo>(Model2AVideo::Hooks{
                [this] {
#ifdef M2_SRALLY_SCREEN
                    if (screen) return screen->latched_frame();
#endif
                    return uint64_t(0);
                }, [this] { return uint32_t(geo->polys.size()); },
                [this](uint32_t value) { geo->zclip_w(value); }});
            video->video().set_external_3d(options.external_video, options.external_video);
            bus.add_model(video.get());
        }
#else
        if (options.video) throw Fatal("video support is not compiled");
#endif
#ifdef M2_SRALLY_SCREEN
        if (screen) {
            screen->on_begin = [this](uint64_t, uint64_t frame) {
                if (!(video->video_control() & 1) || !(frame & 1)) {
                    geo->zclip_w(video->zclip());
                    geo->parse(tgp->geo_read_start());
                    video->frame_start();
                    ++frames_parsed;
                }
                control->vblank();
#ifdef M2_SRALLY_COMM
                if (comm) comm->vblank();
#endif
            };
            screen->on_end = [this](uint64_t, uint64_t) {
                video->screen_update(geo->polys, geo->windows());
                ++frames_rendered;
                if (!geo->polys.empty()) {
                    if (!frames_with_geometry) first_geometry_frame = frames_rendered;
                    ++frames_with_geometry;
                }
            };
        }
#endif
        cpu.reset();
    }

    void synchronize() {
        if (syncing) return;
        if (cpu.reference_cycles > std::numeric_limits<uint64_t>::max() / 40)
            throw Fatal("Model2A reference clock overflow");
        syncing = true;
        try { clock.advance(cpu.reference_cycles * 40); }
        catch (...) { syncing = false; throw; }
        syncing = false;
    }

    RunResult result(Reason reason, std::string message = {}) const {
        return {reason, cpu.m_IP, schedule.count, std::move(message)};
    }
    RunResult run(uint64_t budget, bool until_frame) {
        if (stopped) return *stopped;
        if (budget > UINT64_MAX - schedule.count) throw Fatal("instruction budget overflow");
        if (until_frame && !options.screen) throw Fatal("run_until_frame requires the screen clock");
        const uint64_t target = schedule.count + budget, first_frame = frames_rendered;
        try {
            while (schedule.count < target) {
                if (options.control) {
                    synchronize();
                    while (!irq_edges.empty()) {
                        const auto [line, asserted] = irq_edges.front();
                        irq_edges.pop_front();
                        cpu.execute_set_input(int(line), asserted ? 1 : 0);
                    }
                    cpu.check_immediate_irqs();
                    schedule.end_count = schedule.count + 1;
                } else schedule.end_count = target;
                if (until_frame && frames_rendered != first_frame) return result(Reason::Frame);
                if (!gen::has_code(cpu.m_IP)) {
                    stopped = result(Reason::MissingCode, "native code target is not compiled");
                    return *stopped;
                }
                previous_dispatch = cpu.m_IP;
                gen::run(env);
                if (until_frame && frames_rendered != first_frame) return result(Reason::Frame);
            }
        } catch (const Model2AAccess &access) {
            auto failure = result(Reason::DeviceAccess, access.what());
            failure.address = access.address; failure.value = access.value;
            failure.width = access.width; failure.write = access.write;
            stopped = std::move(failure);
            return *stopped;
        } catch (const std::exception &error) {
            stopped = result(Reason::RuntimeError, error.what());
            return *stopped;
        }
        return result(Reason::Budget);
    }
};

Model2AMachine::Model2AMachine(Images images, Options options) : impl_(std::make_unique<Impl>(std::move(images), options)) {}
Model2AMachine::~Model2AMachine() = default;
Model2AMachine::RunResult Model2AMachine::run_for(uint64_t budget) { return impl_->run(budget, false); }
Model2AMachine::RunResult Model2AMachine::run_until_frame(uint64_t budget) { return impl_->run(budget, true); }
const Cpu &Model2AMachine::cpu() const { return impl_->cpu; }
const Model2ABus::Diagnostics &Model2AMachine::diagnostics() const { return impl_->bus.diagnostics(); }
Model2AMachine::Stats Model2AMachine::stats(bool hash_screen) const {
    const auto &m = *impl_;
    Stats s;
    s.instructions = m.schedule.count; s.reference_cycles = m.cpu.reference_cycles;
    s.board_time_ns = m.clock.now_ns(); s.pc = m.cpu.m_IP; s.previous_dispatch = m.previous_dispatch;
    s.frames_rendered = m.frames_rendered; s.frames_parsed = m.frames_parsed;
    s.frames_with_geometry = m.frames_with_geometry; s.first_geometry_frame = m.first_geometry_frame;
    s.irq_entries = unsigned(m.schedule.interrupts());
#ifdef M2_SRALLY_SCREEN
    if (m.screen) { s.screen_frame = m.screen->screen_frame(); s.latched_frame = m.screen->latched_frame(); }
#endif
#ifdef M2_SRALLY_TGP
    if (m.tgp) { s.tgp_booted = m.tgp->booted(); s.tgp_instructions = m.tgp->tgp_instructions(); }
#endif
#ifdef M2_SRALLY_UART
    if (m.uart_chip) { s.uart_mode = m.uart_chip->mode(); s.uart_command = m.uart_chip->command(); s.tx_pin_changes = m.tx_pin_changes; }
#endif
#ifdef M2_SRALLY_VIDEO
    if (m.geo) s.polygons = m.geo->polys.size();
    if (hash_screen && m.video) s.screen_hash = m.video->video().screen_hash();
#else
    (void)hash_screen;
#endif
    return s;
}
Video &Model2AMachine::video() {
#ifdef M2_SRALLY_VIDEO
    if (impl_->video) return impl_->video->video();
#endif
    throw Fatal("native video is not enabled");
}
Model2AIO::Inputs &Model2AMachine::inputs() {
#ifdef M2_SRALLY_IO
    if (impl_->io) return impl_->io->inputs;
#endif
    throw Fatal("native I/O is not enabled");
}
std::vector<uint8_t> Model2AMachine::save_eeprom() const {
#ifdef M2_SRALLY_IO
    if (impl_->eeprom) { const auto image = impl_->eeprom->save_image(); return {image.begin(), image.end()}; }
#endif
    throw Fatal("native EEPROM is not enabled");
}
void Model2AMachine::load_eeprom(const std::vector<uint8_t> &image) {
    if (impl_->schedule.count) throw Fatal("load EEPROM before executing the machine");
#ifdef M2_SRALLY_IO
    if (impl_->eeprom) {
        if (!impl_->eeprom->load_image(image)) throw Fatal("Model2A EEPROM image must contain 128 bytes");
        return;
    }
#endif
    throw Fatal("native EEPROM is not enabled");
}
std::vector<uint8_t> Model2AMachine::save_backup() const { return impl_->bus.save_backup(); }
void Model2AMachine::load_backup(const std::vector<uint8_t> &image) {
    if (impl_->schedule.count) throw Fatal("load backup before executing the machine");
    impl_->bus.load_backup(image);
}
} // namespace rt
