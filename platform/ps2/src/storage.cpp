#include "storage.h"
#include <loadfile.h>
#include <sifrpc.h>
#include <sbv_patches.h>
#include <delaythread.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <cstdio>
#include <cstring>

extern "C" {
extern unsigned char iomanX_irx[], fileXio_irx[], bdm_irx[], bdmfs_fatfs_irx[], usbd_irx[], usbmass_bd_irx[];
extern unsigned int size_iomanX_irx, size_fileXio_irx, size_bdm_irx, size_bdmfs_fatfs_irx, size_usbd_irx, size_usbmass_bd_irx;
}

namespace ps2 {
namespace {
const char *failure = "STORAGE NOT INITIALIZED";
bool loaded = false;
bool module(const char *name, unsigned char *data, unsigned size) {
    int result = -1;
    const int id = SifExecModuleBuffer(data, size, 0, nullptr, &result);
    std::printf("PS2 storage module=%s id=%d result=%d\n", name, id, result);
    if (id >= 0 && result >= 0) return true;
    failure = name;
    return false;
}
} // namespace

const char *storage_error() { return failure; }

bool initialize_storage() {
    if (loaded) return true;
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    if (!module("IOMANX LOAD FAILED", iomanX_irx, size_iomanX_irx) ||
        !module("FILEXIO LOAD FAILED", fileXio_irx, size_fileXio_irx) ||
        !module("BDM LOAD FAILED", bdm_irx, size_bdm_irx) ||
        !module("FATFS LOAD FAILED", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx) ||
        !module("USBD LOAD FAILED", usbd_irx, size_usbd_irx) ||
        !module("USBMASS LOAD FAILED", usbmass_bd_irx, size_usbmass_bd_irx)) return false;
    if (fileXioInit() < 0) { failure = "FILEXIO INIT FAILED"; return false; }
    loaded = true;
    failure = "NONE";
    return true;
}

bool wait_for_file(const char *path, unsigned milliseconds) {
    if (!path || !*path) { failure = "EMPTY PACK PATH"; return false; }
    // The caller supplies a local pack. This probe never creates or modifies it.
    if (std::strncmp(path, "mass", 4) != 0) milliseconds = 0;
    constexpr unsigned interval = 100;
    for (unsigned elapsed = 0;;) {
        if (FILE *file = std::fopen(path, "rb")) {
            std::fclose(file);
            failure = "NONE";
            return true;
        }
        if (elapsed >= milliseconds) break;
        const unsigned delay = milliseconds - elapsed < interval ? milliseconds - elapsed : interval;
        DelayThread(static_cast<s32>(delay * 1000u));
        elapsed += delay;
    }
    failure = "PACK NOT FOUND";
    return false;
}
} // namespace ps2
