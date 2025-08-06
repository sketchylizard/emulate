#pragma once
#include <cstdio>
#include <cstdint>

namespace emu6502 {

inline void trace(const char* fmt, ...) {
#ifdef ENABLE_TRACE
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
#endif
}

} // namespace emu6502
