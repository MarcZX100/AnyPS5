#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
long long APS5_VABI atoll_nid_postfix(const char*);
}

int main() {
    if (atoll_nid_postfix("  -42tail") != -42) std::abort();
    if (atoll_nid_postfix("+9223372036854775807") != 9223372036854775807LL) std::abort();
    if (atoll_nid_postfix("-9223372036854775808") != (-9223372036854775807LL - 1)) std::abort();
    if (atoll_nid_postfix("not a number") != 0) std::abort();
}
