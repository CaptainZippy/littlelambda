#include "littlelambda.h"
#include <inttypes.h>
#include <charconv>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(disable : 6011)  // Dereferencing NULL pointer 'pointer-name'.

enum ErrorCode {
    OK = 0,
    GenericFailure,
    ParseEndOfInput,
    ParseUnexpectedNull,
    ParseUnexpectedSemiColon,
    ParseUnexpectedEscape,
    ParseUnexpectedEndOfFile,
    ParseUnexpectedEndList,
    ParseMissingSymbolName,
    ImportNotFound,
    SymbolNotFound,
    WrongNumberOfArguments,
    NonNumericArguments,
};

static lam_value lam_eval(lam_value val, lam_env* env);

static bool is_white(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}
static bool is_word_boundary(char c) {
    return is_white(c) || c == '(' || c == ')' || c == '\0';
}
static bool is_newline(char c) {
    return c == '\r' || c == '\n';
}
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

template <typename T, typename... V>
static bool _try_parse_as(const char* start, const char* end, T& out, V... v) {
    auto [ptr, rc] = std::from_chars(start, end, out, v...);
    if (rc == std::errc{} && ptr == end) {
        return true;
    }
    return false;
}

lam_hooks::~lam_hooks() = default;

struct lam_vm {
    ugc_t gc{};
    lam_hooks* hooks{};
    lam_env* root{};
    std::unordered_map<std::string, lam_value> imports{};
    struct {
        lam_u64 alloc_count{};
        lam_u64 free_count{};
        lam_u64 gc_iter_count{};
    } gc_stats;
};

// Allocate and zero "sizeof(T) + extra" bytes
// Register T with the garbage collector
template <typename T>
static T* callocPlus(lam_vm* vm, size_t extra) {
    void* p = vm->hooks->mem_alloc(sizeof(T) + extra);
    memset(p, 0, sizeof(T) + extra);
    auto o = reinterpret_cast<T*>(p);
    ugc_register(&vm->gc, &o->header);
    vm->gc_stats.alloc_count += 1;
    return o;
}

