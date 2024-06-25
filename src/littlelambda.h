#pragma once

#include <cstdint>
#include "mini-gmp.h"
extern "C" {
#include "../ugc/ugc.h"
}

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

/// Types a lam_value can contain.
enum class lam_type {
    /*inline values*/
    Null = 0,  // 0
    Double,    // 1
    Int,       // 2
    Opaque,    // 3
    /*heap objects*/
    BigInt = 10,  // 10
    String,       // 11
    Symbol,       // 12
    List,         // 13
    Applicative,  // 14
    Operative,    // 15
    Environment,  // 16
    Error,        // 17
};

struct lam_env;
struct lam_obj;
struct lam_list;
struct lam_error;
struct lam_symbol;
struct lam_string;
struct lam_callable;
struct lam_bigint;

namespace lam_Detail {
#define Type_Traits(X)                \
    X(lam_bigint, lam_type::BigInt)   \
    X(lam_string, lam_type::String)   \
    X(lam_symbol, lam_type::Symbol)   \
    X(lam_list, lam_type::List)       \
    X(lam_env, lam_type::Environment) \
    X(lam_error, lam_type::Error)

template <typename T>
struct TypeTrait;
#define Declare_Trait(SYM, VAL)                           \
    template <>                                           \
    struct TypeTrait<SYM> {                               \
        constexpr static const lam_type StaticType = VAL; \
    };
Type_Traits(Declare_Trait)
#undef Declare_Trait
#undef Type_Traits

}  // namespace lam_Detail

using lam_u64 = unsigned long long;
using lam_u32 = unsigned long;

// Implementation detail of inlined lam_value
enum lam_Magic : lam_u64 {
    // Anything other than 7ff8.... means the value is a non-nan double
    NormalNan = 0x7ff80000'00000000,  // 1000 - 'Normal' NaN prefix
    TaggedNan = 0x7ffc0000'00000000,  // 11.. - 'Reserved' NaN values

    Mask = 0x7fff0000'00000000,      // 1111
    TagInt = 0x7ffc0000'00000000,    // 1100 + 32 bit signed integer value
    TagObj = 0x7ffd0000'00000000,    // 1101 + 48 bit pointer to lam_obj
    TagConst = 0x7ffe0000'00000000,  // 1110 + lower bits indicate which constant: null, true, false
    TagOpaque = 0x7fff0000'00000000,  // 1111 + lower 48 bits are opaque data

    ValueConstNull = TagConst | 2,
};

/// Base class of all heap allocated objects.
struct lam_obj {
    lam_obj(lam_type t) : type{t} {}
    ugc_header_s header{};
    lam_type type;
};

/// 'Boxed' NaN tagged value.
/// This either contains an double/integer or a pointer to a lam_obj derived object.
union lam_value {
    lam_u64 uval;
    double dval;

    using uint48_t = std::uint64_t;

    constexpr int as_int() const {
        assert((uval & lam_Magic::Mask) == lam_Magic::TagInt);
        return int(unsigned(uval));
    }

    double as_double() const {
        assert((uval & lam_Magic::TaggedNan) != lam_Magic::TaggedNan);
        return dval;
    }

    uint48_t as_opaque() const {
        assert((uval & lam_Magic::Mask) == lam_Magic::TagOpaque);
        return uval & ~0xffff0000'00000000;
    }

