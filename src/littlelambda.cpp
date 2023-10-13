#include "littlelambda.h"
#include <cstring>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(disable : 6011)  // Dereferencing NULL pointer 'pointer-name'.

enum ErrorCode {
    OK = 0,
    ParseUnexpectedNull,
    ParseUnexpectedEscape,
    SymbolNotFound,
    WrongNumberOfArguments,
    NonNumericArguments,
};

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
lam_value lam_parse(const char* input, const char** restart) {
    *restart = input;
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
                    *restart = cur;
                    return l;
                }
                curList = &stack.back();
                curList->push_back(l);
                break;
            }

            case '"': {
                const char* start = cur;
                std::string res;
                bool done = false;
                while (!done) {
                    switch (char c = *cur++) {
                        case '0': {
                            return lam_make_error(ParseUnexpectedNull,
                                                  "Unexpected null when parsing string");
                        }
                        case '\\': {
                            if (*cur == 'n') {
                                res.append(start, cur - 1);
                                res.push_back('\n');
                                cur += 1;
                                start = cur;
                            } else {
                                return lam_make_error(ParseUnexpectedEscape,
                                                      "Unexpected escape sequence");
                            }
                            break;
                        }
                        case '"': {
                            res.append(start, cur - 1);
                            auto val = lam_make_string(res.data(), res.size());
                            if (curList) {
                                curList->push_back(val);
                            } else {
                                *restart = cur;
                                return val;
                            }
                            done = true;
                            break;
                        }
                        default:
                            break;
                    }
                }
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
                    val = lam_make_symbol(start, end - start);
                } while (false);

                if (curList) {
                    curList->push_back(val);
                } else {
                    *restart = cur;
                    return val;
                }
                break;
            }
        }
    }
    return lam_make_null();
}

lam_value lam_parse(const char* input) {
    const char* restart = nullptr;
    auto r = lam_parse(input, &restart);
    assert(*restart == 0);
    return r;
}

