#define _CRT_SECURE_NO_WARNINGS
#include <vector>
#include <cstdio>
#include "littlelambda.h"

int slurp(const char* path, std::vector<char>& buf) {
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

int main() {
    std::vector<char> buf;
    if (slurp("test.ll", buf) == 0) {
        lam_env* env = lam_make_env_builtin();
        for (const char* cur = buf.data(); *cur;) {
            const char* next = nullptr;
            lam_value expr = lam_parse(cur, &next);
            cur = next;
            lam_value obj = lam_eval(expr, env);
        }
    }
    if (1) {
        lam_parse("hello");
        lam_parse("\"world\"");
        lam_parse("12");
        lam_parse("12.2");
        lam_parse("(hello world)");
        lam_parse("(hello (* num 141.0) world)");
        lam_parse("(begin (define r 10) (* pi (* r r)))");
    }

    if (0) {
        lam_value expr = lam_parse("(begin (define r 10) (* pi (* r r)))");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 314);
    }

    if (1) {
        lam_value expr = lam_parse(
            "(begin"
            " (define (area r) (* pi (* r r)))"
            " (define r 10)"
            " (print (let (r 20) (area r)) \"\\n\")"
            " (print (area r) \"\\n\")"
            ")");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval == 0);
    }

    if (0) {
        lam_value expr =
            lam_parse("(begin (define (circle-area r) (* pi (* r r))) (circle-area 3))");

        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (1) {
        lam_value expr = lam_parse(
            "(begin"
            "   (define (fact n) (if (<= n 1) 1 (* n (fact (- n 1))) ))"
            "   (fact (bigint 35)))");

        lam_env* env = lam_make_env_builtin();
        for (int i = 0; i < 1; ++i) {
            lam_value obj = lam_eval(expr, env);
            assert(obj.type() == lam_type::BigInt);
            printf("%s\n", obj.as_bigint()->str());
        }
    }

    if (1) {
        lam_value expr = lam_parse(
            "(begin"
            "   (define (twice x) (* 2 x))"
            "   (define repeat (lambda (f) (lambda (x) (f (f x)))))"
            "   ((repeat twice) 10)"
            ")");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 40);

        expr = lam_parse("((repeat (repeat twice)) 10)");
        obj = lam_eval(expr, env);
        assert(obj.as_int() == 160);
    }

    if (0) {
        lam_value expr = lam_parse(
            "(define (range a b) (if (= a b) (quote()) (cons a (range(+ a 1) b))))"
            //"(define range (a b) (list-expr (+ a i) i (enumerate (- b a))"
            "(range 0 10)");
    }

    if (1) {
        lam_value expr = lam_parse(
            //"(define (count item L) (if L (+ (equal? item (first L)) (count item (rest L))) 0))"
            "(begin"
            "  (define ltest (lambda args (print args)))"
            "  (define (vtest . args) (print args))"
            "  (ltest 1 2)"
            "  (ltest 1 (+ 2 2))"
            "  (vtest 1 2)"
            "  (vtest 1 2 (+ 3 5))"
            //"  (macro (curry a b) (lambda (x) (a b x)))"
            "  (define (count item L)"
            "    (mapreduce (lambda (x) (equal? item x))"
            //"    (mapreduce (curry equal? item)"
            "             + L))"
            "  (count 0 (list 0 1 2 0 3 0 0)))");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 4);
    }

    if (1) {
        lam_value expr = lam_parse(
            //"(define (count item L) (if L (+ (equal? item (first L)) (count item (rest L))) 0))"
            R"---(
            (begin
                (print "hello\n")
                (define (test_one expr) (begin (define a 202) (eval expr)))
                (define (test_two expr) (begin (define a 999) (eval expr)))
                (define qfoo (quote (+ (* 6 7) a)))
                (define lfoo (list + (* 6 7) (quote a)))
                (print "qfoo" qfoo "\n")
                (print "lfoo" lfoo "\n")
                (print (test_one qfoo) "\n")
                (print (test_two qfoo) "\n")
                (print (test_one lfoo) "\n")
                (print (test_two lfoo) "\n")

                (define envp (begin (define a 10) (define b 20) (getenv)))
                (print "Y " a b "\n")
                (print "X " envp (eval qfoo envp))

                303)
            )---");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 303);
    }

    return 0;
}
