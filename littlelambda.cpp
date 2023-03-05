#include "littlelambda.h"
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(disable : 6011)

static bool is_white(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}
static bool is_word_boundary(char c) {
    return is_white(c) || c == '(' || c == ')' || c == '\0';
}

// Allocate and zero "sizeof(T) + extra" bytes
template <typename T>
static T* callocPlus(size_t extra) {
    void* p = calloc(1, sizeof(T) + extra);
    return reinterpret_cast<T*>(p);
}

// Parse null terminated input
lam_value lam_parse(const char* input) {
    std::vector<std::vector<lam_value>> stack;
    std::vector<lam_value>* curList{};
    for (const char* cur = input; *cur;) {
        const char* start = cur;
        switch (*cur++) {
            case ' ':
            case '\t':
            case '\r':
            case '\n': {
                while (is_white(*cur)) {
                    ++cur;
                }
                break;
            }

            case '(': {
                stack.emplace_back();
                curList = &stack.back();
                break;
            }
            case ')': {
                auto l = lam_make_list_v(curList->data(), curList->size());
                stack.pop_back();
                if (stack.empty()) {
                    while (is_white(*cur)) {
                        ++cur;
                    }
                    assert(*cur == '\0');
                    return l;
                }
                curList = &stack.back();
                curList->push_back(l);
                break;
            }

            default: {
                while (!is_word_boundary(*cur)) {
                    ++cur;
                }
                const char* end = cur;
                lam_value val{};
                do {
                    char* endparse;
                    long asInt = strtol(start, &endparse, 10);
                    if (endparse == end) {
                        val = lam_make_int(asInt);
                        break;
                    }
                    double asDbl = strtold(start, &endparse);
                    if (endparse == end) {
                        val = lam_make_double(asDbl);
                        break;
                    }
                    val = lam_make_sym(start, end - start);
                } while (false);

                if (curList) {
                    curList->push_back(val);
                } else {
                    return val;
                }
                break;
            }
        }
    }
    return {};
}

