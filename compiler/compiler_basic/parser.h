#pragma once

#include "lexer.h"
#include "instruction.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace pswgoo {

struct AstNode;
typedef std::unique_ptr<AstNode> AstNodePtr;

class Object {
public:
	template<typename T>
	bool Is() const {
		return dynamic_cast<const T*>(this) != nullptr;
	}
	template<typename T>
	const T* To() const {
		return dynamic_cast<const T*>(this);
	}
	template<typename T>
	T* To() {
		return dynamic_cast<T*>(this);
	}
protected:
	virtual void V() {};
};

class Symbol: public Object {
public:
	Symbol() = default;
	Symbol(const std::string& name) : name_(name) {}

	const std::string& name()const { return name_; }

	friend class Scope;

	std::string name_;
	int64_t index_; // for global scope, it is const symbol table index; for local scope, it is local stack index.
};
typedef std::unique_ptr<Symbol> SymbolPtr;

class Type : public Symbol {
public:
	enum TypeId { kBool, kChar, kInt, kLong, kFloat, kDouble, kReference, kPrimitiveType, kArray, kFunction, kClass };
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
typedef std::unique_ptr<Type> TypePtr;

class LiteralSymbol;
class ImmediateSymbol;
class Scope: public Object {
public:
	Scope(const Scope* parent_ptr = nullptr) : parent_(parent_ptr) { if (parent_) depth_ = parent_->depth_ + 1; }

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
	virtual const Symbol* Put(SymbolPtr&& symbol_ptr) {
		SymbolPtr& ptr = symbol_table_[symbol_ptr->name()];
		if (!ptr) {
			ptr = std::move(symbol_ptr);
			if (!symbol_ptr->Is<ImmediateSymbol>() && !symbol_ptr->Is<LiteralSymbol>())
				ptr->index_ = top_++;
		}
		return ptr.get();
	}
	Symbol* Get(const std::string& name) {
		return const_cast<Symbol*>(const_cast<const Scope*>(this)->Get(name));
	}
	const Scope* parent() const { return parent_; }
	int depth() const { return depth_; }


protected:
	const Scope* parent_ = nullptr;
	int depth_ = 0;
	int64_t top_ = 0;
	std::unordered_map<std::string, SymbolPtr> symbol_table_;
};
typedef std::unique_ptr<Scope> ScopePtr;

class VariableSymbol : public Symbol {
public:
	VariableSymbol(const std::string& name, const Type* type) : Symbol(name), type_(type) {}
	const Type* type_; // must be PrimaryType
};

class LocalScope : public Scope {
public:
	LocalScope(const Scope* parent) : Scope(parent), stack_start_(0), max_stack_size_(0) {
		if (parent_->Is<LocalScope>())
			top_ = stack_start_ = parent->To<LocalScope>()->stack_start_;
	}

	const Symbol* Put(SymbolPtr&& symbol_ptr) override {
		SymbolPtr& ptr = symbol_table_[symbol_ptr->name()];
		if (!ptr) {
			ptr = std::move(symbol_ptr);
			if (symbol_ptr->Is<VariableSymbol>()) {
				ptr->index_ = top_;
				top_ += symbol_ptr->To<VariableSymbol>()->type_->SizeOf();
			}
		}
		return ptr.get();
	}
	void add_child_scope(std::unique_ptr<LocalScope>&& child_scope) {
		max_stack_size_ = std::max(max_stack_size_, top_ - stack_start_ + child_scope->max_stack_size_);
		child_scopes_.emplace_back(std::move(child_scope));
	}

protected:
	int64_t stack_start_;
	int64_t max_stack_size_;
	std::vector<std::unique_ptr<LocalScope>> child_scopes_;
};
typedef std::unique_ptr<LocalScope> LocalScopePtr;

class ConstSymbol : public Symbol {
public:
	ConstSymbol(const std::string& name, const Type* type) : Symbol(name), type_(type) {}
	const Type* type_;
};

class LiteralSymbol : public ConstSymbol {
public:
	LiteralSymbol(const Type* type, const std::string& value) : ConstSymbol("$" + value + "$" + type->name(), type), value_(value) {}
	const Type* type_;
	std::string value_;
};

// ImmediateSymbol must know value in compiling time.
class ImmediateSymbol : public ConstSymbol {
public:
	ImmediateSymbol(const std::string& name, const Type* type, const LiteralSymbol* ptr_symbol) : ConstSymbol(name, type), literal_symbol_(ptr_symbol) {}
	const Type* type_;
	const LiteralSymbol* literal_symbol_;
};

class FunctionSymbol : public ConstSymbol {
public:
	FunctionSymbol(const std::string& name, const std::vector<std::string>& param_names, AstNodePtr&& body, ScopePtr&& scope, const Type* type) :
		ConstSymbol(name, type), param_names_(param_names), body_(std::move(body)), scope_(std::move(scope)) {}

