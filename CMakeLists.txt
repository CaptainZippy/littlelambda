cmake_minimum_required(VERSION 3.24)
project(littlelambda)
set(CMAKE_CXX_STANDARD 20)

set(SRCS test.cpp littlelambda.cpp littlelambda.h mini-gmp.h mini-gmp.c README.md)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})
add_executable (littlelambda ${SRCS})

if(MSVC)
    set_source_files_properties(mini-gmp.c PROPERTIES COMPILE_FLAGS "/wd4146 /wd4244 /wd4267")
    target_compile_options(littlelambda PUBLIC "/EHsc")
endif()

enable_testing()
add_test(NAME Simple COMMAND $<TARGET_FILE:littlelambda>)
