#include <vector>
#include "littlelambda.h"

int main() {
    if (1) {
        lam_parse("hello");
        lam_parse("12");
        lam_parse("12.2");
        lam_parse("(hello world)");
        lam_parse("(hello (* num 141.0) world)");
        lam_parse("(begin (define r 10) (* pi (* r r)))");
    }

    if (1) {
        lam_value expr = lam_parse("(begin (define r 10) (* pi (* r r)))");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 314);
    }

    if (1) {
        lam_value expr =
            lam_parse("(begin (define (circle-area r) (* pi (* r r))) (circle-area 3))");

        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (0) {
        lam_value expr = lam_parse(
            "(begin"
            "   (define (fact n) (if (<= n 1) 1 (* n (fact (- n 1))) ))"
            "   (fact 5))");

        lam_env* env = lam_make_env_builtin();
        for (int i = 0; i < 10000; ++i) {
            lam_value obj = lam_eval(expr, env);
            assert(obj.as_int() == 120);
            if (i % 100 == 0)
                printf("%i\n", i);
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
            "(define range (a b) (if (= a b) (quote()) (cons a (range(+ a 1) b))))"
            //"(define range (a b) (list-expr (+ a i) i (enumerate (- b a))"
            "(range 0 10)");
    }
    if (0) {
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
              (print "\n")
              (define (test_one expr) (begin (define a 202) (eval expr)))
              (define (test_two expr) (begin (define a 999) (eval expr)))
              (define foo (quote + 101 a))
              (print (test_one foo) "\n")
              (print (test_two foo) "\n")
              303)
            )---");
        lam_env* env = lam_make_env_builtin();
        lam_value obj = lam_eval(expr, env);
        assert(obj.as_int() == 303);
    }

    return 0;
}
