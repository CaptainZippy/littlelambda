#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>
#if __cpp_lib_stacktrace >= 202011L
#include <stacktrace> // C++23 feature
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

lam_result read_and_eval(lam_vm* vm, const char* path) {
    // TODO: accept path of loading module, accept environment, split out finding module
    std::vector<char> buf;
    if (slurp_file(path, buf) == 0) {
        const char* bufEnd = buf.data() + buf.size();
        lam_value lastObj{};
        for (const char* cur = buf.data(); cur < bufEnd;) {
            const char* next = nullptr;
            lam_result res = lam_parse(vm, cur, bufEnd, &next);
            if (res.code != 0) {
                return res;
            }
            cur = next;
            lastObj = lam_eval(vm, res.value);
        }
        return lam_result::ok(lastObj);
    }
    return lam_result::fail(10000, "module not found");
}

static lam_value _lam_parse_or_die(lam_vm* vm, const char* input, int N) {
    const char* restart = nullptr;
    const char* end = input + N;
    auto r = lam_parse(vm, input, end, &restart);
    assert2(restart == end, "Input was not consumed");
    assert(r.code == 0);
    return r.value;
}

template<int N>
static lam_value lam_parse_or_die(lam_vm* vm, const char (&input)[N]) {
    return _lam_parse_or_die(vm, input, N-1); // not null terminator
}

static lam_result import_impl(lam_vm* vm, const char* modname) {
    std::string path = std::format("{}.ll", modname);
    std::vector<char> buf;
    if (slurp_file(path.c_str(), buf) == 0) {
        return lam_vm_import(vm, modname, buf.data(), buf.size());
    }
    return lam_result::fail(1, "Import failed");
}

struct DebugHooks : lam_hooks {
    DebugHooks() {}

    void* mem_alloc(size_t size) override {
        void* p = malloc(size);
        assert(allocs.find(p) == allocs.end());
        allocs[p] = std::stacktrace::current();
        return p;
    }
    void mem_free(void* addr) override {
        auto it = allocs.find(addr);
        assert(it != allocs.end());
        allocs.erase(it);
        free(addr);
    }
    void init() override {
        //assert(allocs.empty());
        allocs.clear();
    }
    void quit() override {
        /*for (auto it : allocs) {
            printf("%p\n", it.first);
            for (auto p : it.second) {
                printf("\t%s\n", p.description().c_str());
            }
        }*/
    }
    lam_result import(lam_vm* vm, const char* modname) override { return import_impl(vm, modname); }

    std::unordered_map<void*, std::stacktrace> allocs;
};

