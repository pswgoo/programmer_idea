set (SRCS
	unit_test_parser_main.cpp
)

add_source_groups(${SRCS})
add_executable(unit_test_parser_main ${SRCS})

target_link_libraries(unit_test_parser_main
	compiler_basic
)