lam_value lam_make_symbol(const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = callocPlus<lam_symbol>(len + 1);
    d->type = lam_type::Symbol;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_string(const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = callocPlus<lam_string>(len + 1);
    d->type = lam_type::String;
    d->len = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_bigint(int i) {
    auto* d = callocPlus<lam_bigint>(0);
    d->type = lam_type::BigInt;
    mpz_init_set_si(d->mp, i);
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_error(unsigned code, const char* msg) {
    auto* d = callocPlus<lam_error>(0);
    d->type = lam_type::Error;
    d->code = code;
    d->msg = msg;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_bigint(mpz_t m) {
    auto* d = callocPlus<lam_bigint>(0);
    d->type = lam_type::BigInt;
    static_assert(sizeof(d->mp) == 16);
    memcpy(d->mp, m, sizeof(d->mp));
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_list_v(const lam_value* values, size_t len) {
    auto* d = callocPlus<lam_list>(len * sizeof(lam_value));
    d->type = lam_type::List;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, values, len * sizeof(lam_value));
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

struct lam_env : lam_obj {
    lam_env(lam_env* parent, const char* name)
        : lam_obj(lam_type::Environment), _parent{parent}, _name{name} {}

    lam_env(const char* keys[],
            size_t nkeys,
            lam_value* values,
            size_t nvalues,
            lam_env* parent,
            const char* variadic)
        : lam_obj(lam_type::Environment), _parent(parent) {
        assert((nvalues == nkeys) || (variadic && nvalues >= nkeys));
        for (size_t i = 0; i < nkeys; ++i) {
            add_value(keys[i], values[i]);
        }
        if (variadic) {
            lam_value kw = lam_make_list_v(values + nkeys, nvalues - nkeys);
            add_value(variadic, kw);
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
        d->context = nullptr;
        lam_value v{.uval = lam_u64(d) | lam_Magic::TagObj};
        _map.emplace(name, v);
    }
    void add_operative(const char* name, lam_invoke b, const void* context = nullptr) {
        auto* d = callocPlus<lam_callable>(0);
        d->type = lam_type::Operative;
        d->name = name;
        d->invoke = b;
        d->env = nullptr;
        d->body = {};
        d->num_args = 0;
        d->context = context;
        lam_value v{.uval = lam_u64(d) | lam_Magic::TagObj};
        _map.emplace(name, v);
    }

    // symStr is a possibly-dotted identifier
    static lam_value _lookup(const char* symStr, lam_env* startEnv) {
        std::string_view todo{symStr};
        lam_env* env = startEnv;
        while (1) {
            // Extract 'cur' - the next segment of the dotted path
            auto dot = todo.find('.', 0);
            std::string_view cur = todo;
            if (dot != std::string::npos) {
                cur = cur.substr(0, dot);
                todo.remove_prefix(dot + 1);
            } else {  // end of path
                todo = {};
            }

            // Look up 'cur', return it if it's the last segment.
            // Otherwise update 'env' and loop for next segment.
            for (auto e = env;;) {
                auto it = e->_map.find(cur);
                if (it != e->_map.end()) {
                    lam_value v = it->second;
                    if (todo.empty()) {  // we're at the last dotted id
                        return v;
                    } else {  // go to next dot
                        env = v.as_env();
                        break;  // to outer loop
                    }
                }
                if (auto p = e->_parent) {
                    e = e->_parent;
                } else {
                    return lam_make_error(SymbolNotFound, "symbol not found");
                }
            }
        }
    }
    lam_value lookup(const char* sym) { return _lookup(sym, this); }

    void insert(const char* sym, lam_value value) {
        auto pair = _map.emplace(sym, value);
        assert(pair.second && "symbol already defined");
    }

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
    struct string_hash {
        using is_transparent = void;
        [[nodiscard]] size_t operator()(const char* txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(std::string_view txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(const std::string& txt) const {
            return std::hash<std::string>{}(txt);
        }
    };
    std::unordered_map<std::string, lam_value, string_hash, std::equal_to<>> _map;
    lam_env* _parent{nullptr};
    const char* _name{nullptr};
};

static lam_value_or_tail_call lam_invokelambda(lam_callable* call,
                                               lam_env* env,
                                               lam_value* args,
                                               auto narg) {
    lam_env* inner = inner = new lam_env((const char**)call->args(), call->num_args, args, narg,
                                         call->env, call->variadic);
    return {call->body, inner};
}

static bool truthy(lam_value v) {
    if ((v.uval & lam_Magic::Mask) == lam_Magic::TagInt) {
        return unsigned(v.uval) != 0;
    } else if ((v.uval & lam_Magic::Mask) == lam_Magic::TagObj) {
        lam_obj* obj = reinterpret_cast<lam_obj*>(v.uval & ~lam_Magic::Mask);
        if (obj->type == lam_type::List) {
            lam_list* list = reinterpret_cast<lam_list*>(obj);
            return list->len != 0;
        }
    }
    assert(false);
    return false;
}

void lam_print(lam_value val) {
    switch (val.type()) {
        case lam_type::Double:
            printf("%lf", val.as_double());
            break;
        case lam_type::Int:
            printf("%i", val.as_int());
            break;
        case lam_type::Null:
            printf("null");
            break;
        case lam_type::Symbol:
            printf(":%s", val.as_symbol()->val());
            break;
        case lam_type::String:
            printf("%s", val.as_string()->val());
            break;
        case lam_type::List: {
            printf("(");
            auto lst = val.as_list();
            const char* sep = "";
            for (auto i = 0; i < lst->len; ++i) {
                printf("%s", sep);
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
        case lam_type::Environment:
            printf("E{%p}", val.as_env());
            break;
        default:
            assert(false);
    }
}

lam_value lam_eval_call(lam_callable* call, lam_env* env, lam_value* args, size_t narg) {
    auto ret = call->invoke(call, env, args, narg);
    if (ret.env == nullptr) {
        return ret.value;
    }
    return lam_eval(ret.value, ret.env);
}

static constexpr int combine_numeric_types(lam_type xt, lam_type yt) {
    assert(xt <= lam_type::BigInt);
    assert(yt <= lam_type::BigInt);
    static_assert(int(lam_type::BigInt) <= 3);
    return (int(xt) << 2) | int(yt);
}

lam_env* lam_make_env_builtin(lam_hooks* hooks) {
    lam_env* ret = new lam_env(nullptr, "builtin");
    if (hooks) {
        lam_env* _hooks = new lam_env(nullptr, "_hooks");
        if (hooks->import) {
            _hooks->add_operative(
                "_import",
                [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
                    assert(n == 1);
                    auto modname = a[0].as_symbol();
                    auto h = static_cast<const lam_hooks*>(call->context);
                    return h->import(modname->val());
                },
                hooks);
        }
        ret->add_value("_hooks", lam_make_value(_hooks));
    }

    ret->add_operative(
        "define", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            auto lhs = a[0];
            auto rhs = a[1];
            if (lhs.type() == lam_type::Symbol) {
                auto sym = reinterpret_cast<lam_symbol*>(lhs.uval & ~lam_Magic::Mask);
                auto r = lam_eval(rhs, env);
                env->insert(sym->val(), r);
            } else if (lhs.type() == lam_type::List) {
                auto argsList = reinterpret_cast<lam_list*>(lhs.uval & ~lam_Magic::Mask);
                std::span<lam_value> args{argsList->first() + 1,
                                          argsList->len - 1};  // drop sym from args list
                const char* variadic{nullptr};

                if (args.size() >= 2 && args[args.size() - 2].as_symbol()->val()[0] == '.') {
                    variadic = args[args.size() - 1].as_symbol()->val();
                    args = args.subspan(0, args.size() - 2);
                }
                auto func =
                    callocPlus<lam_callable>(args.size() * sizeof(char*));  // TODO intern names
                const char* name = argsList->at(0).as_symbol()->val();
                func->type = lam_type::Applicative;
                func->invoke = &lam_invokelambda;
                func->name = name;
                func->env = env;
                func->body = rhs;
                func->num_args = args.size();
                func->variadic = variadic;
                char** names = func->args();
                for (size_t i = 0; i < args.size(); ++i) {
                    names[i] = const_cast<char*>(args[i].as_symbol()->val());
                }
                lam_value y = {.uval = lam_u64(func) | lam_Magic::TagObj};
                env->insert(name, y);
            } else {
                assert(false);
            }
            return lam_value{};
        });

    ret->add_operative(
        "lambda", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            if (n != 2) {
                return lam_make_error(WrongNumberOfArguments, "(lambda args body)");
            }
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
                variadic = lhs.as_symbol()->val();
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
                auto sym = argp[i].as_symbol();
                names[i] = const_cast<char*>(sym->val());
            }
            lam_value y = {.uval = lam_u64(func) | lam_Magic::TagObj};
            return y;
        });

    ret->add_operative(
        "if", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 2);
            assert(n <= 3);
            auto cond = lam_eval(a[0], env);
            if (truthy(cond)) {
                return lam_value_or_tail_call(a[1], env);
            } else if (n == 3) {
                return lam_value_or_tail_call(a[2], env);
            } else {
                assert(false);
                return lam_make_double(0);
            }
        });
    ret->add_operative(
        "module", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            auto modname = a[0].as_symbol();
            lam_env* inner = new lam_env(env, modname->val());
            lam_value r = lam_make_null();
            for (size_t i = 1; i < n; i += 1) {
                r = lam_eval(a[i], inner);
            }
            env->insert(modname->val(), lam_make_value(inner));
            return r;
        });
    ret->add_operative(
        "import", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            auto modname = a[0].as_symbol();
            lam_value imp = env->lookup("_hooks._import");
            lam_callable* func = imp.as_func();
            lam_value result = lam_eval_call(func, nullptr, a, 1);
            env->insert(modname->val(), result);
            return result;
        });
    ret->add_operative(
        "quote", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            return a[0];
        });
    ret->add_operative(
        "begin", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            for (size_t i = 0; i < n - 1; ++i) {
                lam_eval(a[i], env);
            }
            return {a[n - 1], env};  // tail call
        });
    ret->add_operative(
        "let", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 2);
            lam_env* inner = inner = new lam_env(nullptr, 0, nullptr, 0, env, nullptr);
            lam_list* locals = a[0].as_list();
            assert(locals);
            assert(locals->len % 2 == 0);
            for (size_t i = 0; i < locals->len; i += 2) {
                auto k = locals->at(i + 0);
                auto s = k.as_symbol();
                assert(s);
                auto v = lam_eval(locals->at(i + 1), inner);
                inner->add_value(s->val(), v);
            }

            for (size_t i = 1; i < n - 1; ++i) {
                lam_eval(a[i], env);
            }
            return {a[n - 1], inner};  // tail call
        });
    ret->add_applicative(
        "eval", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            switch (n) {
                case 1:
                    return lam_eval(a[0], env);
                case 2:
                    return lam_eval(a[0], a[1].as_env());
                default:
                    return lam_make_error(WrongNumberOfArguments, "(eval expr) or (eval expr env)");
            }
        });
    ret->add_applicative(
        "getenv", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 0);
            return lam_make_value(env);
        });

    ret->add_applicative(
        "print", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            for (auto i = 0; i < n; ++i) {
                lam_print(a[i]);
            }
            return lam_value{};
        });

    ret->add_applicative(
        "list", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            return lam_make_list_v(a, n);
        });

    ret->add_applicative(
        "bigint", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            assert(a[0].type() == lam_type::Int);
            return lam_make_bigint(a[0].as_int());
        });

    ret->add_applicative(
        "equal?", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            auto l = a[0];
            auto r = a[1];
            if ((l.uval & lam_Magic::Mask) != (r.uval & lam_Magic::Mask)) {
                return lam_make_int(false);
            } else if ((l.uval & lam_Magic::Mask) == lam_Magic::TagInt) {
                return lam_make_int(unsigned(l.uval) == unsigned(r.uval));
            }
            // TODO other types
            assert(false);
            return lam_value{};
        });

    ret->add_applicative(
        "mapreduce",
        [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 3);
            lam_callable* map = a[0].as_func();
            lam_callable* red = a[1].as_func();
            lam_list* lst = a[2].as_list();
            assert(lst->len >= 1);
            lam_value acc[]{lam_eval_call(map, env, lst->first(), 1), {}};
            for (size_t i = 1; i < lst->len; ++i) {
                acc[1] = lam_eval_call(map, env, lst->first() + i, 1);
                acc[0] = lam_eval_call(red, env, acc, 2);
            }
            return acc[0];
        });

    ret->add_applicative(
        "*", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            lam_value x = a[0];
            lam_value y = a[1];
            lam_type xt = x.type();
            lam_type yt = y.type();
            switch (combine_numeric_types(xt, yt)) {
                case combine_numeric_types(lam_type::Double, lam_type::Double):
                    return lam_make_double(x.dval * y.dval);
                case combine_numeric_types(lam_type::Double, lam_type::Int):
                    return lam_make_double(x.dval * double(y.as_int()));
                case combine_numeric_types(lam_type::Double, lam_type::BigInt):
                    assert(false);
                    return lam_value{};

                case combine_numeric_types(lam_type::Int, lam_type::Double):
                    return lam_make_double(double(x.as_int()) * y.dval);
                case combine_numeric_types(lam_type::Int, lam_type::Int):
                    return lam_make_int(x.as_int() * y.as_int());
                case combine_numeric_types(lam_type::Int, lam_type::BigInt): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul_si(r, y.as_bigint()->mp, x.as_int());
                    return lam_make_bigint(r);
                }

                case combine_numeric_types(lam_type::BigInt, lam_type::Double):
                    assert(false);
                    break;
                case combine_numeric_types(lam_type::BigInt, lam_type::Int): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul_si(r, x.as_bigint()->mp, y.as_int());
                    return lam_make_bigint(r);
                } break;
                case combine_numeric_types(lam_type::BigInt, lam_type::BigInt): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul(r, x.as_bigint()->mp, y.as_bigint()->mp);
                    return lam_make_bigint(r);
                }
                default:
                    assert(false);
            }
            return lam_value{};
            /*assert(n == 2);
            switch (lam_env::coerce_numeric_types(a[0], a[1])) {
                case lam_type::Double:
                    return lam_make_double(a[0].dval * a[1].dval);
                case lam_type::Int:
                    return lam_make_int(a[0].as_int() * a[1].as_int());
                case lam_type::BigInt: {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul(r, a[0].as_bigint()->mp, a[1].as_bigint()->mp);
                    return lam_make_bigint(r);
                }
                default:
                    assert(false);
                    return lam_value{};
            }*/
        });
    ret->add_applicative(
        "+", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            switch (lam_env::coerce_numeric_types(a[0], a[1])) {
                case lam_type::Double:
                    return lam_make_double(a[0].dval + a[1].dval);
                case lam_type::Int:
                    return lam_make_int(a[0].as_int() + a[1].as_int());
                default:
                    assert(false);
                    return lam_value{};
            }
        });
    ret->add_applicative(
        "-", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            lam_value x = a[0];
            lam_value y = a[1];
            lam_type xt = x.type();
            lam_type yt = y.type();
            switch (combine_numeric_types(xt, yt)) {
                case combine_numeric_types(lam_type::Double, lam_type::Double):
                    return lam_make_double(x.dval - y.dval);
                case combine_numeric_types(lam_type::Double, lam_type::Int):
                    return lam_make_double(x.dval - double(y.as_int()));
                case combine_numeric_types(lam_type::Double, lam_type::BigInt):
                    assert(false);
                    return lam_value{};

                case combine_numeric_types(lam_type::Int, lam_type::Double):
                    return lam_make_double(double(x.as_int()) - y.dval);
                case combine_numeric_types(lam_type::Int, lam_type::Int):
                    return lam_make_int(x.as_int() - y.as_int());
                case combine_numeric_types(lam_type::Int, lam_type::BigInt):
                    assert(false);
                    // return lam_make_bigint(x.as_int() - y.as_int());
                    // c = mpz_cmp_si(y.as_bigint()->mp, x.as_int()) >= 0;
                    return lam_value{};

                case combine_numeric_types(lam_type::BigInt, lam_type::Double):
                    assert(false);
                    break;
                case combine_numeric_types(lam_type::BigInt, lam_type::Int): {
                    mpz_t r;
                    mpz_init(r);
                    long z = y.as_int();
                    if (z >= 0) {
                        mpz_sub_ui(r, x.as_bigint()->mp, z);
                    } else {
                        mpz_add_ui(r, x.as_bigint()->mp, -z);
                    }
                    return lam_make_bigint(r);
                }
                case combine_numeric_types(lam_type::BigInt, lam_type::BigInt): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_sub(r, x.as_bigint()->mp, y.as_bigint()->mp);
                    return lam_make_bigint(r);
                }
                default:
                    assert(false);
            }
            return lam_value{};
            /*assert(n == 2);
                switch (lam_env::coerce_numeric_types(a[0], a[1])) {
                    case lam_type::Double:
                        return lam_make_double(a[0].dval - a[1].dval);
                    case lam_type::Int:
                        return lam_make_int(a[0].as_int() - a[1].as_int());
                    default:
                        assert(false);
                        return lam_value{};
                }*/
        });
    ret->add_applicative(
        "/", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            assert(a[0].type() == lam_type::Double);  // TODO
            assert(a[1].type() == lam_type::Double);
            return lam_make_double(a[0].dval / a[1].dval);
        });
    ret->add_applicative(
        "<=", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            lam_value x = a[0];
            lam_value y = a[1];
            lam_type xt = x.type();
            lam_type yt = y.type();
            int c = 0;
            switch (combine_numeric_types(xt, yt)) {
                case combine_numeric_types(lam_type::Double, lam_type::Double):
                    c = x.dval <= y.dval;
                    break;
                case combine_numeric_types(lam_type::Double, lam_type::Int):
                    c = x.dval <= double(y.as_int());
                    break;
                case combine_numeric_types(lam_type::Double, lam_type::BigInt):
                    c = mpz_cmp_d(y.as_bigint()->mp, x.as_double()) >= 0;
                    break;

                case combine_numeric_types(lam_type::Int, lam_type::Double):
                    c = double(x.as_int()) <= y.dval;
                    break;
                case combine_numeric_types(lam_type::Int, lam_type::Int):
                    c = x.as_int() <= y.as_int();
                    break;
                case combine_numeric_types(lam_type::Int, lam_type::BigInt):
                    c = mpz_cmp_si(y.as_bigint()->mp, x.as_int()) >= 0;
                    break;

                case combine_numeric_types(lam_type::BigInt, lam_type::Double):
                    c = mpz_cmp_d(x.as_bigint()->mp, y.as_double()) <= 0;
                    break;
                case combine_numeric_types(lam_type::BigInt, lam_type::Int):
                    c = mpz_cmp_si(x.as_bigint()->mp, y.as_int()) <= 0;
                    break;
                case combine_numeric_types(lam_type::BigInt, lam_type::BigInt):
                    c = mpz_cmp(x.as_bigint()->mp, y.as_bigint()->mp) <= 0;
                    break;
                default:
                    assert(false);
                    return lam_value{};
            }
            return lam_make_int(c);
        });
    ret->add_value("null", lam_make_null());
    return ret;
}

