#include "littlelambda.h"
#include <cassert>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

lam_value lam_Sym(const char* s) {
    lam_u64 len = strlen(s);
    auto* d = reinterpret_cast<lam_sym*>(malloc(sizeof(lam_sym) + len + 1));
    d->type = lam_type::Symbol;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, s, len + 1);
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_value lam_ListN(size_t len, const lam_value* values) {
    auto* d = reinterpret_cast<lam_list*>(
        malloc(sizeof(lam_list) + len * sizeof(lam_value)));
    d->type = lam_type::List;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, values, len * sizeof(lam_value));
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_value lam_Func(const char* name, lam_builtin b) {
    auto* d = reinterpret_cast<lam_func*>(malloc(sizeof(lam_func)));
    d->type = lam_type::Builtin;
    d->name = name;
    d->func = b;
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_env_t::~lam_env_t() {}

struct lam_env_1 : lam_env_t {
    ~lam_env_1() override {}
    lam_env_1() {
        add_special("define", [](auto vm, auto a, auto n) {
            assert(n == 2);
            assert(a[0].type() == lam_type::Symbol);
            auto env = static_cast<lam_env_1*>(vm->env);
            auto r = lam_eval(vm, a[1]);
            auto sym = reinterpret_cast<lam_sym*>(a[0].uval & ~Magic::Mask);
            env->map.emplace(sym->val(), r);
            return r;
        });

        add_special("if", [](auto vm, auto a, auto n) {
            assert(n >= 2);
            assert(n <= 3);
            auto cond = lam_eval(vm, a[0]);
            if (cond.as_int() != 0) {
                return lam_eval(vm, a[1]);
            } else if (n == 2) {
                return lam_eval(vm, a[2]);
            } else {
                return lam_Double(0);
            }
        });
        // add_special("quote",
        add_func("begin", [](auto vm, auto a, auto n) {
            assert(n >= 1);
            return a[n - 1];
        });
        add_func("*", [](auto vm, auto a, auto n) {
            assert(n == 2);
            // assert(a[0].type == lam_type::Double);
            // assert(a[1].type == lam_type::Double);
            return lam_Double(a[0].dval * a[1].dval);
        });
        map.emplace("pi", lam_Double(3.14159));
    }
    void add_func(const char* name, lam_builtin b) {
        map.emplace(name, lam_Func(name, b));
    }
    void add_special(const char* name, lam_builtin b) {
        auto* d = reinterpret_cast<lam_func*>(malloc(sizeof(lam_func)));
        d->type = lam_type::Special;
        d->name = name;
        d->func = b;
        map.emplace(name, lam_value{.uval = lam_u64(d) | Magic::TagObj});
    }

    lam_value lookup(const char* sym) override { return map.at(sym); }
    std::unordered_map<std::string, lam_value> map;
};

void lam_init(lam_vm_t* vm) {
    vm->env = new lam_env_1();
}

lam_value lam_call(lam_vm_t* vm, lam_builtin func, std::span<lam_value> args) {
    assert(!args.empty());
    return func(vm, &args[0], args.size());
}

lam_value lam_eval_obj(lam_vm_t* vm, lam_obj* obj) {
    switch (obj->type) {
        case lam_type::Symbol: {
            auto sym = static_cast<lam_sym*>(obj);
            return vm->env->lookup(sym->val());
        }
        case lam_type::List: {
            auto list = static_cast<lam_list*>(obj);
            lam_value head = lam_eval(vm, list->at(0));
            lam_func* func =
                reinterpret_cast<lam_func*>(head.uval & ~Magic::Mask);

            if (func->type == lam_type::Special) {
                auto list = static_cast<lam_list*>(obj);
                auto values = reinterpret_cast<lam_value*>(list + 1);
                return lam_call(vm, func->func,
                                {values + 1, values + list->len});
            } else if (func->type == lam_type::Builtin) {
                std::vector<lam_value> bits;
                bits.resize(list->len - 1);
                for (unsigned i = 1; i < list->len; ++i) {
                    bits[i-1] = lam_eval(vm, list->at(i));
                }
                return lam_call(vm, func->func, bits);
            }
        }
        default: {
            assert(0);
            return {};
        }
    }
}

lam_value lam_eval(lam_vm_t* vm, lam_value val) {
    switch (val.uval & Magic::Mask) {
        case Magic::TagObj:
            return lam_eval_obj(
                vm, reinterpret_cast<lam_obj*>(val.uval & ~Magic::Mask));
        case Magic::TagInt:
            return val;
        default:
            return val;
    }
}