#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
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
    buf.push_back(0);
    return 0;
}

static lam_result read_and_eval(const char* path);

struct lam_hooks_impl : lam_hooks {
    lam_hooks_impl() { import = &do_import; }
    static lam_result do_import(lam_hooks* hooks, const char* p) {
        auto self = static_cast<lam_hooks_impl*>(hooks);
        auto v = self->_imports[p];
        if (v.type() != lam_type::Environment) {
            auto p2 = std::string{p} + ".ll";
            auto res = read_and_eval(p2.c_str());
            if (res.code != 0) {
                return res;
            }
            v = res.value;
            self->_imports[p] = res.value;
        }
        return lam_result::ok(v);
    }
    std::unordered_map<std::string, lam_value> _imports;
};

lam_env* lam_make_env_default() {
    auto hooks = new lam_hooks_impl{};
    return lam_make_env_builtin(hooks);
}

lam_result read_and_eval(const char* path) {
    // TODO: accept path of loading module, accept environment, split out finding module
    std::vector<char> buf;
    if (slurp_file(path, buf) == 0) {
        auto env = lam_make_env_default();

        for (const char* cur = buf.data(); *cur;) {
            const char* next = nullptr;
            lam_result res = lam_parse(cur, &next);
            if (res.code != 0) {
                return res;
            }
            cur = next;
            lam_value obj = lam_eval(res.value, env);
        }
        auto v = lam_make_value(reinterpret_cast<lam_obj*>(env));
        return lam_result::ok(v);
    }
    return lam_result::fail(10000, "module not found");
}

static lam_value lam_parse_or_die(const char* input) {
    lam_result res = lam_parse(input);
    assert(res.code == 0);
    return res.value;
}

int main() {
    read_and_eval("module.ll");
    read_and_eval("test.ll");
    if (1) {
        lam_parse("hello");
        lam_parse("\"world\"");
        lam_parse("12");
        lam_parse("12.2");
        lam_parse("(hello world)");
        lam_parse("(hello (* num 141.0) world)");
        lam_parse("(begin ($define r 10) (* 3.4 (* r r)))");
        lam_parse("(begin ($define r null) (print r))");
    }

    read_and_eval("01-Basic.ll");
    if (0) {
        lam_value expr = lam_parse_or_die("(begin ($define r 10) (* 3.1 (* r r)))");
        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 314);
    }

    if (1) {
        lam_value expr0a = lam_parse_or_die(
            "($module bar\n"
            "   ($define (area x y) (* x y))\n"
            "   ($define (perim x y) (* 2 (+ x y))))");
        lam_value expr0b = lam_parse_or_die(
            "($module bar .)\n"
            "($define (area x y) (* x y))\n"
            "($define (perim x y) (* 2 (+ x y)))");

        lam_value expr1a = lam_parse_or_die(
            "($module foo\n"
            "   ($define (area x y)\n"
            "       (if (= x 0) 0 .)\n"
            "       (* x y))\n"
            "   ($define (perim x y) (* 2 (+ x y))))");
        lam_print(expr1a, "\n");
        lam_value expr1b = lam_parse_or_die(
            "($module foo .)\n"
            "($define (area x y)\n"
            "   (if (= x 0) 0 .)\n"
            "   (* x y))\n"
            "($define (perim x y) (* 2 (+ x y)))");
        lam_print(expr1b, "\n");
    }

    if (1) {
        lam_env* env = lam_make_env_default();
        lam_value op = lam_make_opaque(22);
        env->bind("val", op);
        lam_print(op, "\n");
        lam_value expr = lam_parse_or_die("(print val \"\\n\")");
        lam_eval(expr, env);
    }

    if (1) {
        lam_value expr0 = lam_parse_or_die("($let (a 10 b 20) (print a b \"\\n\"))");
        lam_env* env = lam_make_env_default();
        lam_eval(expr0, env);
        assert(env->lookup("a").as_error());
        lam_value expr1 = lam_parse_or_die(
            "(begin\n"
            "   ($let (c 30 d 40))\n"
            "   (print c d \"\\n\"))");
        lam_eval(expr1, env);
        assert(env->lookup("c").as_int() == 30);
    }

    if (1) {
        lam_value expr = lam_parse_or_die(
            "(begin"
            " ($import math)"
            " ($import codepoint)"
            " ($define nl codepoint.newline)"
            " ($define (area r) (* math.pi (* r r)))"
            " ($define r 10)"
            " (print ($let (r 20) (area r)) codepoint.newline)"
            " (print (area r) nl)"
            ")");
        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval == 0);
    }

    if (0) {
        lam_value expr = lam_parse_or_die(
            "(begin"
            " ($import math)"
            " ($define (circle-area r) (* pi (* r r)))"
            " (circle-area 3)"
            ")");

        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (1) {
        lam_value expr = lam_parse_or_die(
            "(begin"
            "   ($define (fact n) ($if (<= n 1) 1 (* n (fact (- n 1))) ))"
            "   (fact (bigint 35)))");

        lam_env* env = lam_make_env_default();
        for (int i = 0; i < 1; ++i) {
            lam_value obj = lam_eval(expr, env);
            assert(obj.type() == lam_type::BigInt);
            printf("%s\n", obj.as_bigint()->str());
        }
    }

    if (1) {
        lam_value expr = lam_parse_or_die(
            "(begin"
            "   ($define (twice x) (* 2 x))"
            "   ($define repeat ($lambda (f) ($lambda (x) (f (f x)))))"
            "   ((repeat twice) 10)"
            ")");
        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 40);

        expr = lam_parse_or_die("((repeat (repeat twice)) 10)");
        obj = lam_eval(expr, env);
        assert(obj.as_int() == 160);
    }

    if (0) {
        lam_value expr = lam_parse_or_die(
            "($define (range a b) ($if (= a b) ($quote()) (cons a (range(+ a 1) b))))"
            //"($define range (a b) (list-expr (+ a i) i (enumerate (- b a))"
            "(range 0 10)");
    }

    if (1) {
        lam_value expr = lam_parse_or_die(
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
        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 4);
    }

    if (1) {
        lam_value expr = lam_parse_or_die(
            //"($define (count item L) ($if L (+ (equal? item (first L)) (count item (rest L))) 0))"
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
        lam_env* env = lam_make_env_default();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 303);
    }

    return 0;
}
