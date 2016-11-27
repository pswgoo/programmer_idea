set (SRCS
	lexer.cpp
	lexer.h
	#symbol_table.cpp
	#symbol_table.h
	#cl_parser.cpp
	#cl_parser.h
	parser.cpp
	parser.h
	instruction.h
	symbol.cpp
	symbol.h
)

add_source_groups(${SRCS})
add_library(compiler_basic STATIC ${SRCS})
