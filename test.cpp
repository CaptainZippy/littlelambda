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

    if (1) {
        /* lam_value expr0 = lam_List(
            lam_Sym("begin"),
            lam_List(lam_Sym("define"), lam_Sym("r"), lam_Double(10)),
            lam_List(lam_Sym("*"), lam_Sym("pi"),
                     lam_List(lam_Sym("*"), lam_Sym("r"), lam_Sym("r"))));*/
        lam_value expr = lam_parse("(begin (define r 10) (* pi (* r r)))");
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 314);
    }

    if (1) {
        /*lam_value expr = lam_List(
            lam_Sym("begin"),
            lam_List(lam_Sym("define"), lam_Sym("circle-area"),
                     lam_List(lam_Sym("lambda"), lam_List(lam_Sym("r")),
                              lam_List(lam_Sym("*"), lam_Sym("pi"),
                                       lam_List(lam_Sym("*"), lam_Sym("r"),
                                                lam_Sym("r"))))),
            lam_List(lam_Sym("circle-area"), lam_Double(3)));*/
        lam_value expr =
            lam_parse("(begin (define circle-area (lambda (r) (* pi (* r r)))) (circle-area 3))");

        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (1) {
        /* lam_value expr = lam_List(
            lam_Sym("begin"),
            lam_List(
                lam_Sym("define"), lam_List(lam_Sym("fact"), lam_Sym("n")),
                lam_List(lam_Sym("if"),
                         lam_List(lam_Sym("<="), lam_Sym("n"), lam_Int(1)),
                         lam_Int(1),
                         lam_List(lam_Sym("*"), lam_Sym("n"),
                                  lam_List(lam_Sym("fact"),
                                           lam_List(lam_Sym("-"), lam_Sym("n"),
                                                    lam_Int(1)))))),
            lam_List(lam_Sym("fact"), lam_Int(5)));*/
        lam_value expr = lam_parse(
            "(begin"
            "   (define (fact n) (if (<= n 1) 1 (* n (fact (- n 1))) ))"
            "   (fact 5))");

        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.as_int() == 120);
    }

    return 0;
}
