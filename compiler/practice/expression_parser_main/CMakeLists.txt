set (SRCS
	expression_parser_main.cpp
)

add_source_groups(${SRCS})
add_executable(expression_parser_main ${SRCS})

target_link_libraries(expression_parser_main
	compiler_basic
)
