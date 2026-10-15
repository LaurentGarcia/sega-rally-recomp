// Included only in the opt-in test consumer, using the raw harness's two
// existing board instances and comparisons. No third provider or end sync.
        namespace rr=srally::reference_resume;
        namespace hi=srally::host_input;
        rr::load_startup_seeds(native_eeprom,nb.board,recording.initial_eeprom(),recording.initial_backup());
        rr::load_startup_seeds(reference_eeprom,rb.board,recording.initial_eeprom(),recording.initial_backup());
        uint64_t writes=0;bool pending_phase=false;
        const auto compare_phase=[&]{
            if(!same(native,ref)||nb.writes!=rb.writes||native_irqs.events!=reference_irqs.events||
               !same_io()||!same_comm()||!same_control()||!same_clock()||!same_video()||!same_uart()||
               native_frames!=reference_frames||native_parses!=reference_parses||native_renders!=reference_renders)
                throw rt::Fatal("independent recording: CPU/write/IRQ/device phase mismatch");
        };
        const auto native_witness=[&]{return hi::Witness{ls.count,native.reference_cycles,native_clock.now_ns(),native_renders,native.m_IP};};
        rr::Hooks hooks;
        hooks.synchronize=[&]{
            nb.writes.clear();rb.writes.clear();native_irqs.events.clear();reference_irqs.events.clear();
            native_frames.clear();reference_frames.clear();clear_uart_events();pending_phase=true;
            sync_native();sync_reference();
        };
        hooks.interrupts=[&]{advance_control();compare_phase();};
        hooks.board_time_ns=[&]{return reference_clock.now_ns();};hooks.frames=[&]{return reference_renders;};
        hooks.guarded_instruction=[&](uint32_t pc)->std::optional<rr::GuardedInstruction>{
            const auto at=profile.find(pc);return at==profile.end()?std::nullopt:std::optional(at->second);
        };
        hooks.inputs=&reference_io.inputs;
        hooks.after_instruction=[&]{
            const auto before=ls.count;ls.end_count=before+1;
            while(!ls.finished())gen::run(env);
            if(ls.count!=before+1)throw rt::Fatal("independent recording: native opcode count differs");
            compare_phase();writes+=nb.writes.size();pending_phase=false;
        };
        // validate() above checked the complete immutable bundle, the actual
        // host/assets and linked profile before either CPU was constructed.
        rr::Stepper stepper(ref,std::move(hooks),recording.context(),{true,true,true});
        if(stepper.witness()!=recording.initial()||native_witness()!=recording.initial())
            throw rt::Fatal("independent recording: startup witness differs");
        recording.each([&](const hi::Resume &row,const rt::Model2AMachine::RunResult &expected,uint64_t,const std::string &){
            if(native_witness()!=row.before)throw rt::Fatal("independent recording: native pre-witness differs");
            native_io.inputs=row.inputs;const auto result=stepper.consume(row);compare_phase();
            if(native_witness()!=row.after||std::tie(result.reason,result.pc,result.instructions,result.message,result.address,result.value,result.width,result.write)!=
                std::tie(expected.reason,expected.pc,expected.instructions,expected.message,expected.address,expected.value,expected.width,expected.write))
                throw rt::Fatal("independent recording: captured native witness/return differs");
            if(pending_phase){writes+=nb.writes.size();pending_phase=false;}
        });
        const auto ee=native_eeprom.save_image(),re=reference_eeprom.save_image();
        if(std::vector<uint8_t>(ee.begin(),ee.end())!=recording.final_eeprom()||ee!=re||
           nb.board.save_backup()!=recording.final_backup()||rb.board.save_backup()!=recording.final_backup()||!same_video(true))
            throw rt::Fatal("independent recording: final saves/video differ");
        recording.verify_stream_files_unchanged();report_screen();report_uart();
        std::printf("PASS independent sound-off recording records=%llu instructions=%llu writes=%llu cycles=%llu frames=%llu pc=%08x scope=independent_i960_shared_boards\n",
            (unsigned long long)recording.summary().records,(unsigned long long)ls.count,(unsigned long long)writes,
            (unsigned long long)native.reference_cycles,(unsigned long long)native_renders,native.m_IP);
        return 0;
