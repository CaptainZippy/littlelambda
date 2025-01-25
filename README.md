
Experiment based on https://norvig.com/lispy.html and http://norvig.com/lispy2.html

Features:
 * Written in a simple subset of C++.
 * NaN boxing. Basic types such as int/double are 64 bits and do not go to the heap.
 * Lists, symbols, functions etc are heap allocated.
 * Bigints via mini-gmp
 * Tail recursion optimization
 * First class environments
 * Supports kernel-style operatives (calls where all the arguments are passed unevaluated)
 * Garbage collection based on github.com/bullno1/ugc

TODO:
 * Optimize function calls via a stack

 [![CMake](https://github.com/CaptainZippy/littlelambda/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/CaptainZippy/littlelambda/actions/workflows/cmake-multi-platform.yml)
 [![CodeQL](https://github.com/CaptainZippy/littlelambda/actions/workflows/codeql.yml/badge.svg)](https://github.com/CaptainZippy/littlelambda/actions/workflows/codeql.yml)
