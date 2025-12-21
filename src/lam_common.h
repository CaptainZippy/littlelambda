#pragma once
// Shared between the public & internal interfaces.

#include <cstddef>
using std::size_t;

#if defined(_MSC_VER)
extern void __debugbreak();  // Compiler Intrinsic
#define lam_debugbreak() __debugbreak()
#elif defined(__clang__)
#define lam_debugbreak() __builtin_debugtrap()
#elif defined(__GNUC__)
#define lam_debugbreak() __builtin_trap()
#else
#error Fixme
#endif

#define assert(COND)      \
    if (!(COND)) {        \
        lam_debugbreak(); \
    }
#define assert2(COND, MSG) \
    if (!(COND)) {         \
        lam_debugbreak();  \
    }

struct lila_vm;

enum class lila_result : int {
    Ok = 0,
    Fail = -1,
    FileNotFound = -2,
};

/// Functionality provided by external systems.
struct lila_hooks {
    virtual ~lila_hooks();
    virtual void* mem_alloc(size_t size) = 0;
    virtual void mem_free(void* addr) = 0;
    virtual void init() = 0;
    virtual void quit() = 0;
    virtual void output(const char* s, size_t n) = 0;
    virtual lila_result import(lila_vm* vm, const char* modname) = 0;
};
