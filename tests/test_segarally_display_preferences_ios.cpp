// Host-side coverage of the iOS 15-compatible preference parser.
#define SEGARALLY_LEGACY_FLOAT_PARSE 1
#include "app/segarally_display_preferences.h"
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace dp=app::display_preferences;
namespace {
unsigned checks=0;
void check(bool pass) {++checks;if(!pass) throw std::runtime_error("iOS preference parser check failed");}
struct Comma : std::numpunct<char> { char do_decimal_point() const override {return ',';} };
}
int main() {try {
    const auto old=std::locale();std::locale::global(std::locale(old,new Comma));
    for(const auto *bad:{"","+0.5"," 0.5","0.5 ","0,5","0.5junk","0x1p-1","nan","inf","-inf","1e99","1e-99",".5e"}) {
        float value=.75f;check(!dp::parse_float(bad,value));check(value==.75f);
    }
    for(const auto *text:{"0","-0",".5","0.5","1.","5e-1","1e-40","1.40129846e-45","3.40282347e38"}) {
        float expected=0,value=0;const auto result=std::from_chars(text,text+std::char_traits<char>::length(text),expected);
        check(result.ec==std::errc{});check(dp::parse_float(text,value));
        check(std::bit_cast<uint32_t>(value)==std::bit_cast<uint32_t>(expected));
    }
    uint32_t state=0x23670u;
    for(unsigned n=0;n<4096;++n) {
        state=state*1664525u+1013904223u;
        const float original=std::bit_cast<float>(state);
        if(!std::isfinite(original)) continue;
        std::ostringstream text;text.imbue(std::locale::classic());
        text<<std::setprecision(std::numeric_limits<float>::max_digits10)<<original;
        float value=0;check(dp::parse_float(text.str(),value));
        check(std::bit_cast<uint32_t>(value)==std::bit_cast<uint32_t>(original));
    }
    auto settings=app::DisplaySettings{};const auto encoded=dp::encode(settings);
    app::DisplaySettings decoded;check(dp::decode(encoded,decoded));check(dp::equal(settings,decoded));
    auto invalid=encoded;const auto offset=invalid.find("scanlines ")+10;
    invalid.replace(offset,invalid.find('\n',offset)-offset,"+0.5");
    check(!dp::decode(invalid,decoded));check(dp::equal(settings,decoded));
    std::locale::global(old);
    std::printf("iOS display parser: %u checks passed\n",checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"%s at check %u\n",error.what(),checks);return 1;}}
