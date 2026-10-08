#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <thread>

extern "C" {
int APS5_VABI sem_init_nid_postfix(void*, int, unsigned int);
int APS5_VABI sem_destroy_nid_postfix(void*);
int APS5_VABI sem_post_nid_postfix(void*);
int APS5_VABI sem_trywait_nid_postfix(void*);
int APS5_VABI sem_timedwait_nid_postfix(void*, const KernelTimespec*);
int APS5_VABI sem_getvalue_nid_postfix(void*, int*);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void CheckTimeoutChunkBoundaries() {
    constexpr auto maximum = std::numeric_limits<KernelUseconds>::max();
    KernelUseconds usec = maximum;
    Require(PosixThread::RemainingTimeoutChunk({0, 1}, {0, 0}, &usec) && usec == 1, "sub-microsecond deadline must round up");
    Require(PosixThread::RemainingTimeoutChunk({0, 0}, {0, 0}, &usec) && usec == 0, "expired deadline must produce zero");
    Require(PosixThread::RemainingTimeoutChunk({4294, 967295000}, {0, 0}, &usec) && usec == maximum, "exact maximum chunk must be preserved");
    Require(PosixThread::RemainingTimeoutChunk({4294, 999999999}, {0, 0}, &usec) && usec == maximum, "maximum second boundary must clamp");
    Require(PosixThread::RemainingTimeoutChunk({4295, 0}, {0, 0}, &usec) && usec == maximum, "next second must clamp");
    Require(PosixThread::RemainingTimeoutChunk({5000, 0}, {4294, 967295000}, &usec) && usec == 705032705, "deadline after one maximum chunk must leave the exact remainder");
}

int main(int argc, char** argv) {
    CheckTimeoutChunkBoundaries();
    constexpr unsigned int maximum = 0x7fffffffu;
    std::uintptr_t sem = 0;
    int value = -1;
    if (argc > 1 && std::strcmp(argv[1], "init") == 0) {
        for (const unsigned int initial : {maximum + 1, 0xffffffffu}) {
            sem = 0x1234;
            *__error_nid_postfix() = 0;
            const int result = sem_init_nid_postfix(&sem, 0, initial);
            std::fprintf(stderr, "sem_init(%u): result=%d errno=%d\n", initial, result, *__error_nid_postfix());
            Require(result == -1 && *__error_nid_postfix() == 22, "initial count above maximum must fail with EINVAL");
            Require(sem == 0x1234, "failed initialization changed semaphore storage");
        }
        return 0;
    }
    Require(sem_init_nid_postfix(&sem, 0, maximum) == 0, "maximum count must initialize");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "maximum count must be readable");
    *__error_nid_postfix() = 0;
    const int result = sem_post_nid_postfix(&sem);
    std::fprintf(stderr, "sem_post(maximum): result=%d errno=%d\n", result, *__error_nid_postfix());
    Require(result == -1 && *__error_nid_postfix() == 84, "post at maximum must fail with guest EOVERFLOW");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "overflow changed count");
    Require(sem_trywait_nid_postfix(&sem) == 0, "wait at maximum must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum - 1), "wait must decrement");
    Require(sem_post_nid_postfix(&sem) == 0, "post below maximum must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "post must reach maximum");
    Require(sem_post_nid_postfix(&sem) == -1 && *__error_nid_postfix() == 84, "repeated overflow must fail");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "repeated overflow changed count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "destroy must succeed");
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "zero count must initialize");
    Require(sem_trywait_nid_postfix(&sem) == -1 && *__error_nid_postfix() == 35, "empty semaphore must report EAGAIN");
    Require(sem_post_nid_postfix(&sem) == 0, "normal post must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 1, "normal post must increment");
    Require(sem_trywait_nid_postfix(&sem) == 0, "normal wait must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "normal wait must decrement");

    std::atomic<bool> posted{false};
    std::thread poster([&] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        posted.store(true, std::memory_order_release);
        Require(sem_post_nid_postfix(&sem) == 0, "far-future post must succeed");
    });
    const KernelTimespec farFuture{std::numeric_limits<std::int64_t>::max(), 0};
    int timedWaitResult = -1;
    std::atomic<bool> waitStarted{false};
    std::atomic<bool> waitFinished{false};
    std::thread waiter([&] {
        waitStarted.store(true, std::memory_order_release);
        timedWaitResult = sem_timedwait_nid_postfix(&sem, &farFuture);
        waitFinished.store(true, std::memory_order_release);
    });
    while (!waitStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    Require(!posted.load(std::memory_order_acquire), "poster released before timed wait");
    const auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!waitFinished.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < waitDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(waitFinished.load(std::memory_order_acquire), "far-future wait did not finish");
    waiter.join();
    Require(posted.load(std::memory_order_acquire), "far-future wait returned before post");
    poster.join();
    Require(timedWaitResult == 0, "far-future wait must acquire after post");
    Require(sem_destroy_nid_postfix(&sem) == 0, "normal destroy must succeed");
}
