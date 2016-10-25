#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "lexer.h"
#include "symbol_table.h"

namespace pswgoo {

typedef std::vector<std::string> ClCodeLine;
class ClCodeBlock;
typedef std::shared_ptr<ClCodeBlock> ClCodePtr;

class ClCodeBlock {
public:
	ClCodeBlock() = default;
	ClCodeBlock(const ClCodePtr& code) { code_blocks_.push_back(code); }
	ClCodeBlock(const ClCodeLine& code) :leaf_code_(new ClCodeLine(code)) {}

	void Add(const ClCodeLine& code) {
		code_blocks_.emplace_back(new ClCodeBlock(code));
	}
	void Add(const ClCodePtr& code_ptr) {
		code_blocks_.emplace_back(code_ptr);
	}

private:
	std::unique_ptr<ClCodeLine> leaf_code_;
	std::vector<ClCodePtr> code_blocks_;
};

class ClAstNode;
typedef std::unique_ptr<ClAstNode> ClAstPtr;

class ClAstNode {
public:
	ClAstNode() {};

public:
	std::string value_;			// name or lexeme value
	std::string node_type_ = "AstNode";
	ClCodePtr code_;
	VariableType value_type_;
	std::vector<ClAstPtr> children_;
};

std::string TokenTypeToCmd(TokenType token_type) {
	return kTokenTypeStr[token_type];
}

VariableType::PrimeType TokenTypeToPrimeType(TokenType token_type) {
	switch (token_type) {
	case TokenType::KEY_BOOL:
	case TokenType::BOOLEAN:
		return VariableType::PrimeType::kBoolean;
	case TokenType::KEY_CHAR:
	case TokenType::CHAR:
		return VariableType::PrimeType::kChar;
	case TokenType::KEY_INT:
	case TokenType::INTEGER:
		return VariableType::PrimeType::kInt;
	case TokenType::KEY_FLOAT:
		return VariableType::PrimeType::kFloat;
	case TokenType::KEY_DOUBLE:
	case TokenType::REAL:
		return VariableType::PrimeType::kDouble;
	case TokenType::KEY_NULL:
		return VariableType::PrimeType::kNull;
	default:
		break;
	}
	return VariableType::PrimeType::kNotVariable;
}

bool IsPrimeType(TokenType token_type);

class ClPrimeTypeNode : public ClAstNode {
public:
	ClPrimeTypeNode(const std::string& value, VariableType::PrimeType type) {
		node_type_ = "PrimeType";
		value_type_ = VariableType(type);
		value_type_.set_is_rvalue(true);
		if (type == VariableType::kBoolean) {
			if (value == "true")
				value_ = "1";
			else
				value_ = "0";
		}
		else if (type == VariableType::kNull)
			value_ = "0";
		else
			value_ = value;
	}
};

class ClBinaryOpNode : public ClAstNode {
public:
	ClBinaryOpNode(ClAstPtr&& lhs, ClAstPtr&& rhs, TokenType token_type, SymbolTable& symbol_table);
};

class ClUnaryOpNode : public ClAstNode {
public:
	ClUnaryOpNode(ClAstPtr&& lhs, TokenType token_type, SymbolTable& symbol_table);
};

class ClIncreDecreOpNode : public ClAstNode {
public:
	ClIncreDecreOpNode(ClAstPtr&& lhs, TokenType token_type, bool is_suffix, SymbolTable& symbol_table);
};

class ClTypeConvertNode : public ClAstNode {
public:
	ClTypeConvertNode(ClAstPtr&& lhs, VariableType::PrimeType target_type, SymbolTable& symbol_table);
};

class ClDeclNode : public ClAstNode {
public:
	ClDeclNode(VariableType::PrimeType type) {
		value_type_ = VariableType(type);
	}

	static ClAstPtr Parse(Lexer& lexer, SymbolTable& symbol_table);

private:
	static ClAstPtr ParseDecl1(Lexer& lexer, VariableType::PrimeType type, SymbolTable& symbol_table);
	static ClAstPtr ParseDecl2(Lexer& lexer, const std::string& id, VariableType::PrimeType type, SymbolTable& symbol_table);
	static ClAstPtr ParseArray(Lexer& lexer, VariableType::PrimeType type, SymbolTable& symbol_table);
};

class ClExprNode : public ClAstNode {
public:
	static ClAstPtr Parse(Lexer& lexer, SymbolTable& symbol_table);

private:
	static ClAstPtr ParseIdValue(Lexer& lexer, SymbolTable& symbol_table);

	// right association
	static ClAstPtr ParseR(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE1(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE1R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE2(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE2R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE3(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE3R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE4(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE4R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE5(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE5R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE6(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE6R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE7(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE7R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE8(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE8R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE9(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE9R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);

	static ClAstPtr ParseE10(Lexer& lexer, SymbolTable& symbol_table);

	static ClAstPtr ParseE11(Lexer& lexer, SymbolTable& symbol_table);
	static ClAstPtr ParseE11R(Lexer& lexer, ClAstPtr&& inherit, SymbolTable& symbol_table);


};

class ClParser {
public:
	static int ParseConstInt(Lexer& lexer, const SymbolTable& symbol_table);

private:
	Lexer lexer_;
};

} // namespace pswgoo
