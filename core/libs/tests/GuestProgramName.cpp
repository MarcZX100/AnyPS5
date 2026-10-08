#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
const char* APS5_VABI getprogname_nid_postfix();
}

int main() {
    const char* name = getprogname_nid_postfix();
    if (name == nullptr || std::strcmp(name, "eboot.bin") != 0) std::abort();
}