	std::vector<std::string> param_names_;
	AstNodePtr body_;
	ScopePtr scope_;
};
typedef std::unique_ptr<FunctionSymbol> FunctionSymbolPtr;

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
	Array(const Type* type, int64_t length, Scope* parent_scope) : 
		Type(type->name() + "_" + std::to_string(length), kArray, parent_scope), element_type_(type), length_(length) {}

	virtual int64_t SizeOf() const override { return element_type_->SizeOf() * length_; }

	const Type* element_type_;
	int64_t length_;
};
typedef std::unique_ptr<Array> ArrayPtr;

class Reference : public Type {
public:
	Reference(const Type* target_type, Scope* parent_scope) : 
		Type("ref@" + target_type->name(), kReference, parent_scope), target_type_(target_type) {}
	virtual int64_t SizeOf() const override { return 8; }
	template<typename T>
	const T* Get() const {
		return dynamic_cast<const T*>(target_type_);
	}

	const Type* target_type_;
};
typedef std::unique_ptr<Reference> ReferencePtr;

class Function : public Type {
public:
	Function(const Type* ret_type, const std::vector<const Type*>& param_types) :
		Type(kFunction), ret_type_(ret_type), param_types_(param_types) {
		name_ = "func";
		for (int i = 0; i < param_types.size(); ++i)
			name_ += "#" + param_types[i]->name();
	}

	virtual int64_t SizeOf() const override { return 8; }

	const Type* ret_type_;
	std::vector<const Type*> param_types_;
};

class Class : public Type {

};

static std::string kAstIndent = "  ";
struct AstNode : public Object {
	AstNode(TokenType token_type = NOT_DEFINED) : token_type_(token_type) {}

	virtual void Print(std::ostream& oa, const std::string& padding)  const {
		oa << padding + "{" << kTokenTypeStr[token_type_] << "}" << std::endl;
	}

	TokenType token_type_;
};

struct StmtNode : public AstNode {
	StmtNode(TokenType token_type = NOT_DEFINED) : AstNode(token_type) {}
};
typedef std::unique_ptr<StmtNode> StmtNodePtr;

struct ExprNode : public StmtNode {
	ExprNode(TokenType token_type, const Type* type) : StmtNode(token_type), type_(type) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override{
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << "}" << std::endl;
	}

	const Type* type_;
};
typedef std::unique_ptr<ExprNode> ExprNodePtr;

struct ImmediateNode : public ExprNode {
	ImmediateNode(TokenType token_type, const Type* type, const std::string& value) : 
		ExprNode(token_type, type), value_(value) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << value_ << "}" << std::endl;
	}

	std::string value_;
};
typedef std::unique_ptr<ImmediateNode> ImmediateNodePtr;