lam_value lam_make_sym(const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = callocPlus<lam_sym>(len + 1);
    d->type = lam_type::Symbol;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_value lam_make_list_v(const lam_value* values, size_t len) {
    auto* d = callocPlus<lam_list>(len * sizeof(lam_value));
    d->type = lam_type::List;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, values, len * sizeof(lam_value));
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_env::~lam_env() {}

struct lam_env_1 : lam_env {
    ~lam_env_1() override {}

    lam_env_1() {}

    lam_env_1(const char* keys[],
              size_t nkeys,
              lam_value* values,
              size_t nvalues,
              lam_env* parent,
              const char* variadic)
        : _parent(parent) {
        assert((nvalues == nkeys) || (variadic && nvalues >= nkeys));
        for (size_t i = 0; i < nkeys; ++i) {
            add_value(keys[i], values[i]);
        }
        if (variadic) {
            lam_value kw = lam_make_list_v(values + nkeys, nvalues - nkeys);
            add_value(keys[nkeys - 1], kw);
        }
    }

    void add_value(const char* name, lam_value val) { _map.emplace(name, val); }
    void add_applicative(const char* name, lam_invoke b) {
        auto* d = callocPlus<lam_callable>(0);
        d->type = lam_type::Applicative;
        d->name = name;
        d->invoke = b;
        d->env = nullptr;
        d->body = {};
        d->num_args = 0;
        lam_value v{.uval = lam_u64(d) | Magic::TagObj};
        _map.emplace(name, v);
    }
    void add_operative(const char* name, lam_invoke b) {
        auto* d = callocPlus<lam_callable>(0);
        d->type = lam_type::Operative;
        d->name = name;
        d->invoke = b;
        d->env = nullptr;
        d->body = {};
        d->num_args = 0;
        lam_value v{.uval = lam_u64(d) | Magic::TagObj};
        _map.emplace(name, v);
    }

    lam_value lookup(const char* sym) override {
        auto it = _map.find(sym);
        if (it != _map.end()) {
            return it->second;
        }
        if (_parent) {
            return _parent->lookup(sym);
        }
        assert(0 && "symbol not found");
        return {};
    }

    void insert(const char* sym, lam_value value) override { _map.emplace(sym, value); }

    static lam_type coerce_numeric_types(lam_value& a, lam_value& b) {
        auto at = a.type();
        auto bt = b.type();
        if ((at != lam_type::Double && at != lam_type::Int) ||
            (bt != lam_type::Double && bt != lam_type::Int)) {
            assert(false);
        }
        if (at == bt) {
            return at;
        }
        if (at == lam_type::Double) {
            b = lam_make_double(b.as_int());
        } else {
            a = lam_make_double(a.as_int());
        }
        return lam_type::Double;
    }

   protected:
    std::unordered_map<std::string, lam_value> _map;
    lam_env* _parent{nullptr};
};
static lam_value lam_invokelambda(lam_callable* call, lam_env* env, lam_value* args, auto narg) {
    lam_env_1* inner = inner = new lam_env_1((const char**)call->args(), call->num_args, args, narg,
                                             call->env, call->variadic);
    return lam_eval(call->body, inner);
}

static bool truthy(lam_value v) {
    if ((v.uval & Magic::Mask) == Magic::TagInt) {
        return unsigned(v.uval) != 0;
    } else if ((v.uval & Magic::Mask) == Magic::TagObj) {
        lam_obj* obj = reinterpret_cast<lam_obj*>(v.uval & ~Magic::Mask);
        if (obj->type == lam_type::List) {
            lam_list* list = reinterpret_cast<lam_list*>(obj);
            return list->len != 0;
        }
    }
    assert(false);
    return false;
}

static void lam_print(lam_value val) {
    switch (val.type()) {
        case lam_type::Double:
            printf("%lf", val.as_double());
            break;

        case lam_type::Int:
            printf("%li", val.as_int());
            break;
        case lam_type::Symbol:
            printf(":%s", val.as_sym()->val());
            break;
        case lam_type::List: {
            printf("(");
            auto lst = val.as_list();
            const char* sep = "";
            for (auto i = 0; i < lst->len; ++i) {
                printf(sep);
                sep = " ";
                lam_print(lst->at(i));
            }
            printf(")");
            break;
        }
        case lam_type::Applicative:
            printf("A{%s}", val.as_func()->name);
            break;
        case lam_type::Operative:
            printf("O{%s}", val.as_func()->name);
            break;
    }
}

lam_env* lam_make_env_builtin() {
    lam_env_1* ret = new lam_env_1();
    ret->add_operative("define", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        auto lhs = a[0];
        auto rhs = a[1];
        if (lhs.type() == lam_type::Symbol) {
            auto sym = reinterpret_cast<lam_sym*>(lhs.uval & ~Magic::Mask);
            auto r = lam_eval(rhs, env);
            env->insert(sym->val(), r);
        } else if (lhs.type() == lam_type::List) {
            auto argsList = reinterpret_cast<lam_list*>(lhs.uval & ~Magic::Mask);
            std::span<lam_value> args{argsList->first() + 1,
                                      argsList->len - 1};  // drop sym from args list
            const char* variadic{nullptr};

            if (args.size() >= 2 && args[args.size() - 2].as_sym()->val()[0] == '.') {
                variadic = args[args.size() - 1].as_sym()->val();
                args = args.subspan(0, args.size() - 2);
            }
            auto func = callocPlus<lam_callable>(args.size() * sizeof(char*));  // TODO intern names
            const char* name = argsList->at(0).as_sym()->val();
            func->type = lam_type::Applicative;
            func->invoke = &lam_invokelambda;
            func->name = name;
            func->env = env;
            func->body = rhs;
            func->num_args = args.size();
            func->variadic = variadic;
            char** names = func->args();
            for (size_t i = 0; i < args.size(); ++i) {
                names[i] = const_cast<char*>(args[i].as_sym()->val());
            }
            lam_value y = {.uval = lam_u64(func) | Magic::TagObj};
            env->insert(name, y);
        } else {
            assert(false);
        }
        return lam_value{};
    });

    ret->add_operative("lambda", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2 && "wrong number of parameters to lambda");
        auto lhs = a[0];
        auto rhs = a[1];
        size_t numArgs{0};
        lam_value* argp{nullptr};
        const char* variadic{nullptr};
        if (lhs.type() == lam_type::List) {
            auto args = lhs.as_list();
            numArgs = args->len;
            argp = args->first();
        } else if (lhs.type() == lam_type::Symbol) {
            variadic = lhs.as_sym()->val();
        } else {
            assert(false && "expected list or symbol");
        }
        auto func = callocPlus<lam_callable>(numArgs * sizeof(char*));  // TODO intern names
        func->type = lam_type::Applicative;
        func->invoke = &lam_invokelambda;
        func->name = "lambda";
        func->env = env;
        func->body = rhs;
        func->num_args = numArgs;
        func->variadic = variadic;
        char** names = func->args();
        for (int i = 0; i < numArgs; ++i) {
            auto sym = argp[i].as_sym();
            names[i] = const_cast<char*>(sym->val());
        }
        lam_value y = {.uval = lam_u64(func) | Magic::TagObj};
        return y;
    });

    ret->add_operative("if", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n >= 2);
        assert(n <= 3);
        auto cond = lam_eval(a[0], env);
        if (truthy(cond)) {
            return lam_eval(a[1], env);
        } else if (n == 3) {
            return lam_eval(a[2], env);
        } else {
            assert(false);
            return lam_make_double(0);
        }
    });
    ret->add_operative("quote", [](lam_callable* call, lam_env* env, auto a, auto n) {
        return lam_make_list_v(a, n);
    });
    ret->add_applicative("eval", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 1);
        return lam_eval(a[0], env);
    });
    ret->add_applicative("begin", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n >= 1);
        return a[n - 1];
    });

    ret->add_applicative("print", [](lam_callable* call, lam_env* env, auto a, auto n) {
        for (auto i = 0; i < n; ++i) {
            lam_print(a[i]);
        }
        return lam_value{};
    });

    ret->add_applicative("list", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n >= 1);
        return lam_make_list_v(a, n);
    });

    ret->add_applicative("equal?", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        auto l = a[0];
        auto r = a[1];
        if ((l.uval & Magic::Mask) != (r.uval & Magic::Mask)) {
            return lam_make_int(false);
        } else if ((l.uval & Magic::Mask) == Magic::TagInt) {
            return lam_make_int(unsigned(l.uval) == unsigned(r.uval));
        }
        // TODO other types
        assert(false);
        return lam_value{};
    });

    ret->add_applicative("mapreduce", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 3);
        lam_callable* map = a[0].as_func();
        lam_callable* red = a[1].as_func();
        lam_list* lst = a[2].as_list();
        assert(lst->len >= 1);
        lam_value acc[]{map->invoke(map, env, lst->first(), 1), {}};
        for (size_t i = 1; i < lst->len; ++i) {
            acc[1] = map->invoke(map, env, lst->first() + i, 1);
            acc[0] = red->invoke(red, env, acc, 2);
        }
        return acc[0];
    });

    ret->add_applicative("*", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_make_double(a[0].dval * a[1].dval);
            case lam_type::Int:
                return lam_make_int(a[0].as_int() * a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("+", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_make_double(a[0].dval + a[1].dval);
            case lam_type::Int:
                return lam_make_int(a[0].as_int() + a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("-", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_make_double(a[0].dval - a[1].dval);
            case lam_type::Int:
                return lam_make_int(a[0].as_int() - a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("/", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_make_double(a[0].dval / a[1].dval);
    });
    ret->add_applicative("<=", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_make_int(a[0].dval <= a[1].dval);
            case lam_type::Int:
                return lam_make_int(a[0].as_int() <= a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_value("pi", lam_make_double(3.14159));
    return ret;
}

static lam_value lam_eval_obj(lam_obj* obj, lam_env* env) {
    switch (obj->type) {
        case lam_type::Symbol: {
            auto sym = static_cast<lam_sym*>(obj);
            return env->lookup(sym->val());
        }
        case lam_type::List: {
            auto list = static_cast<lam_list*>(obj);
            assert(list->len);
            lam_value head = lam_eval(list->at(0), env);
            lam_callable* callable = reinterpret_cast<lam_callable*>(head.uval & ~Magic::Mask);

            if (callable->type == lam_type::Operative) {
                return callable->invoke(callable, env, list->first() + 1, list->len - 1);
            } else if (callable->type == lam_type::Applicative) {
                std::vector<lam_value> args;
                args.resize(list->len - 1);
                for (unsigned i = 1; i < list->len; ++i) {
                    args[i - 1] = lam_eval(list->at(i), env);
                }
                return callable->invoke(callable, env, args.data(), args.size());
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

lam_value lam_eval(lam_value val, lam_env* env) {
    switch (val.uval & Magic::Mask) {
        case Magic::TagObj:
            return lam_eval_obj(reinterpret_cast<lam_obj*>(val.uval & ~Magic::Mask), env);
        case Magic::TagInt:
            return val;
        default:
            return val;
    }
}