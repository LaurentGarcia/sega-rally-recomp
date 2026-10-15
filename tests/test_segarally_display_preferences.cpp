#include "app/segarally_display_preferences.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace dp=app::display_preferences;
unsigned checks=0;
void check(bool v) {++checks;if (!v) throw std::runtime_error("display preference check failed");}
std::string replaced(std::string text,const std::string &key,const std::string &value) {
    const auto start=text.find(key+" ");check(start!=std::string::npos);const auto stop=text.find('\n',start);
    text.replace(start,stop-start,key+" "+value);return text;
}
int main() {try {
    app::DisplaySettings defaults;check(!defaults.crt.enabled && defaults.linear_filter && !defaults.integer_scale && !defaults.accurate_textures);
    check(dp::valid(defaults));
    for (int n=0;n<33;++n) {
        app::DisplaySettings a;a.crt.enabled=n&1;a.linear_filter=n&2;a.integer_scale=n&4;a.accurate_textures=n&8;
        a.crt.scanlines=float(n)/32;a.crt.mask=float(32-n)/32;a.crt.glow=float(n)/64;a.crt.curvature=float(n)/32*.15f;a.crt.chromatic=float(n)/32;
        app::DisplaySettings b;check(dp::decode(dp::encode(a),b));check(dp::equal(a,b));
    }
    const auto good=dp::encode(defaults);
    for (const auto &bad: {std::string{},std::string("segarally-display-v2\n"),good+"unknown 1\n",good+"enabled 1\n",good.substr(0,good.size()-3),std::string(4097,' '),good+std::string(4096,' ')}) {
        auto b=defaults;check(!dp::decode(bad,b));check(dp::equal(b,defaults));
    }
    for (const auto *key:{"scanlines","mask","glow","curvature","chromatic"})
        for (const auto *bad:{"nan","inf","-inf","-0.001","1e99","0.2trailing","9999"}) {
            auto b=defaults;check(!dp::decode(replaced(good,key,bad),b));check(dp::equal(b,defaults));
        }
    for (const auto &[key,over]:std::array<std::pair<const char *,const char *>,5>{{{"scanlines","1.0001"},{"mask","1.0001"},{"glow","0.5001"},{"curvature","0.1501"},{"chromatic","1.0001"}}}) {
        auto b=defaults;check(!dp::decode(replaced(good,key,over),b));check(dp::equal(b,defaults));
    }
    for (const auto *key:{"enabled","linear_filter","integer_scale","accurate_textures"})
        for (const auto *bad:{"true","2","-1","1.0"}) {auto b=defaults;check(!dp::decode(replaced(good,key,bad),b));}
    const auto dir=std::filesystem::temp_directory_path()/("segarally-display-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    try {
        const auto path=dir/"display.settings";std::string error;auto b=defaults;
        check(dp::load(path,b,error));check(!std::filesystem::exists(path));
        std::ofstream(dir/"eeprom.bin",std::ios::binary)<<"unrelated EEPROM sentinel";
        std::ofstream(dir/"backup.bin",std::ios::binary)<<"unrelated backup sentinel";
        check(dp::store(path,defaults,error));check(dp::load(path,b,error));check(dp::equal(defaults,b));
        auto changed=defaults;changed.crt.enabled=true;changed.crt.scanlines=1;changed.accurate_textures=true;
        check(dp::store(path,changed,error));check(dp::load(path,b,error));check(dp::equal(changed,b));
        {std::ofstream f(path,std::ios::binary|std::ios::trunc);f<<std::string(4097,'x');}
        b=defaults;check(!dp::load(path,b,error));check(dp::equal(defaults,b));
        std::filesystem::remove(path);std::filesystem::create_symlink(dir/"eeprom.bin",path);
        check(!dp::load(path,b,error));std::filesystem::remove(path);
        auto temp=path;temp += ".tmp";std::filesystem::create_symlink(dir/"backup.bin",temp);
        check(!dp::store(path,changed,error));std::filesystem::remove(temp);
        std::ifstream ee(dir/"eeprom.bin"),backup(dir/"backup.bin");std::string a,c;std::getline(ee,a);std::getline(backup,c);
        check(a=="unrelated EEPROM sentinel" && c=="unrelated backup sentinel");
        changed.crt.glow=std::numeric_limits<float>::quiet_NaN();check(!dp::store(path,changed,error));check(!std::filesystem::exists(path));
        check(!std::filesystem::exists(temp));
    }catch(...){std::filesystem::remove_all(dir);throw;}
    std::filesystem::remove_all(dir);
    std::printf("Display preferences: %u checks passed\n",checks);return 0;
}catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
