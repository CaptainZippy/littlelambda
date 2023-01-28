#include <cassert>
#include <vector>
#include "littlelambda.h"

int main() {
    if (0) {
        lam_parse("hello");
        lam_parse("12");
        lam_parse("12.2");
        lam_parse("(hello world)");
        lam_parse("(hello (* num 141.0) world)");
        lam_parse("(begin (define r 10) (* pi (* r r)))");
    }

    if (0) {
        lam_value expr = lam_parse("(begin (define r 10) (* pi (* r r)))");
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 314);
    }

    if (0) {
        lam_value expr = lam_parse(
            "(begin (define (circle-area r) (* pi (* r r)))) (circle-area 3))");

        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (0) {
        lam_value expr = lam_parse(
            "(begin"
            "   (define (fact n) (if (<= n 1) 1 (* n (fact (- n 1))) ))"
            "   (fact 100))");

        lam_env* env = lam_env::builtin();
        for (int i = 0; i < 10000; ++i) {
            lam_value obj = lam_eval(env, expr);
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
            "   ((repeat twice) 12)"
            ")");
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.as_int() == 48);
    }

    return 0;
}
