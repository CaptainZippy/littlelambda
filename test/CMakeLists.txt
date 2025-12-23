
set(SRCS
    test.cpp
    01-Basic.ll
    math.ll
    module.ll
    test.ll
)

add_executable (lam_test ${SRCS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

target_link_libraries(lam_test littlelambda)

if(MSVC)
    set_target_properties(lam_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

enable_testing()
add_test(NAME Simple
    COMMAND $<TARGET_FILE:lam_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