// Parse null terminated 'input'
// Set 'restart' to the end of parsing.
lam_result lam_parse(lam_vm* vm, const char* input, const char* endInput, const char** restart) {
    *restart = input;
    // No recursion - explicit stack for lists.
    std::vector<std::vector<lam_value>> stack;
    for (const char* cur = input; cur < endInput;) {
        std::optional<lam_value> parsed{};
        const char* startCur = cur;

        // Try to parse an single value & put it into 'parsed'
        // After this switch, there is a common block to manage the stack
        // and update the restart point etc.
        switch (*cur++) {
            // parse_end
            case '\0': {
                *restart = cur - 1;
                if (!stack.empty()) {
                    return lam_result::fail(ParseUnexpectedEndOfFile,
                                            "End of file in compound expression");
                }
                return lam_result::fail(ParseEndOfInput, nullptr);
            }
            // Whitespace
            case ' ':
            case '\t':
            case '\r':
            case '\n': {
                break;
            }
            // parse_comment ";;" to eol is comment
            case ';': {
                if (cur >= endInput || *cur != ';') {
                    return lam_result::fail(ParseUnexpectedSemiColon, "Unexpected single ';'");
                }
                // Look for the start of any newline sequence \r, \n, \r\n
                while (cur < endInput && !is_newline(*cur)) {
                    ++cur;
                }
                // Consume any sequence of \r,\n
                while (cur < endInput && is_newline(*cur)) {
                    ++cur;
                }
                break;
            }
            // parse_list
            case '(': {
                stack.emplace_back();
                break;
            }
            case ')': {
                if (stack.empty()) {
                    return lam_result::fail(ParseUnexpectedEndList,
                                            "End of list without beginning");
                }
                std::vector<lam_value> curList = std::move(stack.back());
                stack.pop_back();
                // A literal '.' at the end of a list is a shorthand for appending all
                // statements until the end of the scope.
                // e.g. "(foo x .) (a b c) (d e)" is equivalent to "(foo x (a b c) (d e))"
                if (!curList.empty()) {
                    lam_value v = curList.back();
                    if (v.type() == lam_type::Symbol && strcmp(v.as_symbol()->val(), ".") == 0) {
                        std::vector<lam_value> tail;
                        for (bool slurp = true; slurp && cur < endInput;) {
                            const char* next = nullptr;
                            lam_result res = lam_parse(vm, cur, endInput, &next);
                            cur = next;
                            switch (res.code) {
                                case 0:
                                    tail.push_back(res.value);
                                    break;
                                case ParseEndOfInput:
                                case ParseUnexpectedEndList:
                                    slurp = false;
                                    break;
                                default:
                                    return res;
                            }
                        }
                        curList.pop_back();
                        curList.insert(curList.end(), tail.begin(), tail.end());
                    }
                }
                auto l = lam_make_list_v(vm, curList.data(), curList.size());
                parsed.emplace(l);
                break;
            }
            // parse_string
            case '"': {
                const char* start = cur;  // start of the current run
                std::string acc;          // accumulator for current string
                while (!parsed.has_value() && cur < endInput) {
                    switch (char c = *cur++) {
                        case 0: {
                            return lam_result::fail(ParseUnexpectedNull,
                                                    "Unexpected null when parsing string");
                        }
                        case '\\': {
                            if (cur < endInput && *cur == 'n') {
                                acc.append(start, cur - 1);
                                acc.push_back('\n');
                                cur += 1;
                                start = cur;
                            } else {
                                return lam_result::fail(ParseUnexpectedEscape,
                                                        "Unexpected escape sequence");
                            }
                            break;
                        }
                        case '"': {
                            acc.append(start, cur - 1);
                            auto s = lam_make_string(vm, acc.data(), acc.size());
                            parsed.emplace(s);
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            }
            // parse_quote
            case '\'': {
                const char* after = nullptr;
                lam_result quoted = lam_parse(vm, cur, endInput, &after);
                if (quoted.code != 0) {
                    return quoted;
                }
                lam_value val = lam_make_list_l(vm, lam_make_symbol(vm, "$quote"), quoted.value);
                parsed.emplace(val);
                cur = after;
                break;
            }

                //    // keyword
                // case ':': {
                //    while (!is_word_boundary(*cur)) {
                //        ++cur;
                //    }
                //    lam_value val = (cur - start > 1) ? lam_make_symbol(start + 1, cur - start -
                //    1)
                //                                      : lam_make_error(ParseMissingSymbolName,
                //                                                       "Missing symbol name after
                //                                                       ':'");
                //    if (curList) {
                //        curList->push_back(val);
                //    } else {
                //        *restart = cur;
                //        return val;
                //    }
                //    break;
                //}
            // parse_number parse_symbol
            default: {
                while (cur < endInput && !is_word_boundary(*cur)) {
                    ++cur;
                }

                // Todo: tighten these checks. Numbers must begin with - or . or digit?
                if (cur > startCur && is_alpha(startCur[0])) {
                    parsed.emplace(lam_make_symbol(vm, startCur, cur - startCur));
                } else if (long asInt; _try_parse_as<long>(startCur, cur, asInt, 10)) {
                    parsed.emplace(lam_make_int(asInt));
                } else if (double asDbl; _try_parse_as<double>(startCur, cur, asDbl)) {
                    parsed.emplace(lam_make_double(asDbl));
                } else {
                    parsed.emplace(lam_make_symbol(vm, startCur, cur - startCur));
                }

                break;
            }
        }

        // Consume any whitespace & advance restart point
        while (cur < endInput && is_white(*cur)) {
            ++cur;
        }
        *restart = cur;

        // Check for explicit recursion end
        if (parsed.has_value()) {
            auto v = parsed.value();
            if (auto s = stack.size()) {
                stack[s - 1].push_back(v);
            } else {
                *restart = cur;
                return lam_result::ok(v);
            }
        }
    }
    if (stack.size()) {
        return lam_result::fail(ParseEndOfInput, "Unexpected Eof");
    }
    return lam_result::ok(lam_make_int(0));
}

lam_value lam_make_symbol(lam_vm* vm, const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = callocPlus<lam_symbol>(vm, len + 1);
    d->type = lam_type::Symbol;
    d->len = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_string(lam_vm* vm, const char* s, size_t n) {
    size_t len = n == size_t(-1) ? strlen(s) : n;
    auto* d = callocPlus<lam_string>(vm, len + 1);
    d->type = lam_type::String;
    d->len = len;
    memcpy(d + 1, s, len);
    reinterpret_cast<char*>(d + 1)[len] = 0;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_bigint(lam_vm* vm, int i) {
    auto* d = callocPlus<lam_bigint>(vm, 0);
    d->type = lam_type::BigInt;
    mpz_init_set_si(d->mp, i);
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_error(lam_vm* vm, unsigned code, const char* msg) {
    auto* d = callocPlus<lam_error>(vm, 0);
    d->type = lam_type::Error;
    d->code = code;
    d->msg = msg;
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_bigint(lam_vm* vm, mpz_t m) {
    auto* d = callocPlus<lam_bigint>(vm, 0);
    d->type = lam_type::BigInt;
    static_assert(sizeof(d->mp) == 16);
    memcpy(d->mp, m, sizeof(d->mp));
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_value lam_make_list_v(lam_vm* vm, const lam_value* values, size_t len) {
    auto* d = callocPlus<lam_list>(vm, len * sizeof(lam_value));
    d->type = lam_type::List;
    d->len = len;
    d->cap = len;
    memcpy(d + 1, values, len * sizeof(lam_value));
    return {.uval = lam_u64(d) | lam_Magic::TagObj};
}

lam_env::lam_env(lam_vm* v) : lam_obj(lam_type::Environment), vm(v) {}

struct lam_env_impl : lam_env {
    inline void* operator new(std::size_t n, void* ptr) { return ptr; }

    lam_env_impl(lam_vm* vm, lam_env* parent, const char* name)
        : lam_env(vm), _parent{parent}, _name{name} {}

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
    static lam_value _lookup(const char* symStr, const lam_env* startEnv);
    std::unordered_map<std::string, lam_value, string_hash, std::equal_to<>> _map;
    lam_env* _parent{nullptr};
    const char* _name{nullptr};
    bool _sealed{false};
};

static lam_env* lam_new_env(lam_vm* vm, lam_env* parent, const char* name) {
    assert(parent == nullptr || vm == static_cast<lam_env_impl*>(parent)->vm);
    auto* d = callocPlus<lam_env_impl>(vm, 0);
    return new (d) lam_env_impl(vm, parent, name);
}

lam_value lam_make_env(lam_vm* vm, lam_env* parent, const char* name) {
    return {.uval = lam_u64(lam_new_env(vm, parent, name)) | lam_Magic::TagObj};
}

void lam_env::seal() {
    auto self = static_cast<lam_env_impl*>(this);
    self->_sealed = true;
}

void lam_env::bind_multiple(const char* keys[],
                            size_t nkeys,
                            lam_value* values,
                            size_t nvalues,
                            const char* variadic) {
    auto self = static_cast<lam_env_impl*>(this);
    assert(!self->_sealed);
    assert((nvalues == nkeys) || (variadic && nvalues >= nkeys));
    for (size_t i = 0; i < nkeys; ++i) {
        bind(keys[i], values[i]);
    }
    if (variadic) {
        lam_value kw = lam_make_list_v(vm, values + nkeys, nvalues - nkeys);
        bind(variadic, kw);
    }
}

void lam_env::bind(const char* name, lam_value value) {
    auto self = static_cast<lam_env_impl*>(this);
    assert(!self->_sealed);
    auto pair = self->_map.emplace(name, value);
    assert(pair.second && "symbol already defined");
}

void lam_env::bind_applicative(const char* name, lam_invoke b) {
    auto self = static_cast<lam_env_impl*>(this);
    assert(!self->_sealed);
    auto* d = callocPlus<lam_callable>(self->vm, 0);
    d->type = lam_type::Applicative;
    d->name = name;
    d->invoke = b;
    d->env = nullptr;
    d->body = {};
    d->num_args = 0;
    d->context = nullptr;
    lam_value v{.uval = lam_u64(d) | lam_Magic::TagObj};
    self->_map.emplace(name, v);
}

void lam_env::bind_operative(const char* name, lam_invoke b, void* context) {
    auto self = static_cast<lam_env_impl*>(this);
    assert(!self->_sealed);
    auto* d = callocPlus<lam_callable>(self->vm, 0);
    d->type = lam_type::Operative;
    d->name = name;
    d->invoke = b;
    d->env = nullptr;
    d->body = {};
    d->num_args = 0;
    d->context = context;
    lam_value v{.uval = lam_u64(d) | lam_Magic::TagObj};
    self->_map.emplace(name, v);
}

// symStr is a possibly-dotted identifier
lam_value lam_env_impl::_lookup(const char* symStr, const lam_env* startEnv) {
    std::string_view todo{symStr};
    auto env = static_cast<const lam_env_impl*>(startEnv);
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
                    env = static_cast<lam_env_impl*>(v.as_env());
                    break;  // to outer loop
                }
            }
            if (auto p = e->_parent) {
                e = static_cast<lam_env_impl*>(e->_parent);
            } else {
                return lam_make_error(startEnv->vm, SymbolNotFound, "symbol not found");
            }
        }
    }
}

lam_value lam_env::lookup(const char* sym) const {
    return lam_env_impl::_lookup(sym, this);
}

static lam_type lam_coerce_numeric_types(lam_value& a, lam_value& b) {
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

static lam_value_or_tail_call invoke_applicative(lam_callable* call,
                                                 lam_env* env,
                                                 lam_value* args,
                                                 auto narg) {
    assert(call->envsym == nullptr);
    lam_env* inner = lam_new_env(env->vm, call->env, nullptr);
    inner->bind_multiple((const char**)call->args(), call->num_args, args, narg, call->variadic);
    return {call->body, inner};
}

static lam_value_or_tail_call invoke_operative(lam_callable* call,
                                               lam_env* env,
                                               lam_value* args,
                                               auto narg) {
    lam_env* inner = lam_new_env(env->vm, call->env, nullptr);
    inner->bind_multiple((const char**)call->args(), call->num_args, args, narg, call->variadic);
    assert(call->envsym);
    inner->bind(call->envsym, lam_make_value(env));
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

void lam_print(lam_vm* vm, lam_value val, const char* end) {
    /*struct Adaptor {
        using difference_type = std::ptrdiff_t;
        char buf[64];
        int count{0};
        lam_hooks* h;

        Adaptor(lam_hooks* hooks) : h(hooks) {}
        ~Adaptor() { _flush(); }

        void _flush() {
            if (count) {
                h->output(buf, count);
                count = 0;
            }
        }
        inline Adaptor& operator++() {
            ++count;
            if (count >= sizeof(buf)) {
                _flush();
            }
            return *this;
        }
        Adaptor& operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        inline char& operator*() { return buf[count]; }
    };
    static_assert(std::output_iterator<Adaptor, char>);
    Adaptor out{vm->hooks};    */
    char out[256];
    std::format_to_n_result<char*> next{};

    switch (val.type()) {
        case lam_type::Double:
            next = std::format_to_n(out, sizeof(out), "{}", val.as_double());
            break;
        case lam_type::Int:
            next = std::format_to_n(out, sizeof(out), "{}", val.as_int());
            break;
        case lam_type::Null:
            next = std::format_to_n(out, sizeof(out), "{}", "null");
            break;
        case lam_type::Opaque:
            next = std::format_to_n(out, sizeof(out), "Opaque<{}>", val.as_opaque());
            break;
        case lam_type::Symbol:
            next = std::format_to_n(out, sizeof(out), ":{}", val.as_symbol()->val());
            break;
        case lam_type::String:
            next = std::format_to_n(out, sizeof(out), "{}", val.as_string()->val());
            break;
        case lam_type::List: {
            vm->hooks->output("(", 1);
            auto lst = val.as_list();
            for (auto i = 0; i < lst->len; ++i) {
                lam_print(vm, lst->at(i), (i + 1 == lst->len) ? ")" : " ");
            }
            // Lists are handled recursively, 'next' is not assigned-to.
            break;
        }
        case lam_type::Applicative:
            next = std::format_to_n(out, sizeof(out), "Ap<{}>", val.as_callable()->name);
            break;
        case lam_type::Operative:
            next = std::format_to_n(out, sizeof(out), "Op<{}>", val.as_callable()->name);
            break;
        case lam_type::Environment:
            next = std::format_to_n(out, sizeof(out), "Env<{}>", (void*)val.as_env());
            break;
        case lam_type::Error:
            next = std::format_to_n(out, sizeof(out), "Err<{:x},{}>", val.as_error()->code,
                                    val.as_error()->msg);
            break;
        default:
            assert(false);
    }
    if (next.out) {
        vm->hooks->output(out, next.out - out);
    }
    if (end) {
        vm->hooks->output(end, strlen(end));
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
    assert(xt == lam_type::Int || xt == lam_type::Double || xt == lam_type::BigInt);
    assert(yt == lam_type::Int || yt == lam_type::Double || yt == lam_type::BigInt);
    int xm = xt == lam_type::Int ? 0 : xt == lam_type::Double ? 1 : /*xt == lam_type::BigInt*/ 2;
    int ym = yt == lam_type::Int ? 0 : yt == lam_type::Double ? 1 : /*yt == lam_type::BigInt*/ 2;
    return (int(xm) << 2) | int(ym);
}

static lam_env* lam_make_env_builtin(lam_vm* vm) {
    lam_env* ret = lam_new_env(vm, nullptr, "builtin");
#if false
    if (hooks) {
        lam_env* _hooks = lam_new_env(nullptr, "_hooks");
        static const char _import_func[] = "_import_func";
        _hooks->bind(_import_func, lam_make_opaque(hooks));
        if (hooks->import) {
            _hooks->bind_operative(
                "$_import",
                [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
                    assert(n == 1);
                    auto modname = a[0].as_symbol();
                    auto hobj = static_cast<lam_hooks*>(call->context);
                    auto res = hobj->import(hobj, modname->val());
                    assert(res.code == 0);
                    return res.value;
                },
                hooks);
        }
        _hooks->seal();
        ret->bind("_hooks", lam_make_value(_hooks));
    }
#endif

    ret->bind_operative(
        // ($define sym expr) Define 'sym' to be 'expr'
        // ($define (funcname args...) expr) Define function with the given arguments.
        // ($define ($operative args...) env expr) Define an operative with the given arguments.
        //      When the operative is invoked, the callers environment is bound to the symbol 'env'.
        // ($define (funcname arg0 arg1 . rest) body) A literal '.' means the function is variadic
        //      and any arguments after the required ones are bound to 'rest'
        "$define",
        [](lam_callable* call, lam_env* env, auto callArgs,
           auto numCallArgs) -> lam_value_or_tail_call {
            assert(numCallArgs >= 2);
            auto lhs = callArgs[0];
            if (lhs.type() == lam_type::Symbol) {  // ($define sym value)
                assert(numCallArgs == 2);
                auto sym = reinterpret_cast<lam_symbol*>(lhs.uval & ~lam_Magic::Mask);
                auto r = lam_eval(callArgs[1], env);
                env->bind(sym->val(), r);
            } else if (lhs.type() == lam_type::List) {  // ($define (applicative ...) body) or
                                                        // ($define ($operative ...) env body)
                auto argsList = reinterpret_cast<lam_list*>(lhs.uval & ~lam_Magic::Mask);
                const char* name = argsList->at(0).as_symbol()->val();
                std::span<lam_value> fnargs{argsList->first() + 1,
                                            argsList->len - 1};  // drop sym from args list
                const char* variadic{nullptr};

                // variadic?
                if (fnargs.size() >= 2 && fnargs[fnargs.size() - 2].as_symbol()->val()[0] == '.') {
                    variadic = fnargs[fnargs.size() - 1].as_symbol()->val();
                    fnargs = fnargs.subspan(0, fnargs.size() - 2);
                }
                auto func = callocPlus<lam_callable>(
                    env->vm, fnargs.size() * sizeof(char*));  // TODO intern names
                bool operative = name[0] == '$';
                if (operative) {
                    assert(numCallArgs == 3);
                    func->type = lam_type::Operative;
                    func->invoke = &invoke_operative;
                    func->body = callArgs[2];
                    func->envsym = callArgs[1].as_symbol()->val();
                } else {
                    assert(numCallArgs == 2);
                    func->type = operative ? lam_type::Operative : lam_type::Applicative;
                    func->invoke = &invoke_applicative;
                    func->body = callArgs[1];
                    func->envsym = nullptr;
                }
                func->name = name;
                func->env = env;
                func->num_args = fnargs.size();
                func->variadic = variadic;
                char** names = func->args();
                for (size_t i = 0; i < fnargs.size(); ++i) {
                    names[i] = const_cast<char*>(fnargs[i].as_symbol()->val());
                }
                lam_value y = {.uval = lam_u64(func) | lam_Magic::TagObj};
                env->bind(name, y);
            } else {
                assert(false);
            }
            return lam_value{};
        });

    ret->bind_operative(
        // ($lambda (args...) body) Define a function
        // ($lambda vargs body) Define a function taking an arbitrary list of arguments bound to
        // 'vargs'
        "$lambda", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            if (n != 2) {
                return lam_make_error(env->vm, WrongNumberOfArguments, "($lambda args body)");
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
            auto func =
                callocPlus<lam_callable>(env->vm, numArgs * sizeof(char*));  // TODO intern names
            func->type = lam_type::Applicative;
            func->invoke = &invoke_applicative;
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

    ret->bind_operative(
        // ($if cond iftrue iffalse) Evaluate cond and return iftrue or iffalse
        "$if", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
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

    ret->bind_operative(
        // ($module modname body...) Define the module modname
        "$module", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            auto modname = a[0].as_symbol();
            lam_env* inner = lam_new_env(env->vm, env, modname->val());
            lam_value r = lam_make_null();
            for (size_t i = 1; i < n; i += 1) {
                r = lam_eval(a[i], inner);
            }
            env->bind(modname->val(), lam_make_value(inner));
            return r;
        });

    ret->bind_operative(
        // (import modname) Import the module modname and bind it to 'modname'
        "$import", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            auto modname = a[0].as_symbol();
            auto it = env->vm->imports.find(modname->val());
            if (it == env->vm->imports.end()) {
                lam_result result = env->vm->hooks->import(env->vm, modname->val());
                if (result.code == 0) {
                    return result.value;
                } else {
                    return lam_make_error(env->vm, ImportNotFound, result.msg);
                }
            } else {
                lam_value m = it->second;
                env->bind(modname->val(), m);
                return m;
            }
        });

    ret->bind_operative(
        // (quote expr) Return expr without evaluating it
        "$quote", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            return a[0];
        });

    ret->bind_operative(
        // (begin expr...) Sequentially evaluate expressions, result is the last evaluation.
        // Though 'begin' appears like an applicative, the the implementation is
        // operative to get an opportunity to implement tail call optimization.
        "begin", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            for (size_t i = 0; i < n - 1; ++i) {
                lam_eval(a[i], env);
            }
            return {a[n - 1], env};  // tail call
        });

    ret->bind_operative(
        // ($let (name0 val0 name1.. val1..) expr) Evaluate expr with the binding of pairs
        // ($let (name0 val0 name1.. val1..)) Bind pairs in the current environment
        "$let", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            lam_env* inner = (n == 1) ? env : lam_new_env(env->vm, env, nullptr);
            lam_list* locals = a[0].as_list();
            assert(locals);
            assert(locals->len % 2 == 0);
            for (size_t i = 0; i < locals->len; i += 2) {
                auto k = locals->at(i + 0);
                auto s = k.as_symbol();
                assert(s);
                auto v = lam_eval(locals->at(i + 1), inner);
                inner->bind(s->val(), v);
            }

            if (n == 1) {
                return lam_make_null();
            } else {
                for (size_t i = 1; i < n - 1; ++i) {
                    lam_eval(a[i], env);
                }
                return {a[n - 1], inner};  // tail call
            }
        });

    ret->bind_applicative(
        // (eval expr) Evaluate expr in the current environment
        // (eval expr env) Evaluate expr in the environment 'env'
        "eval", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            switch (n) {
                case 1:
                    return lam_eval(a[0], env);
                case 2:
                    return lam_eval(a[0], a[1].as_env());
                default:
                    return lam_make_error(env->vm, WrongNumberOfArguments,
                                          "(eval expr) or (eval expr env)");
            }
        });

    ret->bind_applicative(
        // (getenv) Return the current environment
        "getenv", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 0);
            return lam_make_value(env);
        });

    ret->bind_applicative(
        // (print expr...) Print the given expressions
        "print", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            for (auto i = 0; i < n; ++i) {
                lam_print(env->vm, a[i]);
            }
            return lam_value{};
        });

    ret->bind_applicative(
        "list", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n >= 1);
            return lam_make_list_v(env->vm, a, n);
        });

    ret->bind_applicative(
        "bigint", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 1);
            assert(a[0].type() == lam_type::Int);
            return lam_make_bigint(env->vm, a[0].as_int());
        });

    ret->bind_applicative(
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

    ret->bind_applicative(
        "mapreduce",
        [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 3);
            lam_callable* mapfunc = a[0].as_callable();
            lam_callable* redfunc = a[1].as_callable();
            lam_list* lst = a[2].as_list();
            assert(lst->len >= 1);
            lam_value accum{lam_eval_call(mapfunc, env, lst->first(), 1)};
            for (size_t i = 1; i < lst->len; ++i) {
                lam_value val = lam_eval_call(mapfunc, env, lst->first() + i, 1);
                lam_value args[] = {accum, val};
                accum = lam_eval_call(redfunc, env, args, 2);
            }
            return accum;
        });

    ret->bind_applicative(
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
                    return lam_make_bigint(env->vm, r);
                }

                case combine_numeric_types(lam_type::BigInt, lam_type::Double):
                    assert(false);
                    break;
                case combine_numeric_types(lam_type::BigInt, lam_type::Int): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul_si(r, x.as_bigint()->mp, y.as_int());
                    return lam_make_bigint(env->vm, r);
                } break;
                case combine_numeric_types(lam_type::BigInt, lam_type::BigInt): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_mul(r, x.as_bigint()->mp, y.as_bigint()->mp);
                    return lam_make_bigint(env->vm, r);
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

    ret->bind_applicative(
        "+", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            switch (lam_coerce_numeric_types(a[0], a[1])) {
                case lam_type::Double:
                    return lam_make_double(a[0].dval + a[1].dval);
                case lam_type::Int:
                    return lam_make_int(a[0].as_int() + a[1].as_int());
                default:
                    assert(false);
                    return lam_value{};
            }
        });

    ret->bind_applicative(
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
                    return lam_make_bigint(env->vm, r);
                }
                case combine_numeric_types(lam_type::BigInt, lam_type::BigInt): {
                    mpz_t r;
                    mpz_init(r);
                    mpz_sub(r, x.as_bigint()->mp, y.as_bigint()->mp);
                    return lam_make_bigint(env->vm, r);
                }
                default:
                    assert(false);
            }
            return lam_value{};
        });

    ret->bind_applicative(
        "/", [](lam_callable* call, lam_env* env, auto a, auto n) -> lam_value_or_tail_call {
            assert(n == 2);
            assert(a[0].type() == lam_type::Double);  // TODO
            assert(a[1].type() == lam_type::Double);
            return lam_make_double(a[0].dval / a[1].dval);
        });

    ret->bind_applicative(
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

    ret->bind("null", lam_make_null());
    ret->seal();
    return lam_new_env(vm, ret, nullptr);
}

