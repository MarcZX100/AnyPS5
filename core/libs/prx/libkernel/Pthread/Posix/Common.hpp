#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP

#include <cstdint>
#include <limits>
#include "SceTypes.hpp"

extern "C" int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);

namespace PosixThread {

constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_ETIMEDOUT = 60;

inline int ToErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

inline bool RemainingTimeoutChunk(const KernelTimespec& abstime, const KernelTimespec& now, KernelUseconds* usec) {
    if (!usec || abstime.tv_nsec < 0 || abstime.tv_nsec >= 1000000000 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000) return false;
    if (abstime.tv_sec < now.tv_sec || (abstime.tv_sec == now.tv_sec && abstime.tv_nsec <= now.tv_nsec)) {
        *usec = 0;
        return true;
    }

    std::uint64_t seconds = static_cast<std::uint64_t>(abstime.tv_sec) - static_cast<std::uint64_t>(now.tv_sec);
    std::int64_t nanoseconds = abstime.tv_nsec - now.tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000;
    }

    constexpr std::uint64_t MicrosecondsPerSecond = 1000000;
    constexpr auto MaximumUseconds = std::numeric_limits<KernelUseconds>::max();
    if (seconds > MaximumUseconds / MicrosecondsPerSecond) {
        *usec = MaximumUseconds;
        return true;
    }
    const std::uint64_t microseconds = seconds * MicrosecondsPerSecond + static_cast<std::uint64_t>((nanoseconds + 999) / 1000);
    *usec = microseconds > MaximumUseconds ? MaximumUseconds : static_cast<KernelUseconds>(microseconds);
    return true;
}

inline bool RemainingTimeoutChunk(int clockId, const KernelTimespec* abstime, KernelUseconds* usec) {
    if (!abstime || !usec || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return false;
    KernelTimespec now{};
    if (clock_gettime_nid_postfix(clockId, &now) != 0) return false;
    return RemainingTimeoutChunk(*abstime, now, usec);
}

}

#endif