int main() {
    DebugHooks hooks;
    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        read_and_eval(vm, "module.ll");
        read_and_eval(vm, "test.ll");
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_parse_or_die(vm, "hello");
        lam_parse_or_die(vm, "\"world\"");
        lam_parse_or_die(vm, "12");
        lam_parse_or_die(vm, "12.2");
        lam_parse_or_die(vm, "(hello world)");
        lam_parse_or_die(vm, "(hello (* num 141.0) world)");
        lam_parse_or_die(vm, "(begin ($define r 10) (* 3.4 (* r r)))");
        lam_parse_or_die(vm, "(begin ($define r null) (print r))");
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        read_and_eval(vm, "01-Basic.ll");
        lam_vm_delete(vm);
    }
    if (0) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr = lam_parse_or_die(vm, "(begin ($define r 10) (* 3.1 (* r r)))");
        lam_value obj = lam_eval(vm, expr);
        assert(obj.dval > 314);
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr0a = lam_parse_or_die(vm,
                                            "($module bar\n"
                                            "   ($define (area x y) (* x y))\n"
                                            "   ($define (perim x y) (* 2 (+ x y))))");
        lam_value expr0b = lam_parse_or_die(vm,
                                            "($module bar .)\n"
                                            "($define (area x y) (* x y))\n"
                                            "($define (perim x y) (* 2 (+ x y)))");

        lam_value expr1a = lam_parse_or_die(vm,
                                            "($module foo\n"
                                            "   ($define (area x y)\n"
                                            "       (if (= x 0) 0 .)\n"
                                            "       (* x y))\n"
                                            "   ($define (perim x y) (* 2 (+ x y))))");
        lam_print(expr1a, "\n");
        lam_value expr1b = lam_parse_or_die(vm,
                                            "($module foo .)\n"
                                            "($define (area x y)\n"
                                            "   (if (= x 0) 0 .)\n"
                                            "   (* x y))\n"
                                            "($define (perim x y) (* 2 (+ x y)))");
        lam_print(expr1b, "\n");
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value op = lam_make_opaque(22);
        //TODO env->bind("val", op);
        lam_print(op, "\n");
        lam_value expr = lam_parse_or_die(vm, "(print val \"\\n\")");
        lam_eval(vm, expr);
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr0 = lam_parse_or_die(vm, "($let (a 10 b 20) (print a b \"\\n\"))");
        lam_eval(vm, expr0);
        //assert(env->lookup("a").as_error());
        lam_value expr1 = lam_parse_or_die(vm,
                                           "(begin\n"
                                           "   ($let (c 30 d 40))\n"
                                           "   (print c d \"\\n\"))");
        lam_eval(vm, expr1);
        //assert(env->lookup("c").as_int() == 30);
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr = lam_parse_or_die(vm,
                                          "(begin"
                                          " ($import math)"
                                          " ($import codepoint)"
                                          " ($define nl codepoint.newline)"
                                          " ($define (area r) (* math.pi (* r r)))"
                                          " ($define r 10)"
                                          " (print ($let (r 20) (area r)) codepoint.newline)"
                                          " (print (area r) nl)"
                                          ")");
        lam_value obj = lam_eval(vm, expr);
        assert(obj.dval == 0);
        lam_vm_delete(vm);
    }

    if (0) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr = lam_parse_or_die(vm,
                                          "(begin"
                                          " ($import math)"
                                          " ($define (circle-area r) (* pi (* r r)))"
                                          " (circle-area 3)"
                                          ")");

        lam_value obj = lam_eval(vm, expr);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr =
            lam_parse_or_die(vm,
                             "(begin"
                             "   ($define (fact n) ($if (<= n 1) 1 (* n (fact (- n 1))) ))"
                             "   (fact (bigint 35)))");

        for (int i = 0; i < 1; ++i) {
            lam_value obj = lam_eval(vm, expr);
            assert(obj.type() == lam_type::BigInt);
            printf("%s\n", obj.as_bigint()->str());
        }
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr =
            lam_parse_or_die(vm,
                             "(begin"
                             "   ($define (twice x) (* 2 x))"
                             "   ($define repeat ($lambda (f) ($lambda (x) (f (f x)))))"
                             "   ((repeat twice) 10)"
                             ")");
        lam_value obj = lam_eval(vm, expr);
        assert(obj.as_int() == 40);

        expr = lam_parse_or_die(vm, "((repeat (repeat twice)) 10)");
        obj = lam_eval(vm, expr);
        assert(obj.as_int() == 160);
        lam_vm_delete(vm);
    }

    if (0) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr = lam_parse_or_die(
            vm,
            "($define (range a b) ($if (= a b) ($quote()) (cons a (range(+ a 1) b))))"
            //"($define range (a b) (list-expr (+ a i) i (enumerate (- b a))"
            "(range 0 10)");
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        lam_value expr = lam_parse_or_die(
            vm,
            //"($define (count item L) ($if L (+ (equal? item (first L)) (count item (rest L))) 0))"
            "(begin"
            "  ($define ltest ($lambda args (print args)))"
            "  ($define (vtest . args) (print args))"
            "  (ltest 1 2)"
            "  (ltest 1 (+ 2 2))"
            "  (vtest 1 2)"
            "  (vtest 1 2 (+ 3 5))"
            //"  (macro (curry a b) ($lambda (x) (a b x)))"
            "  ($define (count item L)"
            "    (mapreduce ($lambda (x) (equal? item x))"
            //"    (mapreduce (curry equal? item)"
            "             + L))"
            "  (count 0 (list 0 1 2 0 3 0 0)))");
        lam_value obj = lam_eval(vm, expr);
        assert(obj.as_int() == 4);
        lam_vm_delete(vm);
    }

    if (1) {
        lam_vm* vm = lam_vm_new(&hooks);
        //"($define (count item L) ($if L (+ (equal? item (first L)) (count item (rest L))) 0))"
        lam_value expr = lam_parse_or_die(vm,
                                          R"---(
            (begin
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

                303)
            )---");
        lam_value obj = lam_eval(vm, expr);
        assert(obj.as_int() == 303);
        lam_vm_delete(vm);
    }

    return 0;
}
