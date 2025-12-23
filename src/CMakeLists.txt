
set(SRCS
    littlelambda.cpp
    littlelambda.h
    lam_common.cpp
    lam_common.h
    lam_core.cpp
    lam_core.h
    mini-gmp.h
    mini-gmp.c
    ugc.cpp
    littlelambda.natvis
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(littlelambda STATIC ${SRCS})
target_include_directories(littlelambda PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    set_source_files_properties(mini-gmp.c PROPERTIES COMPILE_FLAGS "/wd4146 /wd4244 /wd4267")
    target_compile_options(littlelambda PUBLIC "/EHsc")
endif()
