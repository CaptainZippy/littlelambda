#include <cassert>
#include <vector>
#include "littlelambda.h"

int main() {
    //['begin', ['define', 'r', 10], ['*', 'pi', ['*', 'r', 'r']]]

    {
        lam_value expr = lam_List(
            lam_Sym("begin"),
            lam_List(lam_Sym("define"), lam_Sym("r"), lam_Double(10)),
            lam_List(lam_Sym("*"), lam_Sym("pi"),
                     lam_List(lam_Sym("*"), lam_Sym("r"), lam_Sym("r"))));
        lam_vm_t vm{};
        lam_init(&vm);
        lam_value obj = lam_eval(&vm, expr);
        assert(obj.dval > 300);
    }
    return 0;
}
