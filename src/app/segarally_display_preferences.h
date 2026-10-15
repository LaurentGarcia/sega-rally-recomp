// Small bounded presentation-only preference file; game saves are not accessed.
#pragma once
#include "app/segarally_display.h"
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>

namespace app::display_preferences {
inline bool equal(const DisplaySettings &a, const DisplaySettings &b) {
    return a.crt.enabled == b.crt.enabled && a.crt.scanlines == b.crt.scanlines &&
        a.crt.mask == b.crt.mask && a.crt.glow == b.crt.glow &&
        a.crt.curvature == b.crt.curvature && a.crt.chromatic == b.crt.chromatic &&
        a.linear_filter == b.linear_filter && a.integer_scale == b.integer_scale &&
        a.accurate_textures == b.accurate_textures;
}
inline bool valid(const DisplaySettings &s) {
    const std::array<float,5> v{s.crt.scanlines,s.crt.mask,s.crt.glow,s.crt.curvature,s.crt.chromatic};
    const std::array<float,5> max{1,1,.5f,.15f,1};
    for (size_t i=0;i<v.size();++i) if (!std::isfinite(v[i]) || v[i]<0 || v[i]>max[i]) return false;
    return true;
}
inline bool decode(const std::string &text, DisplaySettings &settings) {
    if (text.size()>4096) return false;
    std::istringstream in(text); in.imbue(std::locale::classic());
    std::string token; if (!(in>>token) || token!="segarally-display-v1") return false;
    const std::array<const char *,9> names{"enabled","scanlines","mask","glow","curvature","chromatic","linear_filter","integer_scale","accurate_textures"};
    DisplaySettings next{};
    for (size_t i=0;i<names.size();++i) {
        std::string name,value; if (!(in>>name>>value) || name!=names[i]) return false;
        if (i==0 || i>=6) {
            if (value!="0" && value!="1") return false;
            const bool b=value=="1";
            if (i==0) next.crt.enabled=b;
            if (i==6) next.linear_filter=b;
            if (i==7) next.integer_scale=b;
            if (i==8) next.accurate_textures=b;
        } else {
            float v=0; const auto result=std::from_chars(value.data(),value.data()+value.size(),v);
            if (result.ec!=std::errc{} || result.ptr!=value.data()+value.size()) return false;
            switch(i) {case 1:next.crt.scanlines=v;break;case 2:next.crt.mask=v;break;case 3:next.crt.glow=v;break;case 4:next.crt.curvature=v;break;case 5:next.crt.chromatic=v;break;default:break;}
        }
    }
    if ((in>>token) || !valid(next)) return false;
    settings=next; return true;
}
inline std::string encode(const DisplaySettings &s) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    out<<"segarally-display-v1\nenabled "<<s.crt.enabled<<"\nscanlines "<<s.crt.scanlines<<"\nmask "<<s.crt.mask<<"\nglow "<<s.crt.glow<<"\ncurvature "<<s.crt.curvature<<"\nchromatic "<<s.crt.chromatic<<"\nlinear_filter "<<s.linear_filter<<"\ninteger_scale "<<s.integer_scale<<"\naccurate_textures "<<s.accurate_textures<<'\n';
    return out.str();
}
inline bool load(const std::filesystem::path &path, DisplaySettings &settings, std::string &error) {
    std::error_code ec;const auto status=std::filesystem::symlink_status(path,ec);
    if (status.type()==std::filesystem::file_type::not_found) return true;
    if (ec || !std::filesystem::is_regular_file(status)) {error="Display preferences are not a regular file.";return false;}
    std::ifstream in(path,std::ios::binary);std::array<char,4097> bytes{};
    if (!in) {error="Cannot open display preferences.";return false;}
    in.read(bytes.data(),bytes.size());
    if (in.bad() || in.gcount()>4096 || !decode(std::string(bytes.data(),size_t(in.gcount())),settings)) {
        error="Display preferences are invalid; using current settings.";return false;
    }
    return true;
}
inline bool store(const std::filesystem::path &path, const DisplaySettings &settings, std::string &error) {
    if (!valid(settings)) {error="Display preferences are out of range.";return false;}
    if (path.empty()) {error="No display preference directory is available.";return false;}
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    if (ec) {error="Cannot create display preference directory.";return false;}
    auto temporary=path;temporary += ".tmp";
    // Never follow a leftover temporary symlink or overwrite another file type.
    const auto status=std::filesystem::symlink_status(temporary,ec);
    if (!ec && status.type()!=std::filesystem::file_type::not_found && !std::filesystem::is_regular_file(status)) {error="Cannot replace display preference temporary file.";return false;}
    if (!ec && std::filesystem::is_regular_file(status)) std::filesystem::remove(temporary,ec);
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    const auto text=encode(settings);out.write(text.data(),std::streamsize(text.size()));out.close();
    if (!out) {error="Cannot write display preferences.";std::filesystem::remove(temporary,ec);return false;}
    std::filesystem::rename(temporary,path,ec);
    if (ec) {error="Cannot replace display preferences.";std::filesystem::remove(temporary,ec);return false;}
    error.clear();return true;
}
} // namespace app::display_preferences
