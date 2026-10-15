#include "app/segarally_startup.h"
#include "app/segarally_rom_import.h"
#include "app/segarally_platform_files.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace app::segarally {
namespace {
struct Selection {
    std::mutex mutex;
    bool finished=false;
    std::string path,error;
};
void SDLCALL selected(void *userdata,const char *const *files,int) {
    std::unique_ptr<std::shared_ptr<Selection>> owner(static_cast<std::shared_ptr<Selection> *>(userdata));
    auto state=*owner;
    std::lock_guard lock(state->mutex);
    if(!files) state->error=SDL_GetError();
    else if(files[0]) state->path=files[0];
    state->finished=true;
}
std::filesystem::path choose_rom() {
    auto state=std::make_shared<Selection>();
    // SDL owns the asynchronous callback lifetime, including cancellation.
    static const SDL_DialogFileFilter filter{"Sega Rally ROM ZIP","zip"};
    SDL_ShowOpenFileDialog(selected,new std::shared_ptr<Selection>(state),nullptr,&filter,1,nullptr,false);
    for(;;) {
        {
            std::lock_guard lock(state->mutex);
            if(state->finished) {
                if(!state->error.empty())throw std::runtime_error("Cannot open ROM chooser: "+state->error);
                return std::filesystem::u8path(state->path);
            }
        }
        SDL_Event event;
        while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_QUIT)return {};
        SDL_Delay(10);
    }
}
bool cache_name(const std::string &name) {
    if(name.size()<8 || name.size()>100 || name.rfind("import-",0)!=0)return false;
    for(char c:name)if(!((c>='0'&&c<='9') || (c>='a'&&c<='z') || c=='-'))return false;
    return true;
}
}
std::filesystem::path prepare_player_data(const std::filesystem::path &bundled,
    const std::filesystem::path &saves,const std::filesystem::path &rom,bool interactive) {
    namespace fs=std::filesystem;
    if(rom.empty() && fs::is_regular_file(bundled/"assets.json"))return bundled;
    const fs::path root=saves/"imported-game",pointer=root/"current.txt";
    if(fs::is_symlink(root) || fs::is_symlink(pointer))throw std::runtime_error("The game import folder must not be a symbolic link.");
    fs::create_directories(root);
    if(rom.empty() && fs::is_regular_file(pointer) && fs::file_size(pointer)<=128) {
        std::ifstream file(pointer);
        std::string name;std::getline(file,name);
        if(cache_name(name)) {
            try {return validate_cached_assets(root/name).directory;}
            catch(const std::exception &error) {fprintf(stderr,"Saved game import: %s\n",error.what());}
        }
    }
    fs::path selected_rom=rom;
    if(selected_rom.empty()) {
        if(!interactive)throw std::runtime_error("No imported game data. Use --rom FILE.zip or open the app to select your Sega Rally ZIP.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Sega Rally — choose your game",
            "Choose your Sega Rally Championship Revision B ROM ZIP to play.\n\nYour game files stay on this device. Import takes a few seconds; no developer tools are needed.",nullptr);
        selected_rom=choose_rom();
        if(selected_rom.empty())return {};
    }
    for(;;) {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        const std::string name="import-"+std::to_string(stamp);
        const auto destination=root/name;
        try {
            const auto result=import_rom_zip(selected_rom,destination);
            const auto temporary=root/(name+".txt");
            if(fs::exists(temporary) || fs::is_symlink(temporary))throw std::runtime_error("Import receipt already exists; please retry.");
            {
                std::ofstream file(temporary,std::ios::binary);
                file<<name<<'\n';file.flush();
                if(!file)throw std::runtime_error("Cannot save the game import location.");
            }
            app::replace_save_file(temporary,pointer);
            fprintf(stderr,"ROM import complete: verified_files=%u images=%zu\n",result.verified_files,result.images.size());
            return result.directory;
        }catch(const std::exception &error) {
            if(!interactive || !rom.empty())throw;
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Sega Rally — import failed",error.what(),nullptr);
            selected_rom=choose_rom();
            if(selected_rom.empty())return {};
        }
    }
}
}
