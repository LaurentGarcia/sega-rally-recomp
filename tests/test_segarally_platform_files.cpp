// Actual executable identity and replacement of existing saves, without SDL.
#include "app/segarally_platform_files.h"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    const auto exe=app::actual_executable();
    if (!std::filesystem::is_regular_file(exe) || !std::filesystem::equivalent(exe,argv[0])) return 2;
    const auto directory=std::filesystem::path(argv[1])/std::filesystem::path(u8"save spaces é");
    std::filesystem::create_directories(directory);
    const auto destination=directory/"eeprom.bin", temporary=directory/"eeprom.bin.tmp";
    {std::ofstream f(destination,std::ios::binary);f<<"old bytes";}
    {std::ofstream f(temporary,std::ios::binary);f<<"replacement";}
    app::replace_save_file(temporary,destination);
    std::ifstream f(destination,std::ios::binary);
    std::string actual((std::istreambuf_iterator<char>(f)),{});
    if (actual!="replacement" || std::filesystem::exists(temporary)) return 3;
    std::error_code error;
    app::replace_save_file(temporary,destination,error);
    if (!error || !std::filesystem::is_regular_file(destination)) return 4;
    std::cout<<"Executable identity and existing Unicode save replacement: PASS\n";
}
