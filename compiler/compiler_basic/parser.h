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

	const std::string& name()const { return name_; }

	template<typename T>
	bool Is() const {
		return dynamic_cast<const T*>(this) != nullptr;
	}

protected:
	virtual void V() {};

	std::string name_;
};
typedef std::unique_ptr<Symbol> SymbolPtr;

class Type;
typedef std::unique_ptr<Type> TypePtr;
class Scope;
typedef std::unique_ptr<Scope> ScopePtr;

class VariableSymbol : public Symbol {
public:
	VariableSymbol(const std::string& name, const Type* type) : Symbol(name), type_(type) {}
	const Type* type_;
};

class ConstSymbol : public Symbol {
public:
	ConstSymbol(const std::string& name, const Type* type) : Symbol(name), type_(type) {}
	const Type* type_;
};

// ImmediateSymbol must know value in compiling time.
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

	const Symbol* GetCurrent(const std::string& name) const {
		std::unordered_map<std::string, SymbolPtr>::const_iterator fid = symbol_table_.find(name);
		if (symbol_table_.end() != fid)
			return fid->second.get();
		return nullptr;
	}
	const Symbol* Get(const std::string& name) const {
		const Scope* ptr = this;
		while (ptr != nullptr) {
			if (const Symbol* ret_ptr = ptr->GetCurrent(name))
				return ret_ptr;
			ptr = ptr->parent_;
		}
		return nullptr;
	}
	const Symbol* Put(SymbolPtr&& symbol_ptr) {
		SymbolPtr& ptr = symbol_table_[symbol_ptr->name()];
		if (!ptr)
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

	static const Type* Max(const Type* type1, const Type* type2, bool need_assert = false) {
		if (type1->CouldPromoteTo(type2))
			return type2;
		else if (type2->CouldPromoteTo(type1))
			return type1;
		assert(!need_assert && (type1->name() + " and " + type2->name() + " not compatible").c_str());
		return nullptr;
	}

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

	const Type* ret_type_;
	std::vector<VariableSymbol> params_;
};

class Class : public Type {

};

struct AstNode {
	AstNode(TokenType token_type = NOT_DEFINED) : token_type_(token_type) {}
	template<typename T>
	bool Is() const {
		return dynamic_cast<const T*>(this) != nullptr;
	}

	TokenType token_type_;

protected:
	virtual void V() {};
};

struct StmtNode : public AstNode {
	StmtNode(TokenType token_type = NOT_DEFINED) : AstNode(token_type) {}
};
typedef std::unique_ptr<StmtNode> StmtNodePtr;

struct ExprNode : public StmtNode {
	ExprNode(TokenType token_type, const Type* type) : StmtNode(token_type), type_(type) {}
	const Type* type_;
};
typedef std::unique_ptr<ExprNode> ExprNodePtr;

struct ImmediateNode : public ExprNode {
	ImmediateNode(TokenType token_type, const Type* type, const std::string& value) : ExprNode(token_type, type), value_(value) {}
	std::string value_;
};
typedef std::unique_ptr<ImmediateNode> ImmediateNodePtr;

struct VariableNode : public ExprNode {
	VariableNode(TokenType token_type, const Type* type, const std::string& var_name) : ExprNode(token_type, type), var_name_(var_name) {}
	std::string var_name_;
};
typedef std::unique_ptr<VariableNode> VariableNodePtr;

struct AssignNode : public ExprNode {
	AssignNode(TokenType token_type, const Type* type, VariableNodePtr&& left_var, ExprNodePtr&& right_expr) : ExprNode(token_type, type), left_var_(std::move(left_var)), right_expr_(std::move(right_expr)) {}
	VariableNodePtr left_var_;
	ExprNodePtr right_expr_;
};

struct BinaryOpNode : public ExprNode {
	BinaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& left_expr, ExprNodePtr&& right_expr) : ExprNode(token_type, type), left_expr_(std::move(left_expr)), right_expr_(std::move(right_expr)) {}
	ExprNodePtr left_expr_;
	ExprNodePtr right_expr_;
};

struct UnaryOpNode : public ExprNode {
	UnaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& expr) : ExprNode(token_type, type), expr_(std::move(expr)) {}
	ExprNodePtr expr_;
};
struct ArrayNode : public ExprNode {
	ArrayNode(TokenType token_type, const Type* type, ExprNodePtr&& base, ExprNodePtr&& index) : ExprNode(token_type, type), base_(std::move(base)), index_(std::move(index)) {}
	ExprNodePtr base_;  // array base address
	ExprNodePtr index_;  // array index
};

struct CallNode : public ExprNode {
	CallNode(TokenType token_type, const Type* type, const std::string& func_name, std::vector<ExprNodePtr>&& params) : ExprNode(token_type, type), func_name_(func_name), params_(std::move(params)) {}
	std::string func_name_;
	std::vector<ExprNodePtr> params_;
};
typedef std::unique_ptr<CallNode> CallNodePtr;

struct DefNode : public ExprNode {
	DefNode(TokenType token_type, const Type* type, const std::string& var_name, ExprNodePtr&& expr) : ExprNode(token_type, type), var_name_(var_name), expr_(std::move(expr)) {}
	std::string var_name_;
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
		return false;
	}

	StmtNodePtr Parse();

	StmtNodePtr ParseFuncDef1(const Type* type, const std::string &name);

	StmtNodePtr ParseStmt();

	VariableSymbol ParseDecl();

	ExprNodePtr ParseExpr();

	int64_t ParseConstInt();

private:
	ExprNodePtr ParseER(ExprNodePtr &&inherit);
	ExprNodePtr ParseE1();
	ExprNodePtr ParseE1R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE2();
	ExprNodePtr ParseE2R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE3();
	ExprNodePtr ParseE3R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE4();
	ExprNodePtr ParseE4R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE5();
	ExprNodePtr ParseE5R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE6();
	ExprNodePtr ParseE6R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE7();
	ExprNodePtr ParseE7R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE8();
	ExprNodePtr ParseE8R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE9();
	ExprNodePtr ParseE9R(ExprNodePtr &&inherit);
	ExprNodePtr ParseE10();
	ExprNodePtr ParseE11();
	CallNodePtr ParseCall();
	ImmediateNodePtr ParseLiteralValue();

	const Type* GetType(const std::string& type_name) {
		return dynamic_cast<const Type*>(current_scope_->Get(type_name));
	}

private:
	Lexer lexer_;
	StmtBlockNode stmt_block_;
	Scope* current_scope_;
	ScopePtr global_scope_;
};


} // namespace pswgoo