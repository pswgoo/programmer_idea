#pragma once

#include "lexer.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace pswgoo {

class AstNode;
typedef std::unique_ptr<AstNode> AstNodePtr;

class Symbol {
public:
	std::string name_;
};
typedef std::unique_ptr<Symbol> SymbolPtr;

class VariableSymbol : public Symbol {
public:
	TypePtr type_;

};

class ConstSymbol : public Symbol {
public:
	TypePtr type_;
	std::string value_;
};

class Scope {
public:
	Symbol* Get(const std::string& name) const {
		std::unordered_map<std::string, SymbolPtr>::const_iterator fid = symbol_table_.find(name);
		if (symbol_table_.end() != fid)
			return fid->second.get();
		return nullptr;
	}
	void Put(SymbolPtr&& symbol_ptr) {
		symbol_table_[symbol_ptr->name_] = move(symbol_ptr);
	}
	Scope* parent() const {	return parent_; }

private:
	Scope* parent_;
	std::unordered_map<std::string, SymbolPtr> symbol_table_;
};
typedef std::unique_ptr<Scope> ScopePtr;

class Type : public Symbol {
public:
	enum TypeId {kBool, kChar, kInt, kLong, kFloat, kDouble, kPrimitiveType, kArray, kReference, kFunction, kClass};

	// @return type size in bytes
	virtual int64_t SizeOf() const = 0;

	TypeId type_id_;
};
typedef std::unique_ptr<Type> TypePtr;

class Bool : public Type {
public:
	virtual int64_t SizeOf() const override { return 1; }
};

class Char : public Type {
public:
	virtual int64_t SizeOf() const override { return 1; }
};

class Int : public Type {
public:
	virtual int64_t SizeOf() const override { return 4; }
};

class Long : public Type {
public:
	virtual int64_t SizeOf() const override { return 8; }
};

class Float : public Type {
public:
	virtual int64_t SizeOf() const override { return 4; }
};

class Double : public Type {
public:
	virtual int64_t SizeOf() const override { return 8; }
};

class Array : public Type {
public:
	virtual int64_t SizeOf() const override { return element_type_->SizeOf() * length_; }


	TypePtr element_type_;
	int64_t length_;
};

class Reference : public Type {
public:
	virtual int64_t SizeOf() const override { return 8; }
	TypePtr target_type_;
};

class Function : public Type {
public:
	virtual int64_t SizeOf() const override { return 0; }

private:
	TypePtr ret_type_;
	std::vector<VariableSymbol> params_;
	AstNodePtr body_;
	ScopePtr scope_;
};

class Class : public Type {

};

struct AstNode {
	TokenType token_type_;
};

struct StmtNode : public AstNode {

};
typedef std::unique_ptr<StmtNode> StmtNodePtr;

struct ExprNode : public StmtNode {
	TypePtr type_;
};
typedef std::unique_ptr<ExprNode> ExprNodePtr;

struct ImmediateNode : public ExprNode {
	std::string value_;
};

struct VariableNode : public ExprNode {
	std::string name_;
	Scope* scope_;
};

struct AssignNode : public ExprNode {
	ExprNodePtr expr_;
};

struct BinaryOpNode : public ExprNode {
	ExprNodePtr left_expr_;
	ExprNodePtr right_expr_;
};

struct UnaryOpNode : public ExprNode {
	ExprNodePtr expr_;
};

struct ArrayNode : public ExprNode {
	ExprNodePtr base_;  // array base address
	ExprNodePtr index_;  // array index
};

struct CallNode : public ExprNode {
	std::string func_name_;
	std::vector<ExprNodePtr> params_;
	Scope* scope_;
};

struct IfNode : public StmtNode {
	ExprNodePtr condition_;
	StmtNodePtr then_;
	StmtNodePtr else_;
};

struct ForNode : public StmtNode {
	StmtNodePtr init_;
	ExprNodePtr condition_;
	StmtNodePtr iter_;
	StmtNodePtr body_;
};

struct WhileNode : public StmtNode {
	ExprNodePtr condition_;
	StmtNodePtr body_;
};

struct StmtBlockNode: public StmtNode {
	std::vector<StmtNodePtr> stmts_;
};


} // namespace pswgoo