struct VariableNode : public ExprNode {
	VariableNode(TokenType token_type, const Type* type, const std::string& var_name, ExprNodePtr&& expr) : 
		ExprNode(token_type, type), var_name_(var_name), expr_(std::move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << var_name_ << std::endl;
		if (expr_)
			expr_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	std::string var_name_;
	const VariableSymbol* symbol_;
	ExprNodePtr expr_; // may be nullptr
};
typedef std::unique_ptr<VariableNode> VariableNodePtr;

struct AssignNode : public ExprNode {
	AssignNode(TokenType token_type, const Type* type, ExprNodePtr&& left_var, ExprNodePtr&& right_expr) : 
		ExprNode(token_type, type), left_var_(std::move(left_var)), right_expr_(std::move(right_expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		left_var_->Print(oa, padding + kAstIndent);
		right_expr_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	ExprNodePtr left_var_;
	ExprNodePtr right_expr_;
};

struct BinaryOpNode : public ExprNode {
	BinaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& left_expr, ExprNodePtr&& right_expr) : 
		ExprNode(token_type, type), left_expr_(std::move(left_expr)), right_expr_(std::move(right_expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		left_expr_->Print(oa, padding + kAstIndent);
		right_expr_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	ExprNodePtr left_expr_;
	ExprNodePtr right_expr_;
};

struct UnaryOpNode : public ExprNode {
	UnaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& expr) : 
		ExprNode(token_type, type), expr_(std::move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		expr_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	ExprNodePtr expr_;
};
struct ArrayNode : public ExprNode {
	ArrayNode(TokenType token_type, const Type* type, ExprNodePtr&& base, ExprNodePtr&& index) : 
		ExprNode(token_type, type), base_(std::move(base)), index_(std::move(index)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		index_->Print(oa, padding + kAstIndent);
		base_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	ExprNodePtr base_;  // array base address
	ExprNodePtr index_;  // array index
};
typedef std::unique_ptr<ArrayNode> ArrayNodePtr;

struct CallNode : public ExprNode {
	CallNode(TokenType token_type, const Type* type, const Scope* scope, const std::string& func_name, std::vector<ExprNodePtr>&& params) :
		ExprNode(token_type, type), func_name_(func_name), scope_(scope), params_(std::move(params)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << func_name_ << std::endl;
		oa << padding << "params" << std::endl;
		for (const ExprNodePtr& ptr : params_)
			ptr->Print(oa, padding + kAstIndent);
		/*oa << padding << "body:" << std::endl;
		dynamic_cast<const FunctionSymbol*>(scope_->Get(func_name_))->body_->Print(oa, padding + kAstIndent);*/
		oa << padding + "}" << std::endl;
	}

	std::string func_name_;
	const Scope* scope_;
	std::vector<ExprNodePtr> params_;
};
typedef std::unique_ptr<CallNode> CallNodePtr;

struct DefNode : public ExprNode {
	DefNode(TokenType token_type, const Type* type, const std::string& var_name, ExprNodePtr&& expr) : 
		ExprNode(token_type, type), var_name_(var_name), expr_(std::move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << var_name_ << std::endl;
		expr_->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}

	std::string var_name_;
	ExprNodePtr expr_;
};

struct StmtBlockNode : public StmtNode {
	StmtBlockNode() :StmtNode(NT_STMT_BLOCK) {};

	void AddStmt(StmtNodePtr&& stmt) { if (stmt) stmts_.emplace_back(std::move(stmt)); }

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << kTokenTypeStr[token_type_] << " " << stmts_.size() << std::endl;
		for (const StmtNodePtr& ptr : stmts_)
			ptr->Print(oa, padding + kAstIndent);
		oa << padding + "}" << std::endl;
	}
	
private:
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

class Compiler : public StmtBlockNode {
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

	void Generate();

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		for (const FunctionSymbol* func : all_functions_) {
			const Function* func_type = dynamic_cast<const Function*>(func->type_);
			oa << padding + "{Function: " << func->name() << std::endl;
			oa << padding << "return: " << func_type->ret_type_->name()<< std::endl;
			oa << padding << "params: ";
			for (int i = 0; i < func->param_names_.size(); ++i) {
				oa << (i > 0 ? "," : "") << "(" << func_type->param_types_[i]->name() << ")" << func->param_names_[i];
			}
			oa << std::endl << padding << "body:" << std::endl;
			func->body_->Print(oa, padding + kAstIndent);
			oa << padding + "}" << std::endl;
		}
	}

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

	void ParseFuncDef1(const Type* type, const std::string &name);

	StmtNodePtr ParseStmt();

	const VariableSymbol* ParseDecl();

	ExprNodePtr ParseExpr();

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
	// TODO: literal value to symbol_table
	int64_t ParseConstInt();
	ImmediateNodePtr ParseLiteralValue();
	ExprNodePtr ParseArray(ExprNodePtr &&inherit);
	
	const Type* GetType(const std::string& type_name) const {
		return dynamic_cast<const Type*>(current_scope_->Get(type_name));
	}

private:
	Lexer lexer_;
	Scope* current_scope_;
	ScopePtr global_scope_;
	std::vector<const FunctionSymbol*> all_functions_;
};


} // namespace pswgoo
