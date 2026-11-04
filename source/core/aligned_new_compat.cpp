// MAC-LEGACY: the system libc++ of macOS before 10.14 has no aligned operator new/delete
// (C++17, used for the alignas(64) members of the emulator classes). A program may replace these
// global functions, so on such targets they are defined here, on posix_memalign/free.
// Built only for macOS deployment targets below 10.14 (see CMakeLists.txt).
#include <cstdlib>
#include <new>

namespace
{
void* ms2kAlignedAlloc(std::size_t size, std::align_val_t align) noexcept
{
    std::size_t a = static_cast<std::size_t>(align);
    if (a < sizeof(void*)) a = sizeof(void*);
    void* p = nullptr;
    if (posix_memalign(&p, a, size ? size : 1) != 0) return nullptr;
    return p;
}
}

void* operator new(std::size_t size, std::align_val_t align)
{
    if (void* p = ms2kAlignedAlloc(size, align)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t align) { return ::operator new(size, align); }
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { return ms2kAlignedAlloc(size, align); }
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { return ms2kAlignedAlloc(size, align); }

void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
