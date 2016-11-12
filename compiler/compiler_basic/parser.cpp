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
	ExprNodePtr left_ptr = ParseE1();
	return ParseER(move(left_ptr));
}
ExprNodePtr Compiler::ParseER(ExprNodePtr &&inherit) {
	switch (TokenType op_type = lexer_.Current().type_) {
	case TokenType::OP_ASSIGN:
	case TokenType::OP_ADD_ASSIGN:
	case TokenType::OP_MINUS_ASSIGN:
	case TokenType::OP_PRODUCT_ASSIGN:
	case TokenType::OP_DIVIDE_ASSIGN:
	case TokenType::OP_MOD_ASSIGN: {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE1();
		ExprNodePtr ptr_expr_r = ParseER(move(ptr_expr));
		assert(inherit->Is<VariableNode>() && "Assign node's left operand must be VariableNode!");
		VariableNodePtr var_ptr(dynamic_cast<VariableNode*>(inherit.release()));
		return ExprNodePtr(new AssignNode(op_type, var_ptr->type_, move(var_ptr), move(ptr_expr_r)));
	}
	default:
		break;
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE1() {
	ExprNodePtr left_ptr = ParseE2();
	return ParseE1R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE1R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_LOGICAL_OR) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE2();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, GetType("bool"), move(inherit), move(ptr_expr)));
		return ParseE1R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE2() {
	ExprNodePtr left_ptr = ParseE3();
	return ParseE2R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE2R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_LOGICAL_AND) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE3();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, GetType("bool"), move(inherit), move(ptr_expr)));
		return ParseE2R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE3() {
	ExprNodePtr left_ptr = ParseE4();
	return ParseE3R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE3R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_BIT_OR) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE4();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, Type::Max(inherit->type_, ptr_expr->type_, true), move(inherit), move(ptr_expr)));
		return ParseE3R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE4() {
	ExprNodePtr left_ptr = ParseE5();
	return ParseE4R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE4R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_BIT_XOR) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE5();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, Type::Max(inherit->type_, ptr_expr->type_, true), move(inherit), move(ptr_expr)));
		return ParseE4R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE5() {
	ExprNodePtr left_ptr = ParseE6();
	return ParseE5R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE5R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_BIT_AND) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE6();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, Type::Max(inherit->type_, ptr_expr->type_, true), move(inherit), move(ptr_expr)));
		return ParseE5R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE6() {
	ExprNodePtr left_ptr = ParseE7();
	return ParseE6R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE6R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_EQUAL || op_type == OP_NOT_EQUAL) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE7();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, GetType("bool"), move(inherit), move(ptr_expr)));
		return ParseE6R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE7() {
	ExprNodePtr left_ptr = ParseE8();
	return ParseE7R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE7R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_LESS || op_type == OP_LESS_EQUAL || op_type == OP_GREATER || op_type == OP_GREATER_EQUAL) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE8();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, GetType("bool"), move(inherit), move(ptr_expr)));
		return ParseE7R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE8() {
	ExprNodePtr left_ptr = ParseE9();
	return ParseE8R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE8R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_ADD || op_type == OP_MINUS) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE9();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, Type::Max(inherit->type_, ptr_expr->type_, true), move(inherit), move(ptr_expr)));
		return ParseE8R(move(left_expr));
	}
	return move(inherit);
}
ExprNodePtr Compiler::ParseE9() {
	ExprNodePtr left_ptr = ParseE10();
	return ParseE9R(move(left_ptr));
}
ExprNodePtr Compiler::ParseE9R(ExprNodePtr &&inherit) {
	TokenType op_type = lexer_.Current().type_;
	if (op_type == OP_PRODUCT || op_type == OP_DIVIDE) {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE10();
		ExprNodePtr left_expr(new BinaryOpNode(op_type, Type::Max(inherit->type_, ptr_expr->type_, true), move(inherit), move(ptr_expr)));
		return ParseE9R(move(left_expr));
	}
	return move(inherit);
}

ExprNodePtr Compiler::ParseE10() {
	TokenType op_type = lexer_.Current().type_;
	switch (op_type) {
	case TokenType::OP_LOGICAL_NOT:
	case TokenType::OP_BIT_NOT:
	case TokenType::OP_ADD:
	case TokenType::OP_MINUS: {
		lexer_.ToNext();
		ExprNodePtr ptr_expr = ParseE11();
		const Type* type = ptr_expr->type_;
		if (op_type == OP_LOGICAL_NOT)
			type = GetType("bool");
		return ExprNodePtr(new UnaryOpNode(op_type, type, move(ptr_expr)));
	}
	default:
		break;
	}
	return ParseE11();
}

ExprNodePtr Compiler::ParseE11() {
	TokenType cur_type = lexer_.Current().type_;
	if (cur_type == OP_LEFT_PARENTHESIS) {
		lexer_.ToNext();
		if (CurrentIsType()) {
			const Type* type = GetType(lexer_.Current().value_);
			lexer_.ToNext();
			lexer_.Consume(TokenType::OP_RIGHT_PARENTHESIS);
			ExprNodePtr right_ptr = ParseExpr();
			return ExprNodePtr(new UnaryOpNode(NT_TYPE_CAST, type, move(right_ptr)));
		}
		else {
			ExprNodePtr left_ptr = ParseExpr();
			lexer_.Consume(OP_RIGHT_PARENTHESIS);
			return move(left_ptr);
		}
	}
	else if (cur_type == IDENTIFIER) {
		const Symbol* symbol = current_scope_->Get(lexer_.Current().value_);
		assert(symbol && lexer_.Current().value_ + " not defined");
		if (const ImmediateSymbol* immediate = dynamic_cast<const ImmediateSymbol*>(symbol))
			return ExprNodePtr(new ImmediateNode(IDENTIFIER, immediate->type_, immediate->value_));
		else if (const VariableSymbol* var = dynamic_cast<const VariableSymbol*>(symbol))
			return ExprNodePtr(new VariableNode(IDENTIFIER, var->type_, var->name()));
		else if (symbol->Is<FunctionSymbol>()) {
			return ParseCall();
		}
	}
	else
		return ParseLiteralValue();
	throw runtime_error("ClExprNode:: Parse11 error!");
	return nullptr;
}
CallNodePtr Compiler::ParseCall() {
	return nullptr;
}
ImmediateNodePtr Compiler::ParseLiteralValue() {
	TokenType type = lexer_.Current().type_;
	switch (type) {
	case BOOLEAN:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("bool"), lexer_.Current().value_));
	case CHAR:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("char"), lexer_.Current().value_));
	case INTEGER:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("int"), lexer_.Current().value_));
	case REAL:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("double"), lexer_.Current().value_));
	/*case STRING:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("string"), lexer_.Current().value_));*/
	/*case NULL_REF:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("ref_void"), lexer_.Current().value_));*/
	default:
		break;
	}
	return nullptr;
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
