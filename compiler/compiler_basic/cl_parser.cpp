#include "cl_parser.h"

#include <iostream>
#include <algorithm>
#include <string>

using namespace std;

namespace pswgoo {

const string kEmptyCmdArg = "##";

bool IsPrimeType(TokenType token_type) {
	switch (token_type) {
	case TokenType::KEY_BOOL:
	case TokenType::KEY_CHAR:
	case TokenType::KEY_INT:
	case TokenType::KEY_FLOAT:
	case TokenType::KEY_DOUBLE:
		return true;
	default:
		return false;
	}
}

ClBinaryOpNode::ClBinaryOpNode(ClAstPtr&& lhs, ClAstPtr&& rhs, TokenType token_type, SymbolTable& symbol_table) {
	if (!lhs->value_type_.IsPrimeType() || !lhs->value_type_.IsPrimeType())
		throw runtime_error("ClBinaryOpNode:: binary operator must operate prime type!");

	bool is_assign = false;
	switch (token_type) {
	case TokenType::OP_ADD:
	case TokenType::OP_MINUS:
	case TokenType::OP_PRODUCT:
	case TokenType::OP_DIVIDE:
	case TokenType::OP_MOD:
	case TokenType::OP_BIT_AND:
	case TokenType::OP_BIT_OR:
	case TokenType::OP_BIT_XOR:
		if (MaxType(lhs->value_type_, rhs->value_type_).prime_type() <= VariableType::kBoolean)
			throw runtime_error("ClBinaryOpNode:: boolean and null cannot do some binary operator!");
	case TokenType::OP_LOGICAL_AND:
	case TokenType::OP_LOGICAL_OR:
		value_type_ = MaxType(lhs->value_type_, rhs->value_type_);
		break;
	case TokenType::OP_GREATER:
	case TokenType::OP_LESS:
	case TokenType::OP_EQUAL:
	case TokenType::OP_NOT_EQUAL:
	case TokenType::OP_GREATER_EQUAL:
	case TokenType::OP_LESS_EQUAL:
		value_type_ = VariableType(VariableType::kBoolean);
		break;
	case TokenType::OP_ASSIGN:
	case TokenType::OP_ADD_ASSIGN:
	case TokenType::OP_MINUS_ASSIGN:
	case TokenType::OP_PRODUCT_ASSIGN:
	case TokenType::OP_DIVIDE_ASSIGN:
	case TokenType::OP_MOD_ASSIGN:
		if (lhs->value_type_.is_rvalue())
			throw runtime_error("ClBinaryOpNode:: lhs of ASSIGN OP cannot be rvalue!");
		if (lhs->value_type_.prime_type() < rhs->value_type_.prime_type())
			throw runtime_error("ClBinaryOpNode:: Type narrowed with " + TokenTypeToCmd(token_type));
		if (lhs->value_type_.prime_type() == VariableType::kBoolean && token_type != TokenType::OP_ASSIGN)
			throw runtime_error("ClBinaryOpNode:: boolean can only do assign");
		value_type_ = lhs->value_type_; 
		value_ = lhs->value_;
		is_assign = true;
		break;
	default:
		throw runtime_error("ClBinaryOpNode:: Invalid binary operator!");
		break;
	}

	node_type_ = TokenTypeToCmd(token_type);

	code_.reset(new ClCodeBlock(lhs->code_));
	code_->Add(rhs->code_);
	if (!is_assign) {
		SymbolNode* tmp_node = symbol_table.PutTemp(value_type_.prime_type());
		value_ = tmp_node->name();
		value_type_.set_is_rvalue(true);
		code_->Add({ TokenTypeToCmd(token_type), lhs->value_, rhs->value_, value_ });
	}
	else
		code_->Add({ TokenTypeToCmd(token_type), rhs->value_, kEmptyCmdArg, value_ });

	children_.emplace_back(move(lhs));
	children_.emplace_back(move(rhs));
}

ClUnaryOpNode::ClUnaryOpNode(ClAstPtr && lhs, TokenType token_type, SymbolTable & symbol_table) {
	if (token_type != OP_ADD && token_type != OP_MINUS && token_type != OP_BIT_NOT && token_type != OP_LOGICAL_NOT)
		throw runtime_error("ClUnaryOperatorNode:: Invalid unary operator! " + TokenTypeToCmd(token_type));
	if (lhs->value_type_.IsPrimeType()) {
		node_type_ = TokenTypeToCmd(token_type);
		SymbolNode* tmp_node = symbol_table.PutTemp(lhs->value_type_.prime_type());
		value_ = tmp_node->name();
		value_type_ = tmp_node->type_;
		value_type_.set_is_rvalue(true);
		code_.reset(new ClCodeBlock(lhs->code_));
		code_->Add({ TokenTypeToCmd(token_type), lhs->value_, kEmptyCmdArg, value_ });
		children_.emplace_back(move(lhs));
	}
	else
		throw runtime_error("ClUnaryOperatorNode:: Invalid value_type");
}

ClIncreDecreOpNode::ClIncreDecreOpNode(ClAstPtr&& lhs, TokenType token_type, bool is_suffix, SymbolTable& symbol_table) {
	if (token_type != OP_INCREMENT && token_type != OP_DECREMENT)
		throw runtime_error("ClIncrementDecrementNode:: Invalid IncrementDecrement operator! " + TokenTypeToCmd(token_type));
	else if (lhs->value_type_.is_rvalue() || !lhs->value_type_.IsPrimeType() || lhs->value_type_.prime_type() == VariableType::kBoolean || lhs->value_type_.prime_type() == VariableType::kNull)
		throw runtime_error("ClIncrementDecrementNode:: Invalid value_type");
	
	SymbolNode *tmp_node = symbol_table.PutTemp(lhs->value_type_.prime_type());
	code_.reset(new ClCodeBlock(lhs->code_));
	value_ = tmp_node->name();
	string op = token_type == OP_INCREMENT ? TokenTypeToCmd(OP_ADD) : TokenTypeToCmd(OP_MINUS);
	node_type_ = TokenTypeToCmd(token_type) + (is_suffix ? "_suffix" : "_prefix");
	if (is_suffix) {
		code_->Add({ TokenTypeToCmd(OP_ASSIGN), lhs->value_, kEmptyCmdArg, value_ });
		code_->Add({ op, lhs->value_, "1", lhs->value_ });
	}
	else {
		code_->Add({ op, lhs->value_, "1", lhs->value_ });
		code_->Add({ TokenTypeToCmd(OP_ASSIGN), lhs->value_, kEmptyCmdArg, value_ });
	}
}

ClTypeConvertNode::ClTypeConvertNode(ClAstPtr&& lhs, VariableType::PrimeType target_type, SymbolTable& symbol_table) {
	value_type_ = VariableType(target_type);
	if (lhs->value_type_.IsPrimeType() && value_type_.IsPrimeType()) {
		node_type_ = "TypeConvert";
		SymbolNode* symbol = symbol_table.PutTemp(target_type);
		value_ = symbol->name();
		code_.reset(new ClCodeBlock(lhs->code_));
		code_->Add({ TokenTypeToCmd(TokenType::OP_ASSIGN), lhs->value_, kEmptyCmdArg, value_ });
		children_.emplace_back(move(lhs));
	}
	else
		throw runtime_error("ClTypeConvertNode:: type convert only allowed on prime type");
}

ClAstPtr ClDeclNode::Parse(Lexer & lexer, SymbolTable & symbol_table) {
	if (IsPrimeType(lexer.Current().type_)) {
		VariableType::PrimeType type = TokenTypeToPrimeType(lexer.ToNext().type_);
		return ParseDecl1(lexer, type, symbol_table);
	}
	throw runtime_error("ClDeclNode:: Decl must begin with Type!");
	return nullptr;
}

ClAstPtr ClDeclNode::ParseDecl1(Lexer & lexer, VariableType::PrimeType type, SymbolTable & symbol_table) {
	if (lexer.Current().type_ == TokenType::IDENTIFIER) {
		string identifier = lexer.ToNext().value_;
		return ParseDecl2(lexer, identifier, type, symbol_table);
	}
	throw runtime_error("ClDeclNode:: Decl must has identifier!");
	return nullptr;
}

ClAstPtr ClDeclNode::ParseDecl2(Lexer & lexer, const std::string& id, VariableType::PrimeType type, SymbolTable & symbol_table) {
	/*if (lexer.Current().type_ == OP_LEFT_BRACKET) {
		lexer.ToNext();
		lexer.Consume(OP_RIGHT_BRACKET);
		ClAstPtr ret_ptr = ParseArray(lexer, type, symbol_table);
		int64_t width = ret_ptr->value_type_.width().front();
		SymbolNode symbol(id, SymbolNode::kVariable, ret_ptr->value_type_, symbol_table.Alloc(width));
		symbol_table.Put(symbol);
		ret_ptr->value_ = symbol.name();
		return ret_ptr;
	}
	else */{
		ClAstPtr ret_ptr = ParseArray(lexer, type, symbol_table);
		int64_t width = ret_ptr->value_type_.width().front();
		SymbolNode symbol(id, SymbolNode::kVariable, ret_ptr->value_type_, symbol_table.Alloc(width));
		symbol_table.Put(symbol);
		ret_ptr->value_ = symbol.name();
		return ret_ptr;
	}
	return nullptr;
}

ClAstPtr ClDeclNode::ParseArray(Lexer & lexer, VariableType::PrimeType type, SymbolTable & symbol_table) {
	if (lexer.Current().type_ == OP_LEFT_BRACKET) {
		lexer.ToNext();
		int dim = ClParser::ParseConstInt(lexer, symbol_table);
		lexer.Consume(OP_RIGHT_BRACKET);
		ClAstPtr ret_ptr = ParseArray(lexer, type, symbol_table);
		ret_ptr->value_type_.InsertDim(dim);
		return ret_ptr;
	}
	return ClAstPtr(new ClDeclNode(type));
}

int ClParser::ParseConstInt(Lexer& lexer, const SymbolTable& symbol_table) {
	int ret = 0;
	string const_id = lexer.Current().value_;
	if (lexer.Current().type_ == TokenType::INTEGER)
		ret = stoi(const_id);
	else if (lexer.Current().type_ == TokenType::IDENTIFIER) {
		const SymbolNode* ptr_node = symbol_table.Get(const_id);
		if (ptr_node && ptr_node->type_.is_const() && ptr_node->type_.prime_type() == VariableType::kInt && !ptr_node->type_.IsArray())
			ret = stoi(ptr_node->value_);
		else
			throw runtime_error("ClParser:: Parse const int \"" + const_id + "\" failed");
	}
	else
		throw runtime_error("ClParser:: Parse const int \"" + const_id + "\" failed");
	lexer.ToNext();
	return ret;
}

ClExprNode::ClExprNode(Lexer & lexer, SymbolTable & symbol_table) {
	
}

ClAstPtr ClExprNode::Parse(Lexer & lexer, SymbolTable & symbol_table) {
	ClAstPtr left_ptr = ParseE1(lexer, symbol_table);
	return ParseR(lexer, move(left_ptr), symbol_table);
}

ClAstPtr ClExprNode::ParseR(Lexer & lexer, ClAstPtr&& inherit, SymbolTable & symbol_table) {
	TokenType op_type = lexer.Current().type_;
	switch (lexer.Current().type_) {
	case TokenType::OP_ASSIGN:
	case TokenType::OP_ADD_ASSIGN:
	case TokenType::OP_MINUS_ASSIGN:
	case TokenType::OP_PRODUCT_ASSIGN:
	case TokenType::OP_DIVIDE_ASSIGN:
	case TokenType::OP_MOD_ASSIGN: {
		lexer.ToNext();
		ClAstPtr ptr_expr = Parse(lexer, symbol_table);
		ClAstPtr ptr_expr_r = ParseR(lexer, move(ptr_expr), symbol_table);
		return ClAstPtr(new ClBinaryOpNode(move(inherit), move(ptr_expr_r), op_type, symbol_table));
	}
	default:
		break;
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE1(Lexer & lexer, SymbolTable & symbol_table) {
	ClAstPtr left_ptr = ParseE2(lexer, symbol_table);
	return ParseE1R(lexer, move(left_ptr), symbol_table);
}

ClAstPtr ClExprNode::ParseE1R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_LOGICAL_OR) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE1(lexer, symbol_table);
		ClAstPtr ptr_expr_r = ParseE1R(lexer, move(ptr_expr), symbol_table);
		return ClAstPtr(new ClBinaryOpNode(move(inherit), move(ptr_expr_r), op_type, symbol_table));
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE2(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE3(lexer, symbol_table);
	return ParseE2R(lexer, move(left_ptr), symbol_table);
}

ClAstPtr ClExprNode::ParseE2R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_LOGICAL_AND) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE2(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE2R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE3(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE4(lexer, symbol_table);
	return ParseE3R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE3R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_BIT_OR) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE3(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE3R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE4(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE5(lexer, symbol_table);
	return ParseE4R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE4R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_BIT_XOR) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE4(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE4R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE5(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE6(lexer, symbol_table);
	return ParseE5R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE5R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_BIT_AND) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE5(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE5R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE6(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE7(lexer, symbol_table);
	return ParseE6R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE6R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_EQUAL || lexer.Current().type_ == TokenType::OP_NOT_EQUAL) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE6(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE6R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE7(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE8(lexer, symbol_table);
	return ParseE7R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE7R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_LESS || lexer.Current().type_ == TokenType::OP_GREATER
		|| lexer.Current().type_ == TokenType::OP_LESS_EQUAL || lexer.Current().type_ == TokenType::OP_GREATER_EQUAL) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE7(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE7R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE8(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE9(lexer, symbol_table);
	return ParseE8R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE8R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_ADD || lexer.Current().type_ == TokenType::OP_MINUS) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE8(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE8R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE9(Lexer& lexer, SymbolTable& symbol_table) {
	ClAstPtr left_ptr = ParseE10(lexer, symbol_table);
	return ParseE9R(lexer, move(left_ptr), symbol_table);
}
ClAstPtr ClExprNode::ParseE9R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_PRODUCT || lexer.Current().type_ == TokenType::OP_DIVIDE || lexer.Current().type_ == TokenType::OP_MOD) {
		TokenType op_type = lexer.ToNext().type_;
		ClAstPtr ptr_expr = ParseE9(lexer, symbol_table);
		ClAstPtr ptr_left(new ClBinaryOpNode(move(inherit), move(ptr_expr), op_type, symbol_table));
		return ParseE9R(lexer, move(ptr_left), symbol_table);
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseE10(Lexer& lexer, SymbolTable& symbol_table) {
	TokenType op_type = lexer.Current().type_;
	switch (lexer.Current().type_) {
	case TokenType::OP_INCREMENT:
	case TokenType::OP_DECREMENT: {
		lexer.ToNext();
		ClAstPtr ptr_expr = ParseE11(lexer, symbol_table);
		return ClAstPtr(new ClIncreDecreOpNode(move(ptr_expr), op_type, false, symbol_table));
	}
	case TokenType::OP_LOGICAL_NOT:
	case TokenType::OP_BIT_NOT:
	case TokenType::OP_ADD:
	case TokenType::OP_MINUS: {
		lexer.ToNext();
		ClAstPtr ptr_expr = ParseE11(lexer, symbol_table);
		return ClAstPtr(new ClUnaryOpNode(move(ptr_expr), op_type, symbol_table));
	}
	default:
		break;
	}
	return ParseE11(lexer, symbol_table);
}

ClAstPtr ClExprNode::ParseE11(Lexer& lexer, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_LEFT_PARENTHESIS) {
		lexer.ToNext();
		if (IsPrimeType(lexer.Current().type_)) {
			VariableType::PrimeType target_type = TokenTypeToPrimeType(lexer.Current().type_);
			lexer.ToNext();
			lexer.Consume(TokenType::OP_RIGHT_PARENTHESIS);
			ClAstPtr left_ptr = Parse(lexer, symbol_table);
			ClAstPtr ptr_expr = ParseE11R(lexer, move(left_ptr), symbol_table);
			return ClAstPtr(new ClTypeConvertNode(move(ptr_expr), target_type, symbol_table));
		}
		else {
			ClAstPtr left_ptr = Parse(lexer, symbol_table);
			lexer.Consume(OP_RIGHT_PARENTHESIS);
			return ParseE11R(lexer, move(left_ptr), symbol_table);
		}
	}
	else if (lexer.Current().type_ == TokenType::IDENTIFIER) {
		const SymbolNode* ptr_symbol = symbol_table.Get(lexer.Current().value_);
		if (ptr_symbol != nullptr) {
			if (ptr_symbol->category_ == SymbolNode::kVariable) {
				ClAstPtr left_ptr = ParseIdValue(lexer, symbol_table);
				return ParseE11R(lexer, move(left_ptr), symbol_table);
			}
			else {
				throw runtime_error("ClExprNode:: Function not implement now");
			}
		}
	}
	else if (TokenTypeToPrimeType(lexer.Current().type_) != VariableType::kNotVariable && !IsPrimeType(lexer.Current().type_)) {
		VariableType::PrimeType type = TokenTypeToPrimeType(lexer.Current().type_);
		return ClAstPtr(new ClPrimeTypeNode(lexer.ToNext().value_, type));
	}
	throw runtime_error("ClExprNode:: Parse11 error!");
	return nullptr;
}
ClAstPtr ClExprNode::ParseE11R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table) {
	if (lexer.Current().type_ == TokenType::OP_INCREMENT || lexer.Current().type_ == TokenType::OP_DECREMENT) {
		return ClAstPtr(new ClIncreDecreOpNode(move(inherit), lexer.ToNext().type_, true, symbol_table));
	}
	return move(inherit);
}

ClAstPtr ClExprNode::ParseIdValue(Lexer& lexer, SymbolTable& symbol_table) {
	return nullptr;
}

} // namespace pswgoo
