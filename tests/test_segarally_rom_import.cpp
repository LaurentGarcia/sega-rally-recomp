// Includes the implementation to exercise private ZIP guards without exposing
// a caller-configurable manifest or a production verification bypass.
#include "app/segarally_rom_import.cpp"
#include <iostream>

namespace {
using namespace app::segarally;
using namespace app::segarally::detail;
unsigned checks=0;
void check(bool condition) {++checks;if(!condition)throw std::runtime_error("fixture check failed");}
template<class F> void rejects(F action) {bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed);}
void put16(Bytes &bytes,uint16_t v) {bytes.push_back(v&255);bytes.push_back(v>>8);}
void put32(Bytes &bytes,uint32_t v) {put16(bytes,v&65535);put16(bytes,v>>16);}
void set16(Bytes &bytes,size_t p,uint16_t v) {bytes.at(p)=v&255;bytes.at(p+1)=v>>8;}
void set32(Bytes &bytes,size_t p,uint32_t v) {set16(bytes,p,v&65535);set16(bytes,p+2,v>>16);}
struct Member {
    std::string name;Bytes bytes;uint16_t method;Bytes compressed;uint32_t mode;
    Member(std::string n,Bytes b,uint16_t m=0,Bytes c={},uint32_t attributes=0):
        name(std::move(n)),bytes(std::move(b)),method(m),compressed(std::move(c)),mode(attributes) {}
};
Bytes zip(std::initializer_list<Member> members) {
    Bytes output,central;
    for(const auto &m:members) {
        const auto crc=rt::crc32(m.bytes.data(),m.bytes.size());const auto &data=m.method?m.compressed:m.bytes;
        const auto offset=uint32_t(output.size());
        put32(output,0x04034b50);put16(output,20);put16(output,0);put16(output,m.method);put32(output,0);
        put32(output,crc);put32(output,uint32_t(data.size()));put32(output,uint32_t(m.bytes.size()));
        put16(output,uint16_t(m.name.size()));put16(output,0);output.insert(output.end(),m.name.begin(),m.name.end());output.insert(output.end(),data.begin(),data.end());
        put32(central,0x02014b50);put16(central,0x0314);put16(central,20);put16(central,0);put16(central,m.method);put32(central,0);
        put32(central,crc);put32(central,uint32_t(data.size()));put32(central,uint32_t(m.bytes.size()));
        put16(central,uint16_t(m.name.size()));put16(central,0);put16(central,0);put16(central,0);put16(central,0);put32(central,m.mode<<16);put32(central,offset);
        central.insert(central.end(),m.name.begin(),m.name.end());
    }
    const auto start=uint32_t(output.size());output.insert(output.end(),central.begin(),central.end());
    put32(output,0x06054b50);put16(output,0);put16(output,0);put16(output,uint16_t(members.size()));put16(output,uint16_t(members.size()));
    put32(output,uint32_t(central.size()));put32(output,start);put16(output,0);return output;
}
void save(const fs::path &path,const Bytes &bytes) {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();check(bool(file));}
}
int main(int argc,char**argv) {
    using namespace app::segarally;using namespace app::segarally::detail;
    try {
        if(argc!=2 && argc!=4)throw std::runtime_error("usage: test NEW-test-directory [personal.zip reference-assets]");
        fs::path root=fs::absolute(argv[1])/("case-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(!fs::create_directories(root))throw std::runtime_error("test output must be new");
        const Bytes hello{'h','e','l','l','o'};
        const std::string expected="aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d";
        check(sha1(hello)==expected);check(sha1({})=="da39a3ee5e6b4b0d3255bfef95601890afd80709");
        const Load fixture{"hello",nullptr,5,0x3610a686,0,expected.c_str(),false};
        const auto plain=zip({{"safe/hello",hello}});
        check(StrictZip(plain).read("hello",fixture)==hello);
        // RFC1951 raw fixed-Huffman stream generated from original text "hello".
        check(StrictZip(zip({{"hello",hello,8,{0xcb,0x48,0xcd,0xc9,0xc9,0x07,0x00}}})).read("hello",fixture)==hello);
        for(const std::string &name:std::vector<std::string>{"../hello","/hello","C:/hello","safe/../hello","safe\\hello","a//hello","./hello",std::string("bad\0hello",9)})
            rejects([&]{StrictZip unused(zip({{name,hello}}));});
        rejects([&]{StrictZip unused(zip({{"a/hello",hello},{"b/hello",hello}}));});
        rejects([&]{StrictZip unused(zip({{"hello",hello},{"hello",hello}}));});
        rejects([&]{StrictZip unused(zip({{"hello",hello,0,{},0120777}}));});
        rejects([&]{StrictZip unused(zip({{"hello",hello,99,{1,2,3}}}));});
        for(size_t length=0;length<plain.size();++length)rejects([&]{StrictZip unused(Bytes(plain.begin(),plain.begin()+length));});
        const auto central=u32(plain,plain.size()-6);
        for(auto field: {std::pair<size_t,uint32_t>{central+24,0xffffffff}, {central+42,0xffffffff}, {plain.size()-6,0xffffffff}}) {
            auto broken=plain;set32(broken,field.first,field.second);rejects([&]{StrictZip unused(broken);});
        }
        auto encrypted=plain;set16(encrypted,6,1);set16(encrypted,central+8,1);rejects([&]{StrictZip unused(encrypted);});
        auto corrupt=plain;corrupt.at(44)^=255;rejects([&]{StrictZip(corrupt).read("hello",fixture);});
        auto wrong=fixture;wrong.sha1="0000000000000000000000000000000000000000";rejects([&]{StrictZip(plain).read("hello",wrong);});
        wrong=fixture;wrong.size=4;rejects([&]{StrictZip(plain).read("hello",wrong);});
        // Malformed compressed data must throw or return bounded bytes, never
        // read/write outside the inherited inflater's supplied spans.
        uint32_t seed=0x395c425e;
        for(unsigned i=0;i<4096;++i) {
            Bytes compressed(i%48+1);for(auto &byte:compressed) {seed=seed*1664525u+1013904223u;byte=uint8_t(seed>>24);}
            Bytes output(i%32);
            try {rt::inflate(compressed.data(),compressed.size(),output);}catch(const rt::ZipError&){}
            ++checks;
        }
        fs::create_directory(root/"saves");save(root/"saves/sentinel",hello);
        save(root/"invalid.zip",plain);rejects([&]{import_rom_zip(root/"invalid.zip",root/"assets");});
        check(!fs::exists(root/"assets"));check(read_bounded(root/"saves/sentinel",5)==hello);
        fs::create_directory(root/"existing");save(root/"existing/sentinel",hello);
        rejects([&]{import_rom_zip(root/"invalid.zip",root/"existing");});check(read_bounded(root/"existing/sentinel",5)==hello);
        // An empty existing target must survive even a final rename race.
        fs::create_directory(root/"from");save(root/"from/one",hello);fs::create_directory(root/"to");
        rejects([&]{commit_no_replace(root/"from",root/"to");});check(fs::exists(root/"from/one"));check(fs::is_empty(root/"to"));
        fs::remove(root/"to");commit_no_replace(root/"from",root/"to");check(read_bounded(root/"to/one",5)==hello);
        rejects([&]{write_exclusive(root/"to/one",hello);});check(read_bounded(root/"to/one",5)==hello);
#ifndef _WIN32
        fs::create_directory(root/"linked-manifest");
        const auto canonical_manifest=manifest();save(root/"manifest.json",Bytes(canonical_manifest.begin(),canonical_manifest.end()));
        fs::create_symlink(root/"manifest.json",root/"linked-manifest/assets.json");
        rejects([&]{validate_cached_assets(root/"linked-manifest");});
        fs::create_directory_symlink(root/"existing",root/"link");rejects([&]{import_rom_zip(root/"invalid.zip",root/"link/new");});
#endif
        {Stage stage;stage.path=root/"aborted-stage";fs::create_directory(stage.path);save(stage.path/"partial",hello);}
        check(!fs::exists(root/"aborted-stage"));check(read_bounded(root/"saves/sentinel",5)==hello);
        if(argc==4) {
            const auto begin=std::chrono::steady_clock::now();
            const auto imported=import_rom_zip(argv[2],root/"actual-assets");
            const auto cached=validate_cached_assets(root/"actual-assets");
            check(imported.images.size()==8&&cached.images.size()==8&&imported.verified_files==23);
            for(const auto &image:imported.images) {
                const fs::path reference=fs::path(argv[3])/(image.name+".bin");
                check(read_bounded(reference,image.size)==read_bounded(root/"actual-assets"/(image.name+".bin"),image.size));
            }
            std::cout<<"Actual ZIP/import/cache/eight-image byte comparison seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<'\n';
            rejects([&]{import_rom_zip(argv[2],root/"actual-assets");});
            save(root/"actual-assets/program.bin",hello);rejects([&]{validate_cached_assets(root/"actual-assets");});
            check(read_bounded(root/"saves/sentinel",5)==hello);
        }
        std::cout<<"PASS "<<checks<<" checks; no guest, GPU, audio, network or subprocess imports\n";return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
