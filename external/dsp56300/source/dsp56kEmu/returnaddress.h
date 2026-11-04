#pragma once
// MS2000 (portable build): _ReturnAddress() is an MSVC intrinsic from <intrin.h>; the debug prints
// that use it get the same address from __builtin_return_address(0) on GCC/Clang.
#ifdef _MSC_VER
#include <intrin.h>
#define DSP56K_RETURN_ADDRESS() _ReturnAddress()
#else
#define DSP56K_RETURN_ADDRESS() __builtin_return_address(0)
#endif
