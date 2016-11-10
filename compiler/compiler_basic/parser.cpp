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
	StmtNodePtr body;
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
	if (lexer_.Current().type_ == OP_LEFT_BRACE)
		body = ParseStmt();
	lexer_.Consume(OP_RIGHT_BRACE);

	const Type* function_type = (const Type*)last_scope->Put(unique_ptr<Function>(new Function(type, params)));
	last_scope->Put(unique_ptr<FunctionSymbol>(new FunctionSymbol(name, move(body), move(function_scope), function_type)));
	return nullptr;
}

VariableSymbol Compiler::ParseDecl() {
	return VariableSymbol();
}

StmtNodePtr Compiler::ParseStmt() {
	return nullptr;
}

} // namespace pswgoo