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
    d->env = nullptr;
    d->body = {};
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_env::~lam_env() {}

struct lam_env_1 : lam_env {
    ~lam_env_1() override {}

    lam_env_1() {}

    lam_env_1(size_t npairs,
              const char* keys[],
              lam_value* values,
              lam_env* parent)
        : _parent(parent) {
        for (size_t i = 0; i < npairs; ++i) {
            add_value(keys[i], values[i]);
        }
    }

    void add_value(const char* name, lam_value val) { _map.emplace(name, val); }
    void add_func(const char* name, lam_builtin b) {
        _map.emplace(name, lam_Func(name, b));
    }
    void add_special(const char* name, lam_builtin b) {
        auto* d = reinterpret_cast<lam_func*>(malloc(sizeof(lam_func)));
        d->type = lam_type::Special;
        d->name = name;
        d->func = b;
        _map.emplace(name, lam_value{.uval = lam_u64(d) | Magic::TagObj});
    }

    lam_value lookup(const char* sym) override {
        auto it = _map.find(sym);
        if (it != _map.end()) {
            return it->second;
        }
        if (_parent) {
            return _parent->lookup(sym);
        }
        assert(0);
        return {};
    }

    void insert(const char* sym, lam_value value) override {
        _map.emplace(sym, value);
    }

   protected:
    std::unordered_map<std::string, lam_value> _map;
    lam_env* _parent{nullptr};
};

static lam_value call_lambda(lam_env* env, lam_value* args, size_t n) {
    return lam_Double(11);
}

lam_env* lam_env::builtin() {
    lam_env_1* ret = new lam_env_1();
    ret->add_special("define", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Symbol);
        auto sym = reinterpret_cast<lam_sym*>(a[0].uval & ~Magic::Mask);
        auto r = lam_eval(env, a[1]);
        env->insert(sym->val(), r);
        return r;
    });

    ret->add_special("lambda", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::List);
        auto args = reinterpret_cast<lam_list*>(a[0].uval & ~Magic::Mask);
        auto func = (lam_func*)malloc(
            sizeof(lam_func) + args->len * sizeof(char*));  // TODO intern names
        func->type = lam_type::Lambda;
        func->func = call_lambda;
        func->name = "lambda";
        func->env = env;
        func->body = a[1];
        func->num_args = args->len;
        char** names = func->args();
        for (int i = 0; i < args->len; ++i) {
            assert(args->at(i).type() == lam_type::Symbol);
            auto sym =
                reinterpret_cast<lam_sym*>(args->at(i).uval & ~Magic::Mask);
            names[i] = const_cast<char*>(sym->val());
        }
        lam_value y = {.uval = lam_u64(func) | Magic::TagObj};
        return y;
    });

    ret->add_special("if", [](lam_env* env, auto a, auto n) {
        assert(n >= 2);
        assert(n <= 3);
        auto cond = lam_eval(env, a[0]);
        if (cond.as_int() != 0) {
            return lam_eval(env, a[1]);
        } else if (n == 2) {
            return lam_eval(env, a[2]);
        } else {
            return lam_Double(0);
        }
    });
    // add_special("quote",
    ret->add_func("begin", [](lam_env* env, auto a, auto n) {
        assert(n >= 1);
        return a[n - 1];
    });
    ret->add_func("*", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_Double(a[0].dval * a[1].dval);
    });
    ret->add_func("+", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_Double(a[0].dval + a[1].dval);
    });
    ret->add_func("-", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_Double(a[0].dval - a[1].dval);
    });
    ret->add_func("/", [](lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_Double(a[0].dval / a[1].dval);
    });
    ret->add_value("pi", lam_Double(3.14159));
    return ret;
}

lam_env* lam_env_builtin() {
    return lam_env_1::builtin();
}

lam_value lam_call(lam_env* env, lam_builtin func, std::span<lam_value> args) {
    assert(!args.empty());
    return func(env, &args[0], args.size());
}

lam_value lam_eval_obj(lam_env* env, lam_obj* obj) {
    switch (obj->type) {
        case lam_type::Symbol: {
            auto sym = static_cast<lam_sym*>(obj);
            return env->lookup(sym->val());
        }
        case lam_type::List: {
            auto list = static_cast<lam_list*>(obj);
            lam_value head = lam_eval(env, list->at(0));
            lam_func* func =
                reinterpret_cast<lam_func*>(head.uval & ~Magic::Mask);

            if (func->type == lam_type::Special) {
                auto list = static_cast<lam_list*>(obj);
                auto values = reinterpret_cast<lam_value*>(list + 1);
                return lam_call(env, func->func,
                                {values + 1, values + list->len});
            } else if (func->type == lam_type::Builtin) {
                std::vector<lam_value> bits;
                bits.resize(list->len - 1);
                for (unsigned i = 1; i < list->len; ++i) {
                    bits[i - 1] = lam_eval(env, list->at(i));
                }
                return lam_call(env, func->func, bits);
            } else if (func->type == lam_type::Lambda) {
                std::vector<lam_value> bits;
                bits.resize(list->len - 1);
                for (unsigned i = 1; i < list->len; ++i) {
                    bits[i - 1] = lam_eval(env, list->at(i));
                }
                assert(bits.size() == func->num_args);
                lam_env_1 inner(func->num_args, (const char**)func->args(),
                                bits.data(), env);
                return lam_eval(&inner, func->body);
            } else {
                assert(0);
                return {};
            }
        }
        default: {
            assert(0);
            return {};
        }
    }
}

lam_value lam_eval(lam_env* env, lam_value val) {
    switch (val.uval & Magic::Mask) {
        case Magic::TagObj:
            return lam_eval_obj(
                env, reinterpret_cast<lam_obj*>(val.uval & ~Magic::Mask));
        case Magic::TagInt:
            return val;
        default:
            return val;
    }
}