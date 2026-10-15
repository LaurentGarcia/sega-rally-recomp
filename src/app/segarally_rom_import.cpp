#include "app/segarally_rom_import.h"
#include "runtime/zip.h"
#include "runtime/sha256_digest.h"
#include <Sha1.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif

namespace app::segarally {
namespace detail {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
constexpr uint64_t max_archive = 512ull * 1024 * 1024;
constexpr uint64_t max_member = 16ull * 1024 * 1024;
[[noreturn]] void fail(const std::string &message) { throw std::runtime_error("ROM import: " + message); }

struct Load { const char *name, *alias; uint32_t size, crc, offset; const char *sha1; bool swap; };
struct Region { const char *name; uint32_t size; const char *sha256; std::span<const Load> loads; };
// Declarative metadata from the four committed Sega Rally manifests; public
// MAME model2.cpp dddd73680656e355bb2b5beecab1167c9f07bf81. No ROM bytes.
const Load loads_program[] = {
    {"epr-17888b.12", nullptr, 524288, 0x95bce0b9u, 0, "9b293b430db14cfab35466d2f9a1e3f7e2df3143", false},
    {"epr-17889b.13", nullptr, 524288, 0x395c425eu, 2, "9868d2b79255120abfdb7f9c0930a607aeef5363", false},
};
const Load loads_main_data[] = {
    {"mpr-17746.10", nullptr, 2097152, 0x8fe311f4u, 0, "f4ada8e5c906fc384bed1b96f09cdf313f89e825", false},
    {"mpr-17747.11", nullptr, 2097152, 0x543593fdu, 2, "5ba63a77e9fc70569af21d50b3171bc8ff4522b8", false},
    {"mpr-17744.8", nullptr, 2097152, 0x71fed098u, 4194304, "1d187cad375121a45348d640edd3cc7dce658d28", false},
    {"mpr-17745.9", nullptr, 2097152, 0x8ecca705u, 4194306, "ed2b3298aad6f4e52dc672a0168183e457564b43", false},
    {"mpr-17884.6", nullptr, 2097152, 0x4cfc95e1u, 8388608, "81d927b8c4f9d0c4c5e29d676b30f30f83751fdc", false},
    {"mpr-17885.7", nullptr, 2097152, 0xa08d2467u, 8388610, "9449ac8f8f9ce8d8e536b05a91e46841fed7f2d0", false},
};
const Load loads_tgp_tables[] = {
    {"opr-14742a.45", nullptr, 131072, 0x90c6b117u, 0, "f46429fffcee17d056f56d5fe035a33f1fd6c27e", false},
    {"opr-14743a.46", nullptr, 131072, 0xae7f446bu, 2, "5b9f1fc47caf21e061e930c0d72804e4ec8c7bca", false},
};
const Load loads_copro_data[] = {
    {"mpr-17754.28", "mpr-17754.29", 2097152, 0x81a84f67u, 0, "c0a9b690523a529e4015e9af10dc3fb2a1726f08", false},
    {"mpr-17755.29", "mpr-17755.28", 2097152, 0x2a6e7da4u, 2, "e60803ae951489fe47d66731d15c32249ca547b4", false},
};
const Load loads_polygons[] = {
    {"mpr-17748.16", nullptr, 2097152, 0x3148a2b2u, 0, "283cc49bfb6c6381a7ead9273fd097dca5b981b6", false},
    {"mpr-17750.20", nullptr, 2097152, 0x232aec29u, 2, "4d470e71df61298282c356814e2d151fda323fb6", false},
    {"mpr-17749.17", nullptr, 2097152, 0x0838d184u, 4194304, "704175c8b29e4c989afcb7be42e7e0e096740eaf", false},
    {"mpr-17751.21", nullptr, 2097152, 0xed87ac62u, 4194306, "601542149d33ca52a47536b4b0af47bf1fd87eb2", false},
};
const Load loads_textures[] = {
    {"mpr-17753.25", nullptr, 2097152, 0x6db0eb36u, 0, "dd5fd3c9592360d3e95623ac2491e6faabe9dbcb", false},
    {"mpr-17752.24", nullptr, 2097152, 0xd6aa86ceu, 2, "1d342f87d1af1e5438d1ae818b1b14268e765897", false},
};
const Load loads_sound_program[] = {
    {"epr-17890a.30", "epr-17890.30", 262144, 0x5bac3fa1u, 0, "3635333d36463b6fab25560ed918e05138f964dc", true},
};
const Load loads_sound_samples[] = {
    {"mpr-17756.31", nullptr, 2097152, 0x7725f111u, 0, "1f1ee3f19a6bcf57bc5a1c7dd64ee83f8b81f084", true},
    {"mpr-17757.32", nullptr, 2097152, 0x1616e649u, 2097152, "1d3a0e441d150ada0535a9d50e2f69dd4b99c584", true},
    {"mpr-17886.36", nullptr, 2097152, 0x54a72923u, 4194304, "103c4838b27378c834c08d29d6fb6ba95e7f9d03", true},
    {"mpr-17887.37", nullptr, 2097152, 0x38c31fddu, 6291456, "a85f05160b060d9d4a431aaa73cfc03f24214fb9", true},
};
const Region regions[] = {
    {"program", 2097152, "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439", loads_program},
    {"main_data", 37748736, "2c55fca2ed44c87a8886cfcfb0638ad8e25f5fac0e9fbe5d4cf990ba950cc8e1", loads_main_data},
    {"tgp_tables", 262144, "3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04", loads_tgp_tables},
    {"copro_data", 8388608, "22d65f223f3b341088c013666c8f35b522ef9b4c0a383b36f7c0b683396c524f", loads_copro_data},
    {"polygons", 33554432, "3fddc91c5b0ab9247d32518047066aa71a1db398a38d3d4f56ba3bc84c2ddfb7", loads_polygons},
    {"textures", 16777216, "38c4c6619682366c8a9553dc686a8127f38ae2385abe7a8649c152273b23302e", loads_textures},
    {"sound_program", 524288, "6be2a7a6c9c08b55291ceaf7891f2d8418e38291ed00376cf0f569a8a083ad6f", loads_sound_program},
    {"sound_samples", 8388608, "4a561c46de5400040ea21322040fbfef1c91e8dc4e9fc7a5123490187b7896ba", loads_sound_samples},
};

uint16_t u16(std::span<const uint8_t> data, size_t p) {
    if (p > data.size() || data.size() - p < 2) fail("truncated ZIP field");
    return uint16_t(data[p] | uint16_t(data[p+1]) << 8);
}
uint32_t u32(std::span<const uint8_t> data, size_t p) {
    return uint32_t(u16(data,p)) | uint32_t(u16(data,p+2)) << 16;
}
Bytes read_bounded(const fs::path &path, uint64_t bound) {
    if (!fs::is_regular_file(path)) fail("select a regular ZIP file");
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot open selected file");
    Bytes bytes; std::array<char, 65536> buffer;
    for (;;) {
        input.read(buffer.data(), buffer.size());
        const auto count = size_t(input.gcount());
        if (input.bad() || (input.fail() && !input.eof())) fail("file read failed");
        if (count > bound - bytes.size()) fail("file exceeds import limit");
        bytes.insert(bytes.end(), buffer.data(), buffer.data() + count);
        if (input.eof()) return bytes;
    }
}
std::string sha1(std::span<const uint8_t> data) {
    CSha1 state; Sha1_Init(&state);
    if (!Sha1_SetFunction(&state, SHA1_ALGO_SW)) fail("SHA1 software implementation unavailable");
    if (!data.empty()) Sha1_Update(&state, data.data(), data.size());
    std::array<uint8_t, SHA1_DIGEST_SIZE> result; Sha1_Final(&state, result.data());
    constexpr char hex[]="0123456789abcdef"; std::string text;
    for (auto byte:result) {text+=hex[byte>>4];text+=hex[byte&15];}
    return text;
}
std::string safe_basename(const std::string &name) {
    if (name.empty() || name.front()=='/' || name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos)
        fail("unsafe ZIP member path");
    size_t start=0;
    while (start<name.size()) {
        auto end=name.find('/',start); if(end==std::string::npos)end=name.size();
        auto part=name.substr(start,end-start);
        if(part.empty() || part=="." || part=="..") fail("unsafe ZIP member path");
        for (unsigned char c:part) if(c<32 || c==127) fail("unsafe ZIP member characters");
        if(end==name.size()) return part;
        start=end+1;
    }
    return {}; // safe explicit directory entry
}
struct Entry { size_t start; uint32_t compressed, size, crc; uint16_t method; };
class StrictZip {
public:
    explicit StrictZip(Bytes bytes): data(std::move(bytes)) {
        if(data.size()<22) fail("not a supported ZIP");
        size_t end=SIZE_MAX;
        for(size_t p=data.size()-22+1;p-->0 && data.size()-p<=22+65535;) {
            if(u32(data,p)==0x06054b50 && p+22+u16(data,p+20)==data.size()) {end=p;break;}
        }
        if(end==SIZE_MAX) fail("ZIP end record missing");
        const auto count=u16(data,end+10);
        if(u16(data,end+4)||u16(data,end+6)||u16(data,end+8)!=count||!count||count>2048)
            fail("multi-disk, empty or oversized ZIP directory");
        const uint64_t offset=u32(data,end+16), length=u32(data,end+12);
        if(offset+length!=end) fail("ZIP directory bounds mismatch");
        size_t p=size_t(offset); uint64_t total=0;
        std::vector<std::pair<size_t,size_t>> ranges;
        std::set<std::string> fullnames;
        for(unsigned i=0;i<count;++i) {
            if(p>end || end-p<46 || u32(data,p)!=0x02014b50) fail("bad ZIP directory entry");
            const auto flags=u16(data,p+8), method=u16(data,p+10);
            const uint32_t crc=u32(data,p+16), compressed=u32(data,p+20), size=u32(data,p+24), local=u32(data,p+42);
            const uint64_t nl=u16(data,p+28), xl=u16(data,p+30), cl=u16(data,p+32);
            if(nl==0 || p+46+nl+xl+cl>end || u16(data,p+34)) fail("bad ZIP member bounds");
            if((flags & ~uint16_t(0x080e)) || (flags & 1) || (method!=0 && method!=8)) fail("encrypted or unsupported ZIP compression");
            if(size>max_member || compressed>max_archive || uint64_t(local)+30>offset) fail("ZIP member exceeds import bounds");
            total+=size; if(total>max_archive) fail("ZIP expanded size exceeds limit");
            const auto unix_mode=u32(data,p+38)>>16;
            if((unix_mode&0170000)==0120000) fail("ZIP symbolic links are not supported");
            std::string name(reinterpret_cast<const char*>(data.data()+p+46),size_t(nl));
            const auto base=safe_basename(name);
            if(!fullnames.insert(name).second) fail("duplicate ZIP member path");
            if(u32(data,local)!=0x04034b50 || u16(data,local+6)!=flags || u16(data,local+8)!=method)
                fail("ZIP local header differs from directory");
            const uint64_t local_nl=u16(data,local+26), local_xl=u16(data,local+28);
            const uint64_t start=uint64_t(local)+30+local_nl+local_xl;
            if(start+compressed>offset || local_nl!=nl ||
               !std::equal(name.begin(),name.end(),data.begin()+local+30)) fail("ZIP local path or data bounds mismatch");
            if(!(flags&8) && (u32(data,local+14)!=crc || u32(data,local+18)!=compressed || u32(data,local+22)!=size))
                fail("ZIP local sizes or CRC differ from directory");
            for(const auto &[begin,finish]:ranges) if(local<finish && begin<start+compressed) fail("overlapping ZIP members");
            ranges.emplace_back(local,size_t(start+compressed));
            if(base.empty()) {if(size||compressed) fail("nonempty ZIP directory entry");}
            else if(!entries.emplace(base,Entry{size_t(start),compressed,size,crc,method}).second)
                fail("ambiguous ZIP basename: "+base);
            p+=size_t(46+nl+xl+cl);
        }
        if(p!=end) fail("ZIP directory length mismatch");
    }
    Bytes read(const std::string &name, const Load &load) const {
        const auto &entry=entries.at(name);
        if(entry.size!=load.size || entry.crc!=load.crc) fail("wrong Revision B size/CRC: "+name);
        Bytes output(entry.size);
        if(entry.method==0) {
            if(entry.compressed!=entry.size) fail("invalid stored ZIP size");
            std::copy_n(data.begin()+entry.start,entry.size,output.begin());
        } else rt::inflate(data.data()+entry.start,entry.compressed,output);
        if(rt::crc32(output.data(),output.size())!=load.crc || sha1(output)!=load.sha1)
            fail("wrong Revision B bytes: "+name);
        return output;
    }
    Bytes data;
    std::map<std::string,Entry> entries;
};

std::string manifest() {
    std::ostringstream text;
    text<<"{\n  \"schema\": 1,\n  \"romset\": \"srallycb\",\n  \"native_coverage_is_partial\": true,\n"
        <<"  \"provenance\": {\"kind\": \"verified_runtime_rom_import\", \"reference\": \"dddd73680656e355bb2b5beecab1167c9f07bf81\", \"generation_certified\": false},\n  \"images\": {\n";
    bool first=true;
    for(const auto &region:regions) {
        if(!first) text<<",\n"; first=false;
        text<<"    \""<<region.name<<"\": {\"file\": \""<<region.name<<".bin\", \"size\": "<<region.size<<", \"sha256\": \""<<region.sha256<<"\"}";
    }
    text<<"\n  }\n}\n"; return text.str();
}
void require_plain_path(const fs::path &path) {
    auto absolute=fs::absolute(path).lexically_normal();
    for(auto p=absolute;!p.empty();p=p.parent_path()) {
        std::error_code ec;
        const auto status=fs::symlink_status(p,ec);
        if(ec && ec!=std::errc::no_such_file_or_directory) fail("cannot inspect import path");
        if(fs::is_symlink(status)) fail("import paths cannot contain symbolic links");
#ifdef _WIN32
        const auto attrs=GetFileAttributesW(p.c_str());
        if(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&FILE_ATTRIBUTE_REPARSE_POINT)) fail("import paths cannot contain junctions or reparse points");
#endif
        if(p==p.parent_path()) break;
    }
}
void write_exclusive(const fs::path &path, std::span<const uint8_t> bytes) {
#ifdef _WIN32
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) fail("cannot create staged image");
    size_t written=0; bool ok=true;
    while(written<bytes.size()) {
        DWORD count=0; const DWORD request=DWORD(std::min<size_t>(bytes.size()-written,65536));
        if(!WriteFile(file,bytes.data()+written,request,&count,nullptr)||count!=request) {ok=false;break;}
        written+=count;
    }
    if(ok) ok=FlushFileBuffers(file)!=0;
    if(!CloseHandle(file))ok=false;
    if(!ok)fail("cannot finish writing staged image");
#else
    int file=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(file<0)fail("cannot create staged image");
    size_t written=0; bool ok=true;
    while(written<bytes.size()) {
        const auto count=write(file,bytes.data()+written,bytes.size()-written);
        if(count<0 && errno==EINTR)continue;
        if(count<=0) {ok=false;break;}
        written+=size_t(count);
    }
    if(ok && fsync(file)!=0)ok=false;
    if(close(file)!=0)ok=false;
    if(!ok)fail("cannot finish writing staged image");
#endif
}
void commit_no_replace(const fs::path &from,const fs::path &to) {
#ifdef _WIN32
    if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH)) fail("cannot commit import; destination must be new");
