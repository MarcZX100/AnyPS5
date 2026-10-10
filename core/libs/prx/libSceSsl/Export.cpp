#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80435001);
static std::atomic<int> g_nextHandle{1};
static std::mutex g_poolsMutex;
static std::map<int, uint64_t> g_pools;

namespace {

constexpr int ERROR_NOT_FOUND = static_cast<int>(0x8095F004);
constexpr int ERROR_INVALID_ARG = static_cast<int>(0x8095177A);

struct SslData {
    char* ptr;
    size_t size;
};

struct SslMemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

struct SslCaCerts {
    SslData* certs;
    size_t num;
    void* pool;
};

struct SslCaList {
    void** certs;
    int num;
};
static_assert(sizeof(SslCaList) == 16);

void RequireContext(const char* function, int sslCtxId) {
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(sslCtxId)) throw std::invalid_argument(std::string(function) + ": unknown context");
}

}

extern "C" {

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return 0;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslGetCaList(int sslCtxId, SslCaList* caList) {
    if (!caList) return ERROR_INVALID_ARG;
    RequireContext(__func__, sslCtxId);
    *caList = {};
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslFreeCaList(int sslCtxId, SslCaList* caList) {
    if (!caList) return ERROR_INVALID_ARG;
    RequireContext(__func__, sslCtxId);
    if (caList->certs || caList->num) throw std::invalid_argument("sceSslFreeCaList: list not returned by sceSslGetCaList");
    return 0;
}

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_poolsMutex);
    g_pools[id] = pool_size;
    return id;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    std::lock_guard lock(g_poolsMutex);
    g_pools.erase(ssl_ctx_id);
    return 0;
}

int APS5_VABI sceSslClose(int sslConnectionId) {
    (void)sslConnectionId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslGetSerialNumber(int sslCtxId, void* sslCert, uint8_t* sboData, size_t* sboLen) {
    (void)sslCtxId;
    (void)sslCert;
    (void)sboData;
    (void)sboLen;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslLoadCert(int sslCtxId, int caCertNum, SslData** caList, SslData* cert, SslData* privKey) {
    (void)cert;
    (void)privKey;
    if (caCertNum < 0 || (caCertNum > 0 && caList == nullptr)) return ERROR_INVALID_ARG;
    RequireContext(__func__, sslCtxId);
    return 0;
}

int APS5_VABI sceSslUnloadCert(int sslCtxId) {
    RequireContext(__func__, sslCtxId);
    return 0;
}

int APS5_VABI sceSslGetMemoryPoolStats(int ssl_ctx_id, SslMemoryPoolStats* stats) {
    if (stats == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(g_poolsMutex);
    const auto pool = g_pools.find(ssl_ctx_id);
    if (pool == g_pools.end()) throw std::invalid_argument("sceSslGetMemoryPoolStats: unknown context");
    *stats = {static_cast<size_t>(pool->second), 0, 0, 0};
    return 0;
}

int APS5_VABI sceSslFreeSslCertName(int sslCtxId, void* certName) {
    (void)sslCtxId;
    (void)certName;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI sceSslGetIssuerName(int sslCtxId, void* sslCert) {
    (void)sslCtxId;
    (void)sslCert;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

int APS5_VABI sceSslGetNameEntryCount(int sslCtxId, void* certName) {
    (void)sslCtxId;
    (void)certName;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslGetNameEntryInfo(int sslCtxId, void* certName, int entryNum, char* oidname, size_t maxOidnameLen, uint8_t* value, size_t maxValueLen, size_t* valueLen) {
    (void)sslCtxId;
    (void)certName;
    (void)entryNum;
    (void)oidname;
    (void)maxOidnameLen;
    (void)value;
    (void)maxValueLen;
    (void)valueLen;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslGetPem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI sceSslGetSubjectName(int sslCtxId, void* sslCert) {
    (void)sslCtxId;
    (void)sslCert;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

}
