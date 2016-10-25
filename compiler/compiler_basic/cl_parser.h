#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "lexer.h"
#include "symbol_table.h"

namespace pswgoo {

struct ClCode {
	ClCode() = default;
	ClCode(const std::vector<std::string>& code) : code_(code){}
	std::vector<std::string> code_;
};

const char kAddressSymbol = '&';

class ClAstNode;
typedef std::unique_ptr<ClAstNode> ClAstPtr;
typedef std::shared_ptr<ClCode> ClCodePtr;

class ClAstNode {
public:
	ClAstNode(){};

	const VariableType& value_type()const { return value_type_; }
	const std::vector<ClCodePtr>& code() const { return code_; }
	const std::string& value() const { return value_; }

protected:
	std::string value_;			// address or lexeme value
	std::vector<ClCodePtr> code_;
	VariableType value_type_;
	std::vector<ClAstPtr> children_;
};

class ClPrimeTypeNode : public ClAstNode {
public:
	ClPrimeTypeNode(const std::string& value, VariableType::PrimeType type) {
		value_ = value;
		value_type_ = VariableType(type);
	}
};

class ClAddNode : public ClAstNode {
public:
	ClAddNode(ClAstPtr&& lhs, ClAstPtr&& rhs, SymbolTable& symbol_table) {
		value_type_ = MaxType(lhs->value_type(), rhs->value_type());
		if (!value_type_.IsPrimeType())
			throw std::runtime_error("Add operators get invalid parameters");
		code_ = lhs->code();
		code_.insert(code_.end(), rhs->code().begin(), rhs->code().end());
		SymbolNode* tmp_node = symbol_table.PutTemp(value_type_.prime_type());
		value_ = kAddressSymbol + std::to_string(tmp_node->address_);
		std::vector<std::string> code_str = { "Add", lhs->value(), rhs->value(), value_ };
		ClCodePtr add_code(new ClCode(code_str));

		children_.emplace_back(move(lhs));
		children_.emplace_back(move(rhs));
	}
};

class ClExprNode : public ClAstNode {
public:
	static std::unique_ptr<ClAstNode> Parse(Lexer& lexer, ClAstPtr&& inherit);

};

class ClParser {
public:
	

private:
	Lexer lexer_;
};

} // namespace pswgoo
