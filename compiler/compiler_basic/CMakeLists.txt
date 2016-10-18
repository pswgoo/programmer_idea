set (SRCS
	lexer.cpp
	lexer.h
)

add_source_groups(${SRCS})
add_library(compiler_basic STATIC ${SRCS})
