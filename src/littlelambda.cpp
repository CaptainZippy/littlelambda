#include "littlelambda.h"
#include "lam_core.h"

lila_hooks::~lila_hooks() = default;

lila_result lila_vm_import(lila_vm* vm, const char* name, const void* data, size_t len) {
    const char* next = nullptr;
    auto cur = static_cast<const char*>(data);
    auto end = static_cast<const char*>(data) + len;
    lam_env* env = lam_new_env(vm, vm->root, name);

    while (cur < end) {
        lam_result res = lam_parse(vm, cur, end, &next);
        if (res.code != 0) {
            return lila_result::Fail;
        }
        lam_value v = lam_eval(res.value, env);
        cur = next;
    }
    lam_value mod = lam_make_value(env);
    vm->root->bind(name, mod);
    vm->stack.push_back(mod);
    return lila_result::Ok;
}

lila_vm* lila_vm_new(lila_hooks* hooks) {
    hooks->init();
    void* addr = hooks->mem_alloc(sizeof(lila_vm));
    lila_vm* vm = new (addr) lila_vm;
    ugc_init(&vm->gc, &lam_ugc_visit, &lam_ugc_free);
    vm->gc.userdata = vm;
    vm->hooks = hooks;
    vm->root = lam_make_env_builtin(vm);
    return vm;
}

template <typename T>
static inline void swap_reset_container(T& t) {
    T e;
    t.swap(e);
}

lila_result lila_parse(lila_vm* vm, const char* input, const char* end, const char** restart) {
    lam_result res = lam_parse(vm, input, end, restart);
    if (res.code != 0) {
        return lila_result::Fail;
    }
    vm->stack.push_back(res.value);
    return lila_result::Ok;
}

lila_result lila_eval(lila_vm* vm, int index) {
    lam_value val = lam_eval(vm, vm->stack[index]);
    vm->stack[index] = val;
    return lila_result::Ok;
}

lila_result lila_pop(lila_vm* vm, int n) {
    assert(n >= 0);
    if (n > vm->stack.size()) {
        return lila_result::Fail;
    }
    vm->stack.resize(vm->stack.size() - n);
    return lila_result::Ok;
}

lila_result lila_push_opaque(lila_vm* vm, unsigned long long u) {
    vm->stack.push_back(lam_make_opaque(u));
    return lila_result::Ok;
}

void lila_print(lila_vm* vm, int index, const char* end) {
    lam_print(vm, vm->stack[index], end);
}

double lila_tonumber(lila_vm* vm, int index) {
    const lam_value& v = vm->stack[index];
    assert(v.type() == lam_type::Double);
    return v.as_double();
}

int lila_tointeger(lila_vm* vm, int index) {
    const lam_value& v = vm->stack[index];
    assert(v.type() == lam_type::Int);
    return v.as_int();
}

bool lila_isnull(lila_vm* vm, int index) {
    const lam_value& v = vm->stack[index];
    return v.type() == lam_type::Null;
}

void lila_vm_delete(lila_vm* vm) {
    vm->stack.resize(0);
    // Test: remove garbage
    ugc_collect(&vm->gc);
    // Test: everything else
    vm->root = nullptr;
    swap_reset_container(vm->imports);
    ugc_collect(&vm->gc);
    auto hooks = vm->hooks;
    hooks->mem_free(vm);
    hooks->quit();
}
