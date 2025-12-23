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

struct lam_vm;

enum class lam_code : int {
    Ok = 0,
    Fail = -1,
    FileNotFound = -2,
};

/// Functionality provided by external systems.
struct lam_hooks {
    virtual ~lam_hooks();
    virtual void* mem_alloc(size_t size) = 0;
    virtual void mem_free(void* addr) = 0;
    virtual void init() = 0;
    virtual void quit() = 0;
    virtual void output(const char* s, size_t n) = 0;
    virtual lam_code import(lam_vm* vm, const char* modname) = 0;
};
