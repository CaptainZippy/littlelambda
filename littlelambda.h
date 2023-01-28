#pragma once

// #include "../littlegc/littlegc.h"
#include <cassert>

enum class lam_type { Double, Int, Symbol, List, Lambda, Builtin, Special };

struct lam_env;
struct lam_obj;
struct lam_sym;

using lam_u64 = unsigned long long;
using lam_u32 = unsigned long;

enum Magic : lam_u64 {
    Mask = 0x7fff0000'00000000,    // 1111
    TagObj = 0x7fff0000'00000000,  // 1111
    TagInt = 0x7ffd0000'00000000,  // 1101
    Prefix = 0x7ffc0000'00000000,  // 11.. NaN=1000, prefix means special NaN
};

struct lam_obj {
    // lgc_object_t gcobj{};
    lam_type type;
};

// NaN tagged value
union lam_value {
    lam_u64 uval;
    double dval;

    constexpr int as_int() const {
        assert((uval & Magic::Mask) == TagInt);
        return int(unsigned(uval));
    }

    double as_double() const {
        assert((uval & Magic::Prefix) != Magic::Prefix);
        return int(unsigned(uval));
    }

    lam_sym* as_sym() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::Symbol);
        return reinterpret_cast<lam_sym*>(obj);
    }

    lam_type type() const {
        switch (uval & Magic::Mask) {
            case TagObj: {
                auto o = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
                return o->type;
            }
            case TagInt:
                return lam_type::Int;
            default:
                return lam_type::Double;
        }
    }
};

using lam_builtin = lam_value(lam_env* env, lam_value* a, size_t n);

struct lam_list : lam_obj {
    lam_u64 len;
    lam_u64 cap;
    // lam_value values[cap]; // variable length
    lam_value at(size_t i) {
        assert(i < len);
        return reinterpret_cast<lam_value*>(this + 1)[i];
    }
};

struct lam_func : lam_obj {
    const char* name;
    lam_builtin* func;
    lam_env* env;
    lam_value body;
    size_t num_args;
    // char name[num_args]; // variable length
    char** args() { return reinterpret_cast<char**>(this + 1); }
};

struct lam_sym : lam_obj {
    lam_u64 len;
    lam_u64 cap;
    const char* val() const { return reinterpret_cast<const char*>(this + 1); }
    // char name[cap]; // variable length
};

static inline lam_value lam_Double(double d) {
    return {.dval = d};
}
static inline lam_value lam_Int(int i) {
    return {.uval = lam_u32(i) | Magic::TagInt};
}
lam_value lam_Sym(const char* s, size_t n = size_t(-1));

template <typename... Args>
static inline lam_value lam_List(Args... args) {
    lam_value values[] = {args...};
    return lam_ListN(sizeof...(Args), values);
}

lam_value lam_ListN(size_t N, const lam_value* values);

struct lam_env {
    virtual ~lam_env() = 0;
    virtual lam_value lookup(const char* sym) = 0;
    virtual void insert(const char* sym, lam_value) = 0;

    static lam_env* builtin();
};

void lam_init(lam_env* env);
lam_value lam_eval(lam_env* env, lam_value val);
lam_value lam_parse(const char* input);