    template <typename ReqType>
    static ReqType* obj_cast_value(lam_u64 val) {
        assert((val & lam_Magic::Mask) == lam_Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(val & ~lam_Magic::Mask);
        assert(obj->type == lam_Detail::TypeTrait<ReqType>::StaticType);
        return reinterpret_cast<ReqType*>(obj);
    }

    lam_obj* obj_cast_value() {
        if ((uval & lam_Magic::Mask) == lam_Magic::TagObj) {
            return reinterpret_cast<lam_obj*>(uval & ~lam_Magic::Mask);
        }
        return nullptr;
    }

    lam_symbol* as_symbol() const { return obj_cast_value<lam_symbol>(uval); }

    lam_string* as_string() const { return obj_cast_value<lam_string>(uval); }

    lam_list* as_list() const { return obj_cast_value<lam_list>(uval); }

    lam_bigint* as_bigint() const { return obj_cast_value<lam_bigint>(uval); }

    lam_error* as_error() const { return obj_cast_value<lam_error>(uval); }

    lam_env* as_env() const { return obj_cast_value<lam_env>(uval); }

    lam_callable* as_callable() const {
        assert((uval & lam_Magic::Mask) == lam_Magic::TagObj);
        lam_obj* obj = reinterpret_cast<lam_obj*>(uval & ~lam_Magic::Mask);
        assert(obj->type == lam_type::Applicative || obj->type == lam_type::Operative);
        return reinterpret_cast<lam_callable*>(obj);
    }

    lam_type type() const {
        switch (uval & lam_Magic::Mask) {
            case TagObj: {
                auto o = reinterpret_cast<lam_obj*>(uval & ~lam_Magic::Mask);
                return o->type;
            }
            case TagInt:
                return lam_type::Int;
            case TagConst:
                return lam_type::Null;
            case TagOpaque:
                return lam_type::Opaque;
            default:
                return lam_type::Double;
        }
    }
};

struct lam_value_or_tail_call {
    lam_value_or_tail_call(lam_value v) : value(v), env(nullptr) {}
    lam_value_or_tail_call(lam_value v, lam_env* e) : value(v), env(e) {}
    lam_value value;  // If env is null, 'value' is the result.
    lam_env* env;     // If env is not null the result is 'eval(value, env)'.
};
using lam_invoke = lam_value_or_tail_call(lam_callable* callable,
                                          lam_env* env,
                                          lam_value* a,
                                          size_t n);

/// Variable length array
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

/// Function type. Either an applicative (evaluates arguments) or an operative (arguments are not
/// implicilty evaluated)
struct lam_callable : lam_obj {
    const char* name;
    lam_invoke* invoke;
    lam_env* env;
    lam_value body;
    size_t num_args;       // not including variadic
    const char* envsym;    // only for operatives, name to which we bind environment
    const char* variadic;  // if not null, bind extra arguments to this name
    void* context;         // extra data
    // char name[num_args]; // variable length
    char** args() { return reinterpret_cast<char**>(this + 1); }
};

/// A symbol.
struct lam_symbol : lam_obj {
    lam_u64 len;
    const char* val() const { return reinterpret_cast<const char*>(this + 1); }
    // char name[len]; // variable length
};

/// A UTF8 string.
struct lam_string : lam_obj {
    lam_u64 len;
    const char* val() const { return reinterpret_cast<const char*>(this + 1); }
    // char name[len]; char zero{0}; // variable length
};

/// Arbitrary precision integer.
struct lam_bigint : lam_obj {
    mpz_t mp;  // TODO: flatten allocation in to the containing struct
    char* str() { return mpz_get_str(nullptr, 10, mp); }  // TODO FREE
};

/// Error code and optional message.
struct lam_error : lam_obj {
    unsigned code;
    const char* msg;
};

/// Environments map symbols to values.
struct lam_env : lam_obj {
    lam_env();
    void bind_multiple(const char* keys[],
                       size_t nkeys,
                       lam_value* values,
                       size_t nvalues,
                       const char* variadic);

    void seal();
    void bind(const char* name, lam_value value);
    void bind_applicative(const char* name, lam_invoke b);
    void bind_operative(const char* name, lam_invoke b, void* context = nullptr);
    lam_value lookup(const char* sym) const;
};

// Create values

static inline lam_value lam_make_double(double d) {
    return {.dval = d};
}
static inline lam_value lam_make_int(int i) {
    return {.uval = lam_u32(i) | lam_Magic::TagInt};
}
static inline lam_value lam_make_opaque(unsigned long long u) {
    assert(u <= 0x0000ffff'ffffffff);
    return {.uval = u | lam_Magic::TagOpaque};
}
static inline lam_value lam_make_opaque(const void* p) {
    return lam_make_opaque((unsigned long long)p);
}
static inline lam_value lam_make_null() {
    return {.uval = lam_Magic::ValueConstNull};
}
lam_value lam_make_symbol(const char* s, size_t n = size_t(-1));
lam_value lam_make_string(const char* s, size_t n = size_t(-1));
lam_value lam_make_bigint(int i);
lam_value lam_make_error(unsigned code, const char* msg);

template <typename... Args>
static inline lam_value lam_make_list_l(Args... args) {
    lam_value values[] = {args...};
    return lam_make_list_v(values, sizeof...(Args));
}

lam_value lam_make_list_v(const lam_value* values, size_t N);

lam_value lam_make_env(lam_env* parent, const char* name);

// If code==0, 'value' is valid, otherwise 'msg'. TODO union?
struct lam_result {
    static lam_result ok(lam_value v) { return {0, v, nullptr}; }
    static lam_result fail(unsigned code, const char* msg) { return {code, {}, msg}; }
    unsigned code;
    lam_value value;
    const char* msg;
};

/// Functionality provided by external systems.
struct lam_hooks {
    using import_func = lam_result(lam_hooks* hooks, const char*);
    import_func* import;
};

lam_env* lam_make_env_builtin(lam_hooks* hooks = nullptr);
static inline lam_value lam_make_value(lam_obj* obj) {
    return {.uval = lam_u64(obj) | lam_Magic::TagObj};
}

/// Evaluate the given value in the given environment.
lam_value lam_eval(lam_value val, lam_env* env);

/// Parse and return a single possibly-compound value from the given input.
lam_result lam_parse(const char* input);

/// Parse and return a single possibly-compound value from the given input.
/// Sets the 'restart' pointer to the end of the input consumed.
lam_result lam_parse(const char* input, const char** restart);

/// Print the given value.
void lam_print(lam_value val, const char* end = nullptr);

/// Initialize.
void lam_init();
