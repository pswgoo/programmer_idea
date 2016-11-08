#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace pswgoo {

class Symbol {
	std::string name_;
};
typedef std::unique_ptr<Symbol> SymbolPtr;

class Type : public Symbol {
public:
	enum TypeId {kBool, kChar, kInt, kLong, kFloat, kDouble, kPrimitiveType, kArray, kReference, kFunction, kClass};

	virtual int SizeOf() const; // = 0

	TypeId type_id_;
};

class Bool : public Type {

};

class Char : public Type {

};

class Int : public Type {

};

class Long : public Type {

};

class Float : public Type {

};

class Double : public Type {

};

class Array : public Type {
	Type element_type_;
	int64_t length_;
};

class Reference : public Type {
	Type target_type_;
};

class Function : public Type {

};

class Class : public Type {

};

class VariableSymbol : public Symbol {
	Type type_;

};

class ConstSymbol : public Symbol {
	Type type_;
	std::string value_;
};

class Scope {
public:
	Symbol* Get(const std::string& name);
	void Put(SymbolPtr&& symbol_ptr);

private:
	Scope* parent_;
	std::unordered_map<std::string, SymbolPtr> symbol_table_;

};

class AstNode {
	
};

} // namespace pswgoo