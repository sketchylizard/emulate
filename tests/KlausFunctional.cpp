// Source: /home/jason/projects/emulate/build/clang/debug/_deps/master-src/bin_files/6502_functional_test.lst
// Credit: Klaus Dormann — https://github.com/Klaus2m5/6502_65C02_functional_tests

#include "KlausFunctional.h"

#include <span>

#include "common/address.h"
#include "incbin.h"

extern "C" {
INCBIN(Common::Byte, _Klaus__6502_functional_testBin,
    "../build/clang/debug/_deps/master-src/bin_files/6502_functional_test.bin");
}

std::span<const Common::Byte> Klaus__6502_functional_test::data() noexcept
{
  return std::span<const Common::Byte>(g_Klaus__6502_functional_testBinData, g_Klaus__6502_functional_testBinSize);
}
