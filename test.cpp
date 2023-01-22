#include <cassert>
#include <vector>
#include "littlelambda.h"

int main() {
    if (0) {
        //['begin', ['define', 'r', 10], ['*', 'pi', ['*', 'r', 'r']]]
        lam_value expr = lam_List(
            lam_Sym("begin"),
            lam_List(lam_Sym("define"), lam_Sym("r"), lam_Double(10)),
            lam_List(lam_Sym("*"), lam_Sym("pi"),
                     lam_List(lam_Sym("*"), lam_Sym("r"), lam_Sym("r"))));
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 300);
    }

    if (0) {
        lam_value expr = lam_List(
            lam_Sym("begin"),
            lam_List(lam_Sym("define"), lam_Sym("circle-area"),
                     lam_List(lam_Sym("lambda"), lam_List(lam_Sym("r")),
                              lam_List(lam_Sym("*"), lam_Sym("pi"),
                                       lam_List(lam_Sym("*"), lam_Sym("r"),
                                                lam_Sym("r"))))),
            lam_List(lam_Sym("circle-area"), lam_Double(3)));
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    if (1) {
        // (define (fact n) (if (<= n 1)
        //      1
        //      (* n (fact (- n 1))) ))
        lam_value expr = lam_List(
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
            lam_List(lam_Sym("fact"), lam_Int(5)));
        lam_env* env = lam_env::builtin();
        lam_value obj = lam_eval(env, expr);
        assert(obj.dval > 9 * 3.1);
        assert(obj.dval < 9 * 3.2);
    }

    return 0;
}
