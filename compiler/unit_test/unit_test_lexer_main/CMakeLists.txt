set (SRCS
	unit_test_lexer_main.cpp
)

add_source_groups(${SRCS})
add_executable(unit_test_lexer_main ${SRCS})

target_link_libraries(unit_test_lexer_main
	compiler_basic
)