#elif defined(__APPLE__)
    if(renamex_np(from.c_str(),to.c_str(),RENAME_EXCL)!=0) fail("cannot commit import; destination must be new");
#elif defined(__linux__)
    if(syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),1)!=0) fail("cannot atomically commit import; destination must be new and renameat2 supported");
#else
    fail("atomic first import is unavailable on this platform");
#endif
}
struct Stage {
    fs::path path;
    ~Stage() {if(!path.empty()) {std::error_code ignored;fs::remove_all(path,ignored);}}
};
} // namespace detail

RomImportResult validate_cached_assets(const std::filesystem::path &directory) {
    using namespace detail;
    require_plain_path(directory);
    if(!fs::is_directory(directory)) fail("assets directory is missing");
    require_plain_path(directory/"assets.json");
    const auto text=read_bounded(directory/"assets.json",65536);
    const auto expected=manifest();
    if(std::string(text.begin(),text.end())!=expected)fail("assets manifest differs from the fixed Revision B identities");
    std::set<std::string> wanted{"assets.json"};
    RomImportResult result;result.directory=directory;
    for(const auto &region:regions) {
        const std::string file=std::string(region.name)+".bin";wanted.insert(file);
        require_plain_path(directory/file);
        const auto hash=rt::sha256_file(directory/file,region.size);
        if(hash.bytes!=region.size || hash.hex!=region.sha256) fail("cached image differs from Revision B: "+file);
        result.images.push_back({region.name,region.size,region.sha256});
        result.verified_files+=unsigned(region.loads.size());
    }
    for(const auto &entry:fs::directory_iterator(directory))
        if(!entry.is_regular_file()||!wanted.erase(entry.path().filename().string())) fail("unexpected file in imported assets");
    if(!wanted.empty()) fail("imported assets are incomplete");
    return result;
}

