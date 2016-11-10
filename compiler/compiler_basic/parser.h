#pragma once

#include "lexer.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace pswgoo {

struct AstNode;
typedef std::unique_ptr<AstNode> AstNodePtr;

class Symbol {
public:
	Symbol() = default;
	Symbol(const std::string& name) : name_(name) {}

	std::string name()const { return name_; }

	virtual void V() {};

	template<typename T>
	bool Is() const {
		return dynamic_cast<const T*>(this) != nullptr;
	}

	std::string name_;
};
typedef std::unique_ptr<Symbol> SymbolPtr;

class Type;
typedef std::unique_ptr<Type> TypePtr;
class Scope;
typedef std::unique_ptr<Scope> ScopePtr;

class VariableSymbol : public Symbol {
public:
	const Type* type_;
};

class ConstSymbol : public Symbol {
public:
	ConstSymbol(const std::string& name, const Type* type) : Symbol(name), type_(type) {}
	const Type* type_;
};

class ImmediateSymbol : public ConstSymbol {
public:
	ImmediateSymbol(const std::string& name, const Type* type, const std::string& value) : ConstSymbol(name, type), value_(value) {}
	const Type* type_;
	std::string value_;
};

class FunctionSymbol : public ConstSymbol {
public:
	FunctionSymbol(const std::string& name, AstNodePtr&& body, ScopePtr&& scope, const Type* type) : ConstSymbol(name, type), body_(std::move(body)), scope_(std::move(scope)) {}

	AstNodePtr body_;
	ScopePtr scope_;
};

class Scope {
public:
	Scope(const Scope* parent_ptr = nullptr) : parent_(parent_ptr) {}

	const Symbol* Get(const std::string& name) const {
		std::unordered_map<std::string, SymbolPtr>::const_iterator fid = symbol_table_.find(name);
		if (symbol_table_.end() != fid)
			return fid->second.get();
		return nullptr;
	}
	const Symbol* Put(SymbolPtr&& symbol_ptr) {
		SymbolPtr& ptr = symbol_table_[symbol_ptr->name_];
		ptr = std::move(symbol_ptr);
		return ptr.get();
	}
	const Scope* parent() const {	return parent_; }

private:
	const Scope* parent_ = nullptr;
	std::unordered_map<std::string, SymbolPtr> symbol_table_;
};

class Type : public Symbol {
public:
	enum TypeId {kNon, kBool, kChar, kInt, kLong, kFloat, kDouble, kPrimitiveType, kArray, kReference, kFunction, kClass};
	Type(TypeId type_id) : type_id_(type_id) {}
	Type(const std::string& name, TypeId type_id, Scope* parent_scope) : Symbol(name), type_id_(type_id), parent_scope_(parent_scope) {}

	// @return type size in bytes
	virtual int64_t SizeOf() const = 0;
	virtual bool CouldPromoteTo(const Type* target_type) const {
		if (target_type->type_id_ < kPrimitiveType && type_id_ == target_type->type_id_)
			return true;
		if (target_type->type_id_ < kPrimitiveType && type_id_ >= kChar && type_id_ <= target_type->type_id_)
			return true;
		return false;
	};

	TypeId type_id_;
	Scope* parent_scope_;
};

class Bool : public Type {
public:
	Bool(Scope* parent_scope) : Type("bool", kBool, parent_scope) {}
	virtual int64_t SizeOf() const override { return 1; }
};

class Char : public Type {
public:
	Char(Scope* parent_scope) : Type("char", kChar, parent_scope) {}
	virtual int64_t SizeOf() const override { return 1; }
};

class Int : public Type {
public:
	Int(Scope* parent_scope) : Type("int", kInt, parent_scope) {}
	virtual int64_t SizeOf() const override { return 4; }
};

class Long : public Type {
public:
	Long(Scope* parent_scope) : Type("long", kLong, parent_scope) {}
	virtual int64_t SizeOf() const override { return 8; }
};

class Float : public Type {
public:
	Float(Scope* parent_scope) : Type("float", kFloat, parent_scope) { }
	virtual int64_t SizeOf() const override { return 4; }
};

class Double : public Type {
public:
	Double(Scope* parent_scope) : Type("double", kDouble, parent_scope) {}
	virtual int64_t SizeOf() const override { return 8; }
};

class Array : public Type {
public:
	Array(const Type* type, int64_t length, Scope* parent_scope) : Type(std::to_string(length) + "_" + type->name(), kArray, parent_scope), element_type_(type), length_(length) {}

	virtual int64_t SizeOf() const override { return element_type_->SizeOf() * length_; }

	const Type* element_type_;
	int64_t length_;
};

class Reference : public Type {
public:
	virtual int64_t SizeOf() const override { return 8; }
	const Type* target_type_;
};

class Function : public Type {
public:
	Function(const Type* ret_type, const std::vector<VariableSymbol>& params) :Type(kFunction), ret_type_(ret_type), params_(params) {
		name_ = "func";
		for (int i = 0; i < params.size(); ++i)
			name_ += "#" + params[i].type_->name();
	}

	virtual int64_t SizeOf() const override { return 8; }

private:
	const Type* ret_type_;
	std::vector<VariableSymbol> params_;
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

struct DefNode : public StmtNode {
	std::string name_;
	ExprNodePtr expr_;
};

struct StmtBlockNode : public StmtNode {
	std::vector<StmtNodePtr> stmts_;
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

class Compiler {
public:
	Compiler(): global_scope_(new Scope){
		// put primitive types to global scope
		global_scope_->Put(std::unique_ptr<Bool>(new Bool(global_scope_.get())));
		global_scope_->Put(std::unique_ptr<Char>(new Char(global_scope_.get())));
		global_scope_->Put(std::unique_ptr<Int>(new Int(global_scope_.get())));
		global_scope_->Put(std::unique_ptr<Long>(new Long(global_scope_.get())));
		global_scope_->Put(std::unique_ptr<Float>(new Float(global_scope_.get())));
		global_scope_->Put(std::unique_ptr<Double>(new Double(global_scope_.get())));
		current_scope_ = global_scope_.get();
	}

	void Parse(const std::string& program);

private:
	bool CurrentIsType() const {
		if (lexer_.Current().type_ == IDENTIFIER) {
			const Symbol* sym = current_scope_->Get(lexer_.Current().value_);
			if (sym != nullptr && sym->Is<Type>())
				return true;
		}
	}

	StmtNodePtr Parse();

	StmtNodePtr ParseFuncDef1(const Type* type, const std::string &name);

	StmtNodePtr ParseStmt();

	VariableSymbol ParseDecl();

	ExprNodePtr ParseExpr();

	int64_t ParseConstInt();

private:
	Lexer lexer_;
	StmtBlockNode stmt_block_;
	Scope* current_scope_;
	ScopePtr global_scope_;
};


} // namespace pswgoo
