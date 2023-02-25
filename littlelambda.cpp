#include "littlelambda.h"
#include <cassert>
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
                auto l = lam_ListN(curList->size(), curList->data());
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
                        val = lam_Int(asInt);
                        break;
                    }
                    double asDbl = strtold(start, &endparse);
                    if (endparse == end) {
                        val = lam_Double(asDbl);
                        break;
                    }
                    val = lam_Sym(start, end - start);
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

lam_value lam_Sym(const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = reinterpret_cast<lam_sym*>(malloc(sizeof(lam_sym) + len + 1));
    d->type = lam_type::Symbol;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | Magic::TagObj};
}

lam_value lam_ListN(size_t len, const lam_value* values) {
    auto* d = reinterpret_cast<lam_list*>(malloc(sizeof(lam_list) + len * sizeof(lam_value)));
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

    lam_env_1(size_t npairs, const char* keys[], lam_value* values, lam_env* parent)
        : _parent(parent) {
        for (size_t i = 0; i < npairs; ++i) {
            add_value(keys[i], values[i]);
        }
    }

    void add_value(const char* name, lam_value val) { _map.emplace(name, val); }
    void add_applicative(const char* name, lam_invoke b) {
        auto* d = reinterpret_cast<lam_callable*>(malloc(sizeof(lam_callable)));
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
        auto* d = reinterpret_cast<lam_callable*>(malloc(sizeof(lam_callable)));
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
            b = lam_Double(b.as_int());
        } else {
            a = lam_Double(a.as_int());
        }
        return lam_type::Double;
    }

   protected:
    std::unordered_map<std::string, lam_value> _map;
    lam_env* _parent{nullptr};
};
static lam_value lam_invokelambda(lam_callable* call, lam_env* env, lam_value* args, auto narg) {
    assert(call->num_args == narg);
    lam_env_1* inner = inner =
        new lam_env_1(call->num_args, (const char**)call->args(), args, call->env);
    return lam_eval(inner, call->body);
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
            for (auto i = 0; i < lst->len; ++i) {
                lam_print(lst->at(i));
                printf(" ");
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

lam_env* lam_env::builtin() {
    lam_env_1* ret = new lam_env_1();
    ret->add_operative("define", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        auto lhs = a[0];
        auto rhs = a[1];
        if (lhs.type() == lam_type::Symbol) {
            auto sym = reinterpret_cast<lam_sym*>(lhs.uval & ~Magic::Mask);
            auto r = lam_eval(env, rhs);
            env->insert(sym->val(), r);
        } else if (lhs.type() == lam_type::List) {
            auto args = reinterpret_cast<lam_list*>(lhs.uval & ~Magic::Mask);
            auto func = (lam_callable*)malloc(
                sizeof(lam_callable) + (args->len - 1) * sizeof(char*));  // TODO intern names
            const char* name = args->at(0).as_sym()->val();
            func->type = lam_type::Applicative;
            func->invoke = &lam_invokelambda;
            func->name = name;
            func->env = env;
            func->body = rhs;
            func->num_args = args->len - 1;
            char** names = func->args();
            for (int i = 1; i < args->len; ++i) {
                assert(args->at(i).type() == lam_type::Symbol);
                auto sym = reinterpret_cast<lam_sym*>(args->at(i).uval & ~Magic::Mask);
                names[i - 1] = const_cast<char*>(sym->val());
            }
            lam_value y = {.uval = lam_u64(func) | Magic::TagObj};
            env->insert(name, y);
        } else {
            assert(false);
        }
        return lam_value{};
    });

    ret->add_operative("lambda", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        auto lhs = a[0];
        auto rhs = a[1];
        assert(lhs.type() == lam_type::List);
        auto args = reinterpret_cast<lam_list*>(lhs.uval & ~Magic::Mask);
        auto func = (lam_callable*)malloc(sizeof(lam_callable) +
                                          args->len * sizeof(char*));  // TODO intern names
        func->type = lam_type::Applicative;
        func->invoke = &lam_invokelambda;
        func->name = "lambda";
        func->env = env;
        func->body = rhs;
        func->num_args = args->len;
        char** names = func->args();
        for (int i = 0; i < args->len; ++i) {
            assert(args->at(i).type() == lam_type::Symbol);
            auto sym = reinterpret_cast<lam_sym*>(args->at(i).uval & ~Magic::Mask);
            names[i] = const_cast<char*>(sym->val());
        }
        lam_value y = {.uval = lam_u64(func) | Magic::TagObj};
        return y;
    });

    ret->add_operative("if", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n >= 2);
        assert(n <= 3);
        auto cond = lam_eval(env, a[0]);
        if (truthy(cond)) {
            return lam_eval(env, a[1]);
        } else if (n == 3) {
            return lam_eval(env, a[2]);
        } else {
            assert(false);
            return lam_Double(0);
        }
    });
    // add_special("quote",
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
        return lam_ListN(n, a);
    });

    ret->add_applicative("equal?", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        auto l = a[0];
        auto r = a[1];
        if ((l.uval & Magic::Mask) != (r.uval & Magic::Mask)) {
            return lam_Int(false);
        } else if ((l.uval & Magic::Mask) == Magic::TagInt) {
            return lam_Int(unsigned(l.uval) == unsigned(r.uval));
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
                return lam_Double(a[0].dval * a[1].dval);
            case lam_type::Int:
                return lam_Int(a[0].as_int() * a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("+", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_Double(a[0].dval + a[1].dval);
            case lam_type::Int:
                return lam_Int(a[0].as_int() + a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("-", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_Double(a[0].dval - a[1].dval);
            case lam_type::Int:
                return lam_Int(a[0].as_int() - a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_applicative("/", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        assert(a[0].type() == lam_type::Double);  // TODO
        assert(a[1].type() == lam_type::Double);
        return lam_Double(a[0].dval / a[1].dval);
    });
    ret->add_applicative("<=", [](lam_callable* call, lam_env* env, auto a, auto n) {
        assert(n == 2);
        switch (lam_env_1::coerce_numeric_types(a[0], a[1])) {
            case lam_type::Double:
                return lam_Int(a[0].dval <= a[1].dval);
            case lam_type::Int:
                return lam_Int(a[0].as_int() <= a[1].as_int());
            default:
                assert(false);
                return lam_value{};
        }
    });
    ret->add_value("pi", lam_Double(3.14159));
    return ret;
}

lam_env* lam_env_builtin() {
    return lam_env_1::builtin();
}

lam_value lam_eval_obj(lam_env* env, lam_obj* obj) {
    switch (obj->type) {
        case lam_type::Symbol: {
            auto sym = static_cast<lam_sym*>(obj);
            return env->lookup(sym->val());
        }
        case lam_type::List: {
            auto list = static_cast<lam_list*>(obj);
            assert(list->len);
            lam_value head = lam_eval(env, list->at(0));
            lam_callable* callable = reinterpret_cast<lam_callable*>(head.uval & ~Magic::Mask);

            if (callable->type == lam_type::Operative) {
                return callable->invoke(callable, env, list->first() + 1, list->len - 1);
            } else if (callable->type == lam_type::Applicative) {
                std::vector<lam_value> args;
                args.resize(list->len - 1);
                for (unsigned i = 1; i < list->len; ++i) {
                    args[i - 1] = lam_eval(env, list->at(i));
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

lam_value lam_eval(lam_env* env, lam_value val) {
    switch (val.uval & Magic::Mask) {
        case Magic::TagObj:
            return lam_eval_obj(env, reinterpret_cast<lam_obj*>(val.uval & ~Magic::Mask));
        case Magic::TagInt:
            return val;
        default:
            return val;
    }
}