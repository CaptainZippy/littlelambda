#pragma once
// Public interfaces.

struct lila_vm;

enum class lila_result : int {
    Ok = 0,
    Fail = -1,
    FileNotFound = -2,
};

struct lila_hooks {
    virtual ~lila_hooks();
    virtual void* mem_alloc(size_t size) = 0;
    virtual void mem_free(void* addr) = 0;
    virtual void init() = 0;
    virtual void quit() = 0;
    virtual void output(const char* s, size_t n) = 0;
    virtual lila_result import(lila_vm* vm, const char* modname) = 0;
};

/// Initialize a new vm.
lila_vm* lila_vm_new(lila_hooks* hooks);

/// Import a module with the given name and contents (sans-io).
lila_result lila_vm_import(lila_vm* vm, const char* name, const void* data, size_t len);

/// Parse one statement from the input. On success,
/// * the statement is placed on top of the stack
/// * the 'restart' pointer is set past the input consumed
/// * the return value is 0
/// On failure, ???
/// Call this multiple times to consume all input.
lila_result lila_parse(lila_vm* vm, const char* input, const char* end, const char** restart);

/// Evaluate stack[idx] and replace it with the evaluation.
lila_result lila_eval(lila_vm* vm, int idx);

/// Pop n values from the stack.
lila_result lila_pop(lila_vm* vm, int n);

/// Print the value at stack[index].
/// Optionally supply a string to print at the end.
void lila_print(lila_vm* vm, int index, const char* end = nullptr);

/// Push the opaque value on top of the stack.
lila_result lila_push_opaque(lila_vm* vm, unsigned long long u);

/// Push the symbol value on top of the stack.
lila_result lila_push_symbol(lila_vm* vm, const char* sym);

/// Push the integer value on top of the stack.
lila_result lila_push_integer(lila_vm* vm, int val);

enum class lila_type : unsigned char {
    Null = 0,
    Double,
    Int,
    Opaque,
    BigInt,
    String,
    Symbol,
    List,
    Applicative,
    Operative,
    Environment,
    Error,
};

struct lila_value {
    lila_type type;
    union {
        double number;
        int integer;
        unsigned long long opaque;
        const char* string;
        const char* symbol;
    };
};

/// Map assignment: stack[index][k] = v
/// Assuming k=stack[-2], v=stack[-1], and stack[index] is a map
/// Pops both the key and value from the stack.
lila_result lila_setmap(lila_vm* vm, int index);

/// Map fetch: Gets stack[index][k]
/// Assuming k=stack[-1], and stack[index] is a map
/// Pops the key from the stack and pushes the value or error.
lila_result lila_getmap(lila_vm* vm, int index);

/// Function call
/// Call a function with 'narg' arguments, returning 'nres' values.
/// The function is pushed first (stack[-narg-1]), then arguments
/// left to right. On return, the function and arguments are replaced
/// by the return values.
lila_result lila_call(lila_vm* vm, int narg, int nres);


/// Peek at the value at stack[index].
/// The value is only valid until the next mutation.
lila_value lila_peekstack(lila_vm* vm, int index);

///
double lila_tonumber(lila_vm* vm, int index);
///
int lila_tointeger(lila_vm* vm, int index);
///
bool lila_isnull(lila_vm* vm, int index);

/// Destroy a VM.
void lila_vm_delete(lila_vm* vm);
