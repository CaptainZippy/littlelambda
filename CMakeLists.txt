cmake_minimum_required(VERSION 3.24)
project(littlelambda)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    set_source_files_properties(mini-gmp.c PROPERTIES COMPILE_FLAGS "/wd4146 /wd4244 /wd4267")
endif()

set(SRCS test.cpp littlelambda.cpp littlelambda.h mini-gmp.h mini-gmp.c README.md)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})
add_executable (littlelambda ${SRCS})
