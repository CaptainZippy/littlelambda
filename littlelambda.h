#pragma once

// #include "../littlegc/littlegc.h"
// #include <cassert>
#define assert(COND)    \
    if (!(COND)) {      \
        __debugbreak(); \
    }

enum class lam_type {
    /*inline values*/
    Double,
    Int,
    /*heap objects*/
    String,
    Symbol,
    List,
    Applicative,
    Operative,
    Environment,
};

struct lam_env;
struct lam_obj;
struct lam_symbol;
struct lam_list;
struct lam_string;
struct lam_callable;

using lam_u64 = unsigned long long;
using lam_u32 = unsigned long;

enum Magic : lam_u64 {
    // Anything other than 7ff8.... means the value is a non-nan double
    NormalNan = 0x7ff80000'00000000,  // 1000 - 'Normal' NaN prefix
    TaggedNan = 0x7ffc0000'00000000,  // 11.. - 'Reserved' NaN values

    Mask = 0x7fff0000'00000000,    // 1111
    TagObj = 0x7ffd0000'00000000,  // 1100 + 48 bit pointer to lam_obj
    TagInt = 0x7ffc0000'00000000,  // 1101 + 32 bit value
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
        assert((uval & Magic::TaggedNan) != Magic::TaggedNan);
        return dval;
    }

    lam_symbol* as_symbol() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::Symbol);
        return reinterpret_cast<lam_symbol*>(obj);
    }

    lam_string* as_string() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::String);
        return reinterpret_cast<lam_string*>(obj);
    }

    lam_callable* as_func() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::Applicative || obj->type == lam_type::Operative);
        return reinterpret_cast<lam_callable*>(obj);
    }

    lam_env* as_env() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::Environment);
        return reinterpret_cast<lam_env*>(obj);
    }

    lam_list* as_list() const {
        assert((uval & Magic::Mask) == Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~Magic::Mask);
        assert(obj->type == lam_type::List);
        return reinterpret_cast<lam_list*>(obj);
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

using lam_invoke = lam_value(lam_callable* callable, lam_env* env, lam_value* a, size_t n);

struct lam_list : lam_obj {
    lam_u64 len;
    lam_u64 cap;
    // lam_value values[cap]; // variable length
    lam_value* first() { return reinterpret_cast<lam_value*>(this + 1); }
    lam_value at(size_t i) {
        assert(i < len);
        return reinterpret_cast<lam_value*>(this + 1)[i];
    }
};

struct lam_callable : lam_obj {
    const char* name;
    lam_invoke* invoke;
    lam_env* env;
    lam_value body;
    size_t num_args;       // not including variadic
    const char* variadic;  // if not null, bind extra arguments to this
    // char name[num_args]; // variable length
    char** args() { return reinterpret_cast<char**>(this + 1); }
};

struct lam_symbol : lam_obj {
    lam_u64 len;
    lam_u64 cap;
    const char* val() const { return reinterpret_cast<const char*>(this + 1); }
    // char name[cap]; // variable length
};

struct lam_string : lam_obj {
    lam_u64 len;
    const char* val() const { return reinterpret_cast<const char*>(this + 1); }
    // char name[len]; char zero{0}; // variable length
};

// Create values

static inline lam_value lam_make_double(double d) {
    return {.dval = d};
}
static inline lam_value lam_make_int(int i) {
    return {.uval = lam_u32(i) | Magic::TagInt};
}
lam_value lam_make_symbol(const char* s, size_t n = size_t(-1));
lam_value lam_make_string(const char* s, size_t n = size_t(-1));

template <typename... Args>
static inline lam_value lam_make_list_l(Args... args) {
    lam_value values[] = {args...};
    return lam_make_list_v(values, sizeof...(Args));
}

lam_value lam_make_list_v(const lam_value* values, size_t N);

lam_value lam_make_env(lam_env* parent);

lam_env* lam_make_env_builtin();
static inline lam_value lam_make_value(lam_obj* obj) {
    return {.uval = lam_u64(obj) | Magic::TagObj};
}

//

lam_value lam_eval(lam_value val, lam_env* env);
lam_value lam_parse(const char* input);