static lam_value lam_eval(lam_value val, lam_env* env) {
    // TODO    ugc_collect(&instance.gc);
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
                        lam_callable* callable = head.as_callable();

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

lam_value lam_eval(lam_vm* vm, lam_value val) {
    return lam_eval(val, vm->root);
}

static void lam_ugc_visit(ugc_t* gc, ugc_header_t* header) {
    if (header == nullptr) {
        assert(gc->userdata == gc);
        auto vm = reinterpret_cast<lam_vm*>(gc);
        if (auto r = vm->root) {
            ugc_visit(gc, &r->header);
        }
        for (auto&& m : vm->imports) {
            if (lam_obj* o = m.second.obj_cast_value()) {
                ugc_visit(gc, &o->header);
            }
        }
    } else {
        static_assert(offsetof(lam_obj, header) == 0);
        lam_obj* obj = reinterpret_cast<lam_obj*>(header);
        switch (obj->type) {
            case lam_type::BigInt:
            case lam_type::Symbol:
            case lam_type::String:
            case lam_type::Error:
                break;
            case lam_type::List: {
                auto lst = static_cast<lam_list*>(obj);
                for (lam_u64 i = 0; i < lst->len; ++i) {
                    lam_value v = lst->at(i);
                    if (auto o = v.obj_cast_value()) {
                        ugc_visit(gc, &o->header);
                    }
                }
                break;
            }
            case lam_type::Operative:
            case lam_type::Applicative: {
                auto call = static_cast<lam_callable*>(obj);
                if (auto e = call->env) {
                    ugc_visit(gc, &e->header);
                }
                if (auto o = call->body.obj_cast_value()) {
                    ugc_visit(gc, &o->header);
                }
                break;
            }
            case lam_type::Environment: {
                auto env = static_cast<lam_env_impl*>(obj);
                if (env->_parent) {
                    ugc_visit(gc, &env->_parent->header);
                }
                for (auto kv : env->_map) {
                    if (auto o = kv.second.obj_cast_value()) {
                        ugc_visit(gc, &o->header);
                    }
                }
                break;
            }
            default:
                lam_debugbreak();
                break;
        }
    }
}

static void lam_ugc_free(ugc_t* gc, ugc_header_t* gobj) {
    static_assert(offsetof(lam_obj, header) == 0);
    auto obj = reinterpret_cast<lam_obj*>(gobj);
    auto vm = reinterpret_cast<lam_vm*>(gc->userdata);
    vm->gc_stats.free_count += 1;
    switch (obj->type) {
        case lam_type::Environment:
            static_cast<lam_env_impl*>(obj)->~lam_env_impl();
            break;
        case lam_type::BigInt:
            mpz_clear(static_cast<lam_bigint*>(obj)->mp);
            break;
    }
    vm->hooks->mem_free(gobj);
}

lam_result lam_vm_import(lam_vm* vm, const char* name, const void* data, size_t len) {
    const char* next = nullptr;
    auto cur = static_cast<const char*>(data);
    auto end = static_cast<const char*>(data) + len;
    lam_env* env = lam_new_env(vm, vm->root, name);

    while (cur < end) {
        lam_result res = lam_parse(vm, cur, end, &next);
        if (res.code != 0) {
            return res;
        }
        lam_value v = lam_eval(res.value, env);
        cur = next;
    }
    lam_value mod = lam_make_value(env);
    vm->root->bind(name, mod);
    return lam_result::ok(mod);
}

lam_vm* lam_vm_new(lam_hooks* hooks) {
    hooks->init();
    void* addr = hooks->mem_alloc(sizeof(lam_vm));
    lam_vm* vm = new (addr) lam_vm;
    ugc_init(&vm->gc, &lam_ugc_visit, &lam_ugc_free);
    vm->gc.userdata = vm;
    vm->hooks = hooks;
    vm->root = lam_make_env_builtin(vm);
    return vm;
}

template <typename T>
static inline void swap_reset_container(T& t) {
    T e;
    t.swap(e);
}

void lam_vm_delete(lam_vm* vm) {
    // Test: remove garbage
    ugc_collect(&vm->gc);
    // Test: everything else
    vm->root = nullptr;
    swap_reset_container(vm->imports);
    ugc_collect(&vm->gc);
    auto hooks = vm->hooks;
    hooks->mem_free(vm);
    hooks->quit();
}
