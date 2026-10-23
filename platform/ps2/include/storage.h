#pragma once

namespace ps2 {
// Call after the host's final IOP reset and before opening any files. Loads
// public PS2SDK USB/FAT modules and registers fileXio with newlib stdio.
// Does not reset the IOP, mount a disc, write a file, or format a device.
bool initialize_storage();
const char *storage_error();
// USB enumeration is asynchronous. Bounded read-only retries avoid treating a
// slowly enumerating stick as a missing pack. No wait is needed for host:.
bool wait_for_file(const char *path, unsigned milliseconds = 5000);
} // namespace ps2
