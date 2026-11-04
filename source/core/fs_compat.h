#pragma once
// MAC-LEGACY: std::filesystem needs macOS 10.15 at run time (its code lives in the system libc++).
// For older macOS targets (and with -DMS2K_FORCE_GHC_FS=ON for testing) the header-only
// ghc::filesystem is used instead - the same API, so the sources only name ms2k_fs.
#if defined(MS2K_USE_GHC_FILESYSTEM)
#include <ghc/filesystem.hpp>
namespace ms2k_fs = ghc::filesystem;
#else
#include <filesystem>
namespace ms2k_fs = std::filesystem;
#endif
