#include "cl_parser.h"

#include <iostream>
#include <algorithm>
#include <string>

using namespace std;

namespace pswgoo {

ClBinaryOpNode::ClBinaryOpNode(ClAstPtr&& lhs, ClAstPtr&& rhs, TokenType token_type, SymbolTable& symbol_table) {
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
		if (rhs->value_type_.prime_type() == VariableType::kNull)
			throw runtime_error("ClBinaryOpNode:: null cannot be rvalue!");
		if (lhs->value_type_.prime_type() < rhs->value_type_.prime_type())
			runtime_error("ClBinaryOpNode:: Type narrowed with " + TokenTypeToCmd(token_type));
		if (lhs->value_type_.prime_type() == VariableType::kBoolean && token_type != TokenType::OP_ASSIGN)
			runtime_error("ClBinaryOpNode:: boolean can only do assign");
		value_type_ = lhs->value_type_;
		break;
	default:
		throw runtime_error("ClBinaryOpNode:: Invalid binary operator!");
		break;
	}

	node_type_ = TokenTypeToCmd(token_type);

	if (!value_type_.IsPrimeType())
		throw std::runtime_error("ClBinaryOpNode:: Binary operators get invalid parameters");
	code_.reset(new ClCodeBlock(lhs->code_));
	code_->Add(rhs->code_);
	SymbolNode* tmp_node = symbol_table.PutTemp(value_type_.prime_type());
	value_ = tmp_node->name();
	code_->Add({ TokenTypeToCmd(token_type), lhs->value_, rhs->value_, value_ });

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
		code_.reset(new ClCodeBlock(lhs->code_));
		code_->Add({TokenTypeToCmd(token_type), lhs->value_, "", value_});
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
		code_->Add({ TokenTypeToCmd(OP_ASSIGN), lhs->value_, "", value_ });
		code_->Add({ op, lhs->value_, "1", lhs->value_ });
	}
	else {
		code_->Add({ op, lhs->value_, "1", lhs->value_ });
		code_->Add({ TokenTypeToCmd(OP_ASSIGN), lhs->value_, "", value_ });
	}
}

ClAstPtr ClDeclNode::Parse(Lexer & lexer, SymbolTable & symbol_table) {
	switch (lexer.Current().type_) {
	case TokenType::KEY_BOOL:
	case TokenType::KEY_CHAR:
	case TokenType::KEY_INT:
	case TokenType::KEY_FLOAT:
	case TokenType::KEY_DOUBLE: {
		VariableType::PrimeType type = TokenTypeToPrimeType(lexer.ToNext().type_);
		return ParseDecl1(lexer, type, symbol_table);
	}
	default:
		break;
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

int ClParser::ParseConstInt(Lexer & lexer, const SymbolTable& symbol_table) {
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

} // namespace pswgoo
