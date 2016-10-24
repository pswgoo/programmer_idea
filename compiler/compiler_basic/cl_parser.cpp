#include "cl_parser.h"

using namespace std;

namespace pswgoo {

std::unique_ptr<ClAstNode> ClProg1Node::Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent) {
	switch (lexer.Current().type_)
	{
	case KEY_BOOL:
	case KEY_CHAR:
	case KEY_INT:
	case KEY_FLOAT:
	case KEY_DOUBLE:{
		unique_ptr<TypeNode> type_node(new TypeNode(lexer, nullptr));

		break;
	}
	default:
		break;
	}
	return nullptr;
}

} // namespace pswgoo
