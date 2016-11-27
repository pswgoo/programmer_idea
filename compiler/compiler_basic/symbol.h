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

class Symbol : public Object {
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
class Scope : public Object {
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
			if (!ptr->Is<ImmediateSymbol>() && !ptr->Is<LiteralSymbol>())
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
			if (ptr->Is<VariableSymbol>()) {
				ptr->index_ = top_;
				top_ += ptr->To<VariableSymbol>()->type_->SizeOf();
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
	std::string value_;
};
typedef std::unique_ptr<LiteralSymbol> LiteralSymbolPtr;

// ImmediateSymbol must know value in compiling time.
class ImmediateSymbol : public ConstSymbol {
public:
	ImmediateSymbol(const std::string& name, const Type* type, const LiteralSymbol* ptr_symbol) : ConstSymbol(name, type), literal_symbol_(ptr_symbol) {}
	const LiteralSymbol* literal_symbol_;
};

class FunctionSymbol : public ConstSymbol {
public:
	FunctionSymbol(const std::string& name, const std::vector<std::string>& param_names, AstNodePtr&& body, LocalScopePtr&& scope, const Type* type) :
		ConstSymbol(name, type), param_names_(param_names), body_(std::move(body)), scope_(std::move(scope)) {}
	void add_code(Instruction::Opcode op_code, int64_t param = -1) { code_.emplace_back(Instruction(op_code, param)); }

	std::vector<std::string> param_names_;
	std::vector<Instruction> code_;
	AstNodePtr body_;
	LocalScopePtr scope_;
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
	Float(Scope* parent_scope) : Type("float", kFloat, parent_scope) {}
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

} // namespace pswgoo
