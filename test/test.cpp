#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>
#if __cpp_lib_stacktrace >= 202011L
#include <stacktrace>  // C++23 feature
#else
namespace std {
struct stacktrace_entry {
    std::string description() { return ""; }
};
struct stacktrace {
    static stacktrace current() { return {}; }
    struct stacktrace_entry* begin() { return nullptr; }
    stacktrace_entry* end() { return nullptr; }
};
}  // namespace std
#endif
#include "littlelambda.h"

static int slurp_file(const char* path, std::vector<char>& buf) {
    buf.clear();
    FILE* fin = fopen(path, "rb");
    if (fin == nullptr) {
        return -1;
    }
    char b[4096];
    while (size_t n = fread(b, 1, sizeof(b), fin)) {
        buf.insert(buf.end(), &b[0], &b[n]);
    }
    fclose(fin);
    return 0;
}

lila_result read_and_eval(lila_vm* vm, const char* path) {
    // TODO: accept path of loading module, accept environment, split out finding module
    std::vector<char> buf;
    if (slurp_file(path, buf) == 0) {
        const char* bufEnd = buf.data() + buf.size();
        for (const char* cur = buf.data(); cur < bufEnd;) {
            const char* next = nullptr;
            lila_result res = lila_parse(vm, cur, bufEnd, &next);
            if (res != lila_result::Ok) {
                return res;
            }
            cur = next;
            res = lila_eval(vm, -1);
            if (res != lila_result::Ok) {
                return res;
            }
        }
        return lila_result::Ok;
    }
    return lila_result::FileNotFound;
}

// Puts the parsed result on top of stack
static lila_result _lila_parse_or_die(lila_vm* vm, const char* input, int N) {
    const char* restart = nullptr;
    const char* end = input + N;
    auto r = lila_parse(vm, input, end, &restart);
    assert2(restart == end, "Input was not consumed");
    assert(r == lila_result::Ok);
    return r;
}

template <int N>
static inline lila_result lila_parse_or_die(lila_vm* vm, const char (&input)[N]) {
    return _lila_parse_or_die(vm, input, N - 1);  // not null terminator
}

static lila_result import_impl(lila_vm* vm, const char* modname) {
    std::string path = std::format("{}.ll", modname);
    std::vector<char> buf;
    if (slurp_file(path.c_str(), buf) == 0) {
        return lila_vm_import(vm, modname, buf.data(), buf.size());
    }
    return lila_result::FileNotFound;
}

struct DebugHooks : lila_hooks {
    DebugHooks() {}

    void* mem_alloc(size_t size) override {
        void* p = malloc(size);
        assert(_allocs.find(p) == _allocs.end());
        _allocs[p] = std::stacktrace::current();
        return p;
    }
    void mem_free(void* addr) override {
        auto it = _allocs.find(addr);
        assert(it != _allocs.end());
        _allocs.erase(it);
        free(addr);
    }
    void init() override {
        assert(_allocs.empty());
        _allocs.clear();
    }
    void quit() override {
        for (auto it : _allocs) {
            printf("%p\n", it.first);
            for (auto p : it.second) {
                printf("\t'%s'\n", p.description().c_str());
            }
        }
        _allocs.clear();
    }
    void output(const char* s, size_t n) { fwrite(s, 1, n, stdout); }
    lila_result import(lila_vm* vm, const char* modname) override { return import_impl(vm, modname); }

    std::unordered_map<void*, std::stacktrace> _allocs;
};

struct SimpleHooks : lila_hooks {
    int _nalloc{};
    virtual void* mem_alloc(size_t size) {
        _nalloc += 1;
        return malloc(size);
    }
    void mem_free(void* addr) {
        free(addr);
        _nalloc -= 1;
    }
    void init() { _nalloc = 0; }
    void quit() { assert(_nalloc == 0); }
    lila_result import(lila_vm* vm, const char* modname) override { return import_impl(vm, modname); }
    void output(const char* s, size_t n) { fwrite(s, 1, n, stdout); }
};