lam_value lam_eval(lam_value val, lam_env* env) {
    while (true) {
        switch (val.uval & lam_Magic::Mask) {
            case lam_Magic::TagObj: {
                lam_obj* obj = reinterpret_cast<lam_obj*>(val.uval & ~lam_Magic::Mask);
                switch (obj->type) {
                    case lam_type::Symbol: {
                        auto sym = static_cast<lam_symbol*>(obj);
                        return env->lookup(sym->val());
                    }
                    case lam_type::List: {
                        auto list = static_cast<lam_list*>(obj);
                        assert(list->len);
                        lam_value head = lam_eval(list->at(0), env);
                        lam_callable* callable = head.as_func();

                        std::vector<lam_value> evaluatedArgs;
                        std::span<lam_value> args{list->first() + 1, list->len - 1};

                        // applicative arguments get evaluated, operative do not
                        if (callable->type == lam_type::Applicative) {
                            evaluatedArgs.resize(args.size());
                            for (unsigned i = 0; i < args.size(); ++i) {
                                evaluatedArgs[i] = lam_eval(args[i], env);
                            }
                            args = {evaluatedArgs.data(), evaluatedArgs.size()};
                        }

                        lam_value_or_tail_call res =
                            callable->invoke(callable, env, args.data(), args.size());
                        if (res.env == nullptr) {
                            return res.value;
                        } else {  // tail call
                            val = res.value;
                            env = res.env;
                        }
                        break;
                    }
                    case lam_type::String:
                    case lam_type::Applicative:
                    case lam_type::Operative:
                    case lam_type::Error: {
                        return lam_make_value(obj);
                    }
                    default: {
                        assert(0);
                        return {};
                    }
                }
                break;
            }
            case lam_Magic::TagConst:
            case lam_Magic::TagInt:
                return val;
            default:  // double
                return val;
        }
    }
}