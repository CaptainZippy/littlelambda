cmake_minimum_required(VERSION 3.22)
project(littlelambda)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRCS
    README.md
    src/test.cpp
    src/littlelambda.cpp
    src/littlelambda.h
    src/mini-gmp.h
    src/mini-gmp.c
    src/ugc.cpp
    test/01-Basic.ll
    test/math.ll
    test/module.ll
    test/test.ll
    ugc/ugc.h
    .vscode/littlelambda.natvis)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})
add_executable (littlelambda ${SRCS})

if(MSVC)
    set_source_files_properties(mini-gmp.c PROPERTIES COMPILE_FLAGS "/wd4146 /wd4244 /wd4267")
    set_target_properties(littlelambda PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
    target_compile_options(littlelambda PUBLIC "/EHsc")
endif()

enable_testing()
add_test(NAME Simple
    COMMAND $<TARGET_FILE:littlelambda>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)

