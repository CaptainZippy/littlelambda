#include <cassert>
#include <vector>
#include "littlelambda.h"

int main() {

    {
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

    {
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
            assert(obj.dval > 9*3.1);
            assert(obj.dval < 9*3.2);
    }

    return 0;
}
