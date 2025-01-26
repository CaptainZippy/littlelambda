#pragma once

#include "lam_common.h"

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

/// Evaluate the top of the stack.
lila_result lila_eval(lila_vm* vm, int idx);

/// Pop n values from the stack.
lila_result lila_pop(lila_vm* vm, int n);

/// Print the value at stack[index].
/// Optionally supply a string to print at the end.
void lila_print(lila_vm* vm, int index, const char* end = nullptr);

/// Push the opaque value on top of the stack.
lila_result lila_push_opaque(lila_vm* vm, unsigned long long u);

/// 
double lila_tonumber(lila_vm* vm, int index);
/// 
int lila_tointeger(lila_vm* vm, int index);

bool lila_isnull(lila_vm* vm, int index);

/// Destroy a VM.
void lila_vm_delete(lila_vm* vm);