void test_all(lila_hooks& hooks) {
    // Basic parsing tests
    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, "hello");
        lila_parse_or_die(vm, "\"world\"");
        lila_parse_or_die(vm, "12");
        lila_parse_or_die(vm, "12.2");
        lila_parse_or_die(vm, "(hello world)");
        lila_parse_or_die(vm, "(hello (* num 141.0) world)");
        lila_parse_or_die(vm, "(begin ($define r 10) (* 3.4 (* r r)))");
        lila_parse_or_die(vm, "(begin ($define r null) (print r))");
        lila_vm_delete(vm);
    }

    // Printing & trailing "." syntax
    // (foo a .) (b c) (d e f) is equivalent to (foo a (b c) (d e f))
    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            ($module bar
                ($define (area x y) (* x y))
                ($define (perim x y) (* 2 (+ x y))))
        )---");
        lila_pop(vm, 1);
        lila_parse_or_die(vm, R"---(
            ($module bar .)
            ($define (area x y) (* x y))
            ($define (perim x y) (* 2 (+ x y)))
        )---");
        lila_pop(vm, 1);
        lila_parse_or_die(vm, R"---(
            ($module foo
               ($define (area x y)
                   (if (= x 0) 0 .) (* x y))
               ($define (perim x y) (* 2 (+ x y))))
        )---");
        lila_print(vm, -1, "\n");
        lila_parse_or_die(vm, R"---(
            ($module foo .)
            ($define (area x y)
                (if (= x 0) .) 0 (* x y))
            ($define (perim x y) (* 2 (+ x y)))
        )---");
        lila_print(vm, -1, "\n");
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        read_and_eval(vm, "module.ll");
        read_and_eval(vm, "test.ll");
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        read_and_eval(vm, "01-Basic.ll");
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, "(begin ($define r 10) (* 3.1415 (* r r)))");
        lila_eval(vm, -1);
        double d = lila_tonumber(vm, -1);
        assert(d > 314);
        lila_vm_delete(vm);
    }

    if (0) {  // missing env->bind call
        lila_vm* vm = lila_vm_new(&hooks);
        lila_push_opaque(vm, 22);
        // TODO env->bind("val", op);
        lila_print(vm, -1, "\n");
        lila_parse_or_die(vm, R"---((print val "\n"))---");
        lila_eval(vm, -1);
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(($let (a 10 b 20) (print a b "\n")))---");
        lila_eval(vm, -1);
        // assert(env->lookup("a").as_error());
        lila_parse_or_die(vm, R"---(
            (begin
                ($let (c 30 d 40))
                (print c d "\n"))
        )---");
        lila_eval(vm, -1);
        // assert(env->lookup("c").as_int() == 30);
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($import math)
            ($import codepoint)
            ($define nl codepoint.newline)
            ($define (area r) (* math.pi (* r r)))
            ($define r 10)
            (print ($let (r 20) (area r)) codepoint.newline)
            (print (area r) nl)
        )---");
        lila_eval(vm, -1);
        assert(lila_isnull(vm,-1));
        lila_vm_delete(vm);
    }

    if (0) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($import math)
            ($define (circle-area r) (* pi (* r r)))
            (circle-area 3)
        )---");
        lila_eval(vm, -1);
		double d = lila_tonumber(vm, -1);
        assert(d > 9 * 3.1);
        assert(d < 9 * 3.2);
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($define (fact n) ($if (<= n 1) 1 (* n (fact (- n 1))) ))
            (fact (bigint 35))
        )---");
        for (int i = 0; i < 1; ++i) {
            lila_eval(vm, -1);
            //TODO assert(obj.type() == lila_type::BigInt);
            //TODO printf("%s\n", obj.as_bigint()->str());
        }
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($define (twice x) (* 2 x))
            ($define repeat ($lambda (f) ($lambda (x) (f (f x)))))
            ((repeat twice) 10)
        )---");
        lila_eval(vm, -1);
        assert(lila_tointeger(vm, -1) == 40);

        lila_parse_or_die(vm, "((repeat (repeat twice)) 10)");
        lila_eval(vm, -1);
        assert(lila_tointeger(vm, -1) == 160);
        lila_vm_delete(vm);
    }

    // List comprehension
    if (0) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($define (range a b) TODO
            (range 0 10)
        )---");
        // ($if (equal? a b) ($quote ()) (cons a (range(+ a 1) b))))
        // ($define range (a b) (list-expr (+ a i) i (enumerate (- b a))"
        lila_eval(vm, -1);
        lila_print(vm, -1, "\n");
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        lila_parse_or_die(vm, R"---(
            (begin .)
            ($define ltest
                ($lambda args
                    (print args)))
            ($define
                (vtest . args)
                (print args))
            (ltest 1 2)
            (ltest 1
                (+ 2 2))
            (vtest 1 2)
            (vtest 1 2
                (+ 3 5))
            ($define
                (curry1 fn arg1)
                ($lambda (x)
                    (fn arg1 x)))
            ($define
                (count item L)
                (mapreduce
                    (curry1 equal? item) + L))
            ($define answer
                (count 0
                    (list 0 1 2 0 3 0 0)))
            (print "cnt=" answer "\n")
            answer
        )---");
        lila_eval(vm, -1);
        assert(lila_tointeger(vm, -1) == 4);
        lila_vm_delete(vm);
    }

    if (1) {
        lila_vm* vm = lila_vm_new(&hooks);
        //"($define (count item L) ($if L (+ (equal? item (first L)) (count item (rest L))) 0))"
        lila_parse_or_die(vm, R"---(
            (begin .)
            (print "hello\n")
            ($define (test_one expr) (begin ($define a 202) (eval expr)))
            ($define (test_two expr) (begin ($define a 999) (eval expr)))
            ($define qfoo ($quote (+ (* 6 7) a)))
            ($define lfoo (list + (* 6 7) ($quote a)))
            (print "qfoo" qfoo "\n")
            (print "lfoo" lfoo "\n")
            (print (test_one qfoo) "\n")
            (print (test_two qfoo) "\n")
            (print (test_one lfoo) "\n")
            (print (test_two lfoo) "\n")

            ($define envp (begin ($define a 10) ($define b 20) (getenv)))
            (print "Y " a b "\n")
            (print "X " envp (eval qfoo envp))

            303
            )---");
        lila_eval(vm, -1);
        assert(lila_tointeger(vm, -1) == 303);
        lila_vm_delete(vm);
    }
}

int main() {
    DebugHooks hooks;
    // SimpleHooks hooks;
    for (int i = 0; i < 100; ++i) {
        test_all(hooks);
    }
    return 0;
}