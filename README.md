
Experiment based on https://norvig.com/lispy.html and http://norvig.com/lispy2.html

Features:
 * Written in a simple subset of C++.
 * NaN boxing. Basic types such as int/double are 64 bits and do not go to the heap.
 * Lists, symbols, functions etc are heap allocated.
 * Tail recursion optimization

Missing features:
 * Garbage collection (but see littlegc)

 [![CMake](https://github.com/CaptainZippy/littlelambda/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/CaptainZippy/littlelambda/actions/workflows/cmake-multi-platform.yml)
 [![CodeQL](https://github.com/CaptainZippy/littlelambda/actions/workflows/codeql.yml/badge.svg)](https://github.com/CaptainZippy/littlelambda/actions/workflows/codeql.yml)
