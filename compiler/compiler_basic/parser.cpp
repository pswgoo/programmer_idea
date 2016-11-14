#include "parser.h"

using namespace std;

namespace pswgoo {

void Compiler::Parse(const std::string& program) {
	lexer_.Tokenize(program);
	while (!lexer_.Current().Non()) {
		StmtNodePtr ptr = Parse();
		if (ptr != nullptr)
			AddStmt(move(ptr));
	}
}

StmtNodePtr Compiler::Parse() {
	const Lexer::Token* tk = &(lexer_.GoNext());
	if (tk->type_ == IDENTIFIER) {
		const Symbol* symbol = current_scope_->Get(tk->value_);
		if (symbol != nullptr && symbol->Is<Type>()) {
			tk = &(lexer_.GoNext());
			if (tk->type_ == IDENTIFIER) {
				ParseFuncDef1((const Type*)symbol, tk->value_);
				return nullptr;
			}
		}
	}
	return nullptr;
}

void Compiler::ParseFuncDef1(const Type* type, const std::string &name) {
	lexer_.Consume(OP_LEFT_PARENTHESIS);

	ScopePtr function_scope(new Scope(current_scope_));
	Scope* last_scope = current_scope_;
	current_scope_ = function_scope.get();
	vector<const Type*> param_types;
	vector<string> param_names;
	pair<int, Scope*> type_scope_(type->parent_scope_->depth(), type->parent_scope_);
	while (lexer_.Current().type_ != OP_RIGHT_PARENTHESIS) {
		const VariableSymbol* var_symbol = ParseDecl();
		param_names.emplace_back(var_symbol->name());
		param_types.emplace_back(var_symbol->type_);
		if (type_scope_.first < var_symbol->type_->parent_scope_->depth())
			type_scope_ = { var_symbol->type_->parent_scope_->depth(), var_symbol->type_->parent_scope_ };
		if (lexer_.Current().type_ == OP_COMMA)
			lexer_.ToNext();
		else if (lexer_.Current().type_ == OP_RIGHT_PARENTHESIS)
			break;
		else
			assert(0 && "Function def paramaters not match!");
	}
	lexer_.Consume(OP_RIGHT_PARENTHESIS);
	const Type* function_type = (const Type*)type_scope_.second->Put(unique_ptr<Function>(new Function(type, param_types)));
	FunctionSymbolPtr func_ptr(new FunctionSymbol(name, param_names, nullptr, move(function_scope), function_type));
	// left_func_ptr used to change function body.
	FunctionSymbol* left_func_ptr = func_ptr.get();
	// To support recursion, put function_symbol to scope first.
	last_scope->Put(move(func_ptr));

	StmtNodePtr body;
	if (lexer_.Current().type_ == OP_LEFT_BRACE)
		body = ParseStmt();
	else
		assert(0 && "Function Body must begin with left brace!");
	left_func_ptr->body_ = move(body);
	current_scope_ = last_scope;
	all_functions_.push_back(left_func_ptr);
}

StmtNodePtr Compiler::ParseStmt() {
	switch (lexer_.Current().type_) {
	case OP_LEFT_BRACE: {
		lexer_.Consume(OP_LEFT_BRACE);
		unique_ptr<StmtBlockNode> stmt_block(new StmtBlockNode());
		while (lexer_.Current().type_ != OP_RIGHT_BRACE) {
			stmt_block->AddStmt(move(ParseStmt()));
			//lexer_.Consume(OP_SEMICOLON);
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
	default: // Expression
		StmtNodePtr expr = ParseExpr();
		lexer_.Consume(OP_SEMICOLON);
		return move(expr);
	}
	return nullptr;
}

const VariableSymbol* Compiler::ParseDecl() {
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
			TypePtr array_type(ArrayPtr(new Array(type, dims[i], type->parent_scope_)));
			type = dynamic_cast<const Type*>(type->parent_scope_->Put(move(array_type)));
		}
		if (dims.size() > 0) {
			TypePtr ref_array(ReferencePtr(new Reference(type, type->parent_scope_)));
			type = dynamic_cast<const Type*>(type->parent_scope_->Put(move(ref_array)));
		}
		if (const Symbol* sym = current_scope_->GetCurrent(var))
			if (const VariableSymbol* ptr_var = dynamic_cast<const VariableSymbol*>(sym))
				assert(ptr_var->type_ == type && "Declare not consistence!");
		const Symbol*symbol = current_scope_->Put(unique_ptr<VariableSymbol>(new VariableSymbol(var, type)));
		return dynamic_cast<const VariableSymbol*>(symbol);
	}
	else// if (lexer_.Current().type_ == OP_LOGICAL_AND)
		assert(0 && "Declare reference not implement!");

	return nullptr;
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
		assert(ptr_expr_r->type_->CouldPromoteTo(inherit->type_) && "Type narrowed!");
		return ExprNodePtr(new AssignNode(op_type, inherit->type_, move(inherit), move(ptr_expr_r)));
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
		assert(symbol && (lexer_.Current().value_ + " not defined").c_str());
		if (const ImmediateSymbol* immediate = dynamic_cast<const ImmediateSymbol*>(symbol)) {
			lexer_.ToNext();
			return ExprNodePtr(new ImmediateNode(IDENTIFIER, immediate->type_, immediate->value_));
		}
		else if (const VariableSymbol* var = dynamic_cast<const VariableSymbol*>(symbol)) {
			lexer_.ToNext();
			return ParseArray(ExprNodePtr(new VariableNode(IDENTIFIER, var->type_, var->name(), nullptr)));
		}
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
	string func_name = lexer_.GoNext().value_;
	const FunctionSymbol* func_symbol = dynamic_cast<const FunctionSymbol*>(current_scope_->Get(func_name));
	assert(func_symbol && (func_name + " is not a function symbol").c_str());
	lexer_.Consume(OP_LEFT_PARENTHESIS);
	vector<ExprNodePtr> params;
	while (lexer_.Current().type_ != OP_RIGHT_PARENTHESIS) {
		params.emplace_back(ParseExpr());
		if (lexer_.Current().type_ == OP_COMMA)
			lexer_.Consume(OP_COMMA);
		else if (lexer_.Current().type_ != OP_RIGHT_PARENTHESIS)
			assert(0 && "Invalid function paramater list");
	}
	lexer_.Consume(OP_RIGHT_PARENTHESIS);
	const Function* func_type = dynamic_cast<const Function*>(func_symbol->type_);
	bool param_match = params.size() == func_type->param_types_.size();
	assert(param_match && "Function paramater number not match!");
	for (int i = 0; i < params.size(); ++i)
		if (!(params[i]->type_->CouldPromoteTo(func_type->param_types_[i]))) {
			param_match = false;
			assert(0 && "Function paramater not compatible!");
		}
			
	return CallNodePtr(new CallNode(NT_CALL, func_type->ret_type_, current_scope_, func_name, move(params)));
}

ImmediateNodePtr Compiler::ParseLiteralValue() {
	TokenType type = lexer_.Current().type_;
	switch (type) {
	case BOOLEAN:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("bool"), lexer_.GoNext().value_));
	case CHAR:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("char"), lexer_.GoNext().value_));
	case INTEGER:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("int"), lexer_.GoNext().value_));
	case REAL:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("double"), lexer_.GoNext().value_));
	/*case STRING:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("string"), lexer_.GoNext().value_));*/
	/*case NULL_REF:
		return ImmediateNodePtr(new ImmediateNode(type, GetType("ref_void"), lexer_.GoNext().value_));*/
	default:
		break;
	}
	return nullptr;
}

ExprNodePtr Compiler::ParseArray(ExprNodePtr &&inherit) {
	ExprNodePtr ret(move(inherit));
	while (lexer_.Current().type_ == OP_LEFT_BRACKET) {
		lexer_.Consume(OP_LEFT_BRACKET);
		ExprNodePtr expr = ParseExpr();
		lexer_.Consume(OP_RIGHT_BRACKET);

		const Type* type = nullptr;
		if (const Reference* ref_type = dynamic_cast<const Reference*>(ret->type_))
			type = ref_type->Get<Array>()->element_type_;
		else
			type = dynamic_cast<const Array*>(ret->type_)->element_type_;
		ArrayNodePtr arr(new ArrayNode(NT_ARRAY, type, move(ret), move(expr)));
		ret = move(arr);
	}
	return move(ret);
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
