
Experiment based on https://norvig.com/lispy.html

This differs from classic lisp which is based on lists+cons cells.
In this verison, lists are arrays. Basic value types (int/double) do not have their own memory, only lists, symbols, functions have their own allocations.

Features:
 * Simple subset of C++.
 * NaN boxing.
 * No GC overhead for int/double types.