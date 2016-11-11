#include "parser.h"

using namespace std;

namespace pswgoo {

void Compiler::Parse(const std::string& program) {
	lexer_.Tokenize(program);
	while (!lexer_.Current().Non()) {
		StmtNodePtr ptr = Parse();
		if (ptr != nullptr)
			stmt_block_.stmts_.emplace_back(move(ptr));
	}
}

StmtNodePtr Compiler::Parse() {
	const Lexer::Token* tk = &lexer_.Current();
	if (tk->type_ == IDENTIFIER) {
		const Symbol* symbol = current_scope_->Get(tk->value_);
		if (symbol != nullptr && symbol->Is<Type>()) {
			tk = &lexer_.ToNext();
			if (tk->type_ == IDENTIFIER)
				return ParseFuncDef1((const Type*)symbol, tk->value_);
		}
	}
	return nullptr;
}

StmtNodePtr Compiler::ParseFuncDef1(const Type* type, const std::string &name) {
	lexer_.Consume(OP_LEFT_PARENTHESIS);
	
	ScopePtr function_scope(new Scope(current_scope_));
	Scope* last_scope = current_scope_;
	current_scope_ = function_scope.get();
	vector<VariableSymbol> params;
	while (true) {
		params.emplace_back(ParseDecl());
		if (lexer_.Current().type_ == OP_COMMON)
			lexer_.ToNext();
		else if (lexer_.Current().type_ == OP_RIGHT_PARENTHESIS) {
			lexer_.ToNext();
			break;
		}
		else
			assert(0 && "Function def paramaters not match!");
	}

	StmtNodePtr body;
	if (lexer_.Current().type_ == OP_LEFT_BRACE)
		body = ParseStmt();
	else
		assert(0 && "Function Body must begin with left brace!");

	const Type* function_type = (const Type*)last_scope->Put(unique_ptr<Function>(new Function(type, params)));
	last_scope->Put(unique_ptr<FunctionSymbol>(new FunctionSymbol(name, move(body), move(function_scope), function_type)));
	current_scope_ = last_scope;
	return nullptr;
}

StmtNodePtr Compiler::ParseStmt() {
	switch (lexer_.Current().type_) {
	case OP_LEFT_BRACE: {
		lexer_.Consume(OP_LEFT_BRACE);
		unique_ptr<StmtBlockNode> stmt_block;
		while (lexer_.Current().type_ != OP_RIGHT_BRACE) {
			stmt_block_.stmts_.emplace_back(move(ParseStmt()));
			lexer_.Consume(OP_SEMICOLON);
		}
		lexer_.Consume(OP_RIGHT_BRACE);
		return move(stmt_block);
	}		
	case IDENTIFIER:
		if (CurrentIsType()) {
			ParseDecl();
			lexer_.Consume(OP_SEMICOLON);
			return nullptr;
		}
	default:
		StmtNodePtr expr = ParseExpr();
		lexer_.Consume(OP_SEMICOLON);
		return move(expr);
	}
	return nullptr;
}

VariableSymbol Compiler::ParseDecl() {
	const Type* type = dynamic_cast<const Type*>(current_scope_->Get(lexer_.GoNext().value_));
	assert(type != nullptr && "Type not exists!");
	if (lexer_.Current().type_ == IDENTIFIER) {
		string var = lexer_.GoNext().value_;
		vector<int64_t> dims;
		while (lexer_.Current().type_ == OP_LEFT_BRACKET) {
			lexer_.Consume(OP_LEFT_BRACKET);
			dims.push_back(ParseConstInt());
			lexer_.Consume(OP_RIGHT_BRACKET);
		}
		for (int i = (int)dims.size() - 1; i >= 0; --i) {
			TypePtr array_type(unique_ptr<Array>(new Array(type, dims[i], type->parent_scope_)));
			type = dynamic_cast<const Type*>(type->parent_scope_->Put(move(array_type)));
		}
		if (const Symbol* sym = current_scope_->GetCurrent(var))
			if (const VariableSymbol* ptr_var = dynamic_cast<const VariableSymbol*>(sym))
				assert(ptr_var->type_ == type && "Declare not consistence!");	
		current_scope_->Put(unique_ptr<VariableSymbol>(new VariableSymbol(var, type)));
	}
	else// if (lexer_.Current().type_ == OP_LOGICAL_AND)
		assert(0 && "Declare reference not implement!");

	return VariableSymbol("", nullptr);
}

ExprNodePtr Compiler::ParseExpr() {

	return ExprNodePtr();
}

int64_t Compiler::ParseConstInt() {
	static const Type* ptr_long = dynamic_cast<const Type*>(global_scope_->Get("long"));
	if (lexer_.Current().type_ == INTEGER)
		return stoll(lexer_.GoNext().value_);
	else if (lexer_.Current().type_ == IDENTIFIER) {
		const ImmediateSymbol* sym = dynamic_cast<const ImmediateSymbol*>(current_scope_->Get(lexer_.GoNext().value_));
		assert(sym != nullptr && "Identifier is not ImmediateSymbol or not defined!");
		if (sym->type_->CouldPromoteTo(ptr_long)) {
			if (sym->type_->Is<Char>())
				return sym->value_.front();
			return stoll(sym->value_);
		}
	}
	assert(0 && "ParseConstInt failed!");
	return 0;
}

} // namespace pswgoo