RomImportResult import_rom_zip(const std::filesystem::path &zip,const std::filesystem::path &new_directory,RomImportProgress progress) {
    using namespace detail;
    const auto destination=fs::absolute(new_directory).lexically_normal();
    require_plain_path(destination);
    if(fs::exists(destination)||!fs::is_directory(destination.parent_path())) fail("select a new assets directory under existing app storage");
    if(progress)progress("Reading your ZIP locally");
    StrictZip archive(read_bounded(zip,max_archive));
    RomImportResult result;result.directory=destination;result.archive_sha256=rt::sha256_hex(archive.data);
    std::map<std::string,Bytes> images;
    for(const auto &region:regions) {
        if(progress)progress(std::string("Verifying ")+region.name);
        Bytes image(region.size,0);
        for(const auto &load:region.loads) {
            Bytes data; bool found=false;
            for(const auto *candidate:{load.name,load.alias}) if(candidate && archive.entries.contains(candidate)) {
                data=archive.read(candidate,load);found=true;
            }
            if(!found) fail(std::string("missing ")+load.name+"; supply the complete Revision B set");
            if(load.swap) {
                for(size_t i=0;i<data.size();i+=2) {image[load.offset+i]=data[i+1];image[load.offset+i+1]=data[i];}
            } else {
                for(size_t i=0;i<data.size();i+=2) {image[load.offset+i*2]=data[i];image[load.offset+i*2+1]=data[i+1];}
            }
            ++result.verified_files;
        }
        if(rt::sha256_hex(image)!=region.sha256) fail(std::string("assembled image differs from fixed Revision B: ")+region.name);
        result.images.push_back({region.name,region.size,region.sha256});
        images.emplace(region.name,std::move(image));
    }
    if(progress)progress("Saving verified game data locally");
    Stage stage;
    for(unsigned attempt=0;attempt<32;++attempt) {
        const auto nonce=std::to_string(std::random_device{}())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto candidate=destination.parent_path()/(".segarally-import-"+nonce);
        if(fs::create_directory(candidate)) {stage.path=std::move(candidate);break;}
    }
    if(stage.path.empty()) fail("cannot reserve temporary import storage");
#ifndef _WIN32
    fs::permissions(stage.path,fs::perms::owner_all,fs::perm_options::replace);
#endif
    for(const auto &[name,bytes]:images)write_exclusive(stage.path/(name+".bin"),bytes);
    const auto text=manifest();
    write_exclusive(stage.path/"assets.json",{reinterpret_cast<const uint8_t*>(text.data()),text.size()});
#ifndef _WIN32
    const int stage_fd=open(stage.path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
    if(stage_fd<0) fail("cannot flush staged assets directory");
    const bool flushed=fsync(stage_fd)==0;
    const bool closed=close(stage_fd)==0;
    if(!flushed||!closed) fail("cannot finish staged assets directory");
#endif
    require_plain_path(destination);
    commit_no_replace(stage.path,destination);stage.path.clear();
    return result;
}
} // namespace app::segarally
