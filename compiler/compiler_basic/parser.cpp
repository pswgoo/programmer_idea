#include "parser.h"

#include "symbol.h"
#include "instruction.h"

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
		assert(symbol != nullptr && "identifier not defined");
		if (symbol->Is<Type>()) {
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

	LocalScopePtr function_scope(new LocalScope(current_scope_));
	Scope* last_scope = current_scope_;
	current_scope_ = function_scope.get();
	vector<const Type*> param_types;
	vector<string> param_names;
	pair<int, Scope*> type_scope(type->parent_scope_->depth(), type->parent_scope_);
	while (lexer_.Current().type_ != OP_RIGHT_PARENTHESIS) {
		const VariableSymbol* var_symbol = ParseDecl();
		param_names.emplace_back(var_symbol->name());
		param_types.emplace_back(var_symbol->type_);
		if (type_scope.first < var_symbol->type_->parent_scope_->depth())
			type_scope = { var_symbol->type_->parent_scope_->depth(), var_symbol->type_->parent_scope_ };
		if (lexer_.Current().type_ == OP_COMMA)
			lexer_.ToNext();
		else if (lexer_.Current().type_ == OP_RIGHT_PARENTHESIS)
			break;
		else
			assert(0 && "Function def paramaters not match!");
	}
	lexer_.Consume(OP_RIGHT_PARENTHESIS);
	const Type* function_type = (const Type*)type_scope.second->Put(unique_ptr<Function>(new Function(type, param_types)));
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
		LocalScopePtr local_scope(new LocalScope(current_scope_));
		current_scope_ = local_scope.get();
		unique_ptr<StmtBlockNode> stmt_block(new StmtBlockNode());
		while (lexer_.Current().type_ != OP_RIGHT_BRACE) {
			stmt_block->AddStmt(move(ParseStmt()));
		}
		lexer_.Consume(OP_RIGHT_BRACE);
		if (current_scope_->parent()->Is<LocalScope>())
			current_scope_->parent()->To<LocalScope>()->add_child_scope(move(local_scope));

		current_scope_ = current_scope_->parent();
		return move(stmt_block);
	}
	case KEY_RETURN: {
		lexer_.ToNext();
		ExprNodePtr expr = ParseExpr();
		lexer_.Consume(OP_SEMICOLON);
		return StmtNodePtr(new ReturnNode(KEY_RETURN, expr->type_, move(expr)));
	}
	case KEY_BREAK: {
		lexer_.ToNext();
		lexer_.Consume(OP_SEMICOLON);
		return StmtNodePtr(new BreakNode());
	}
	case KEY_CONTINUE: {
		lexer_.ToNext();
		lexer_.Consume(OP_SEMICOLON);
		return StmtNodePtr(new ContinueNode());
	}
	case KEY_IF: {
		lexer_.ToNext();
		lexer_.Consume(OP_LEFT_PARENTHESIS);
		ExprNodePtr condition = ParseExpr();
		assert(condition->type_->type_id_ == Type::kBool && "Type is not bool in if condition!");
		lexer_.Consume(OP_RIGHT_PARENTHESIS);
		StmtNodePtr then = ParseStmt();
		StmtNodePtr els;
		if (lexer_.Current().type_ == KEY_ELSE) {
			lexer_.ToNext();
			els = ParseStmt();
		}
		return StmtNodePtr(new IfNode(move(condition), move(then), move(els)));
	}
	case KEY_FOR: {
		lexer_.ToNext();
		lexer_.Consume(OP_LEFT_PARENTHESIS);
		ExprNodePtr init = ParseExpr(); // currently only consider expression.
		lexer_.Consume(OP_SEMICOLON);
		ExprNodePtr condition = ParseExpr();
		lexer_.Consume(OP_SEMICOLON);
		ExprNodePtr iter = ParseExpr();
		lexer_.Consume(OP_RIGHT_PARENTHESIS);
		StmtNodePtr body = ParseStmt();
		return StmtNodePtr(new ForNode(move(init), move(condition), move(iter), move(body)));
	}
	case KEY_WHILE: {
		assert(0 && "Not complete while!");
		return nullptr;
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
		const Type* element_type = type;
		for (int i = (int)dims.size() - 1; i >= 0; --i) {
			TypePtr array_type(ArrayPtr(new Array(element_type, dims[i], element_type->parent_scope_)));
			element_type = element_type->parent_scope_->Put(move(array_type))->To<Type>();
			TypePtr ref_array(ReferencePtr(new Reference(element_type, element_type->parent_scope_)));
			type = element_type->parent_scope_->Put(move(ref_array))->To<Type>();
		}
		if (const Symbol* sym = current_scope_->GetCurrent(var))
			if (const VariableSymbol* ptr_var = dynamic_cast<const VariableSymbol*>(sym))
				assert(ptr_var->type_ == type && "Declare not consistence!");
		const Symbol*symbol = current_scope_->Put(VariableSymbolPtr(new VariableSymbol(var, type)));
		return symbol->To<VariableSymbol>();
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
		const Type* bool_type = GetType("bool");
		assert(inherit->type_->CouldPromoteTo(bool_type) && ptr_expr->type_->CouldPromoteTo(bool_type) && "cannot convert to bool");
		ExprNodePtr left_expr(new BinaryOpNode(op_type, bool_type, move(inherit), move(ptr_expr)));
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
		const Type* bool_type = GetType("bool");
		assert(inherit->type_->CouldPromoteTo(bool_type) && ptr_expr->type_->CouldPromoteTo(bool_type) && "cannot convert to bool");
		ExprNodePtr left_expr(new BinaryOpNode(op_type, bool_type, move(inherit), move(ptr_expr)));
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
		const Type* ptr_type = Type::Max(inherit->type_, ptr_expr->type_, true);
		ExprNodePtr left_expr(new BinaryOpNode(op_type, ptr_type, move(inherit), move(ptr_expr)));
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
		if (op_type == OP_LOGICAL_NOT) {
			type = GetType("bool");
			assert(ptr_expr->type_->CouldPromoteTo(type) && "cannot convert to bool");
		}

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
		assert(symbol && "symbol not defined");
		if (const ImmediateSymbol* immediate = dynamic_cast<const ImmediateSymbol*>(symbol)) {
			lexer_.ToNext();
			return ExprNodePtr(new ImmediateNode(IDENTIFIER, immediate->type_, immediate->literal_symbol_));
		}
		else if (const VariableSymbol* var = dynamic_cast<const VariableSymbol*>(symbol)) {
			lexer_.ToNext();
			return ParseArray(VariableNodePtr(new VariableNode(IDENTIFIER, var->type_, var)));
		}
		else if (symbol->Is<FunctionSymbol>()) {
			return ParseCall();
		}
	}
	else if (cur_type == KEY_NEW) { // new array
		lexer_.ToNext();
		const Type* element_type = current_scope_->Get(lexer_.Current().value_)->To<Type>();
		if (element_type != nullptr) {
			lexer_.ToNext();
			vector<int64_t> dims;
			while (lexer_.Current().type_ == OP_LEFT_BRACKET) {
				lexer_.Consume(OP_LEFT_BRACKET);
				dims.push_back(ParseConstInt());
				lexer_.Consume(OP_RIGHT_BRACKET);
			}
			const Type* ref_type = element_type;
			for (int i = (int)dims.size() - 1; i >= 0; --i) {
				TypePtr array_type(ArrayPtr(new Array(element_type, dims[i], element_type->parent_scope_)));
				element_type = element_type->parent_scope_->Put(move(array_type))->To<Type>();
				TypePtr ref_array(ReferencePtr(new Reference(element_type, element_type->parent_scope_)));
				ref_type = element_type->parent_scope_->Put(move(ref_array))->To<Type>();
			}
			return ExprNodePtr(new NewNode(KEY_NEW, ref_type));
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

	return CallNodePtr(new CallNode(NT_CALL, func_type->ret_type_, func_symbol, func_name, move(params)));
}

ImmediateNodePtr Compiler::ParseLiteralValue() {
	TokenType type = lexer_.Current().type_;
	const Symbol* symbol;
	switch (type) {
	case BOOLEAN:
		symbol = global_scope_->Put(LiteralSymbolPtr(new LiteralSymbol(GetType("bool"), lexer_.GoNext().value_)));
		break;
	case CHAR:
		symbol = global_scope_->Put(LiteralSymbolPtr(new LiteralSymbol(GetType("char"), lexer_.GoNext().value_)));
		break;
	case INTEGER:
		symbol = global_scope_->Put(LiteralSymbolPtr(new LiteralSymbol(GetType("int"), lexer_.GoNext().value_)));
		break;
	case REAL:
		symbol = global_scope_->Put(LiteralSymbolPtr(new LiteralSymbol(GetType("double"), lexer_.GoNext().value_)));
		break;
	case STRING: {
		const Char* ptr_char_type = GetType("char")->To<Char>();
		Scope* ptr_scope = ptr_char_type->parent_scope_;
		ArrayPtr ptr_array_type(new Array(ptr_char_type, (int64_t)lexer_.Current().value_.size(), ptr_scope));
		StringPtr ptr_string_type(new String(ptr_scope->Put(move(ptr_array_type))->To<Array>(), ptr_scope));
		symbol = global_scope_->Put(LiteralSymbolPtr(new LiteralSymbol(ptr_scope->Put(move(ptr_string_type))->To<Type>(), lexer_.GoNext().value_)));
		break;
	}
				 /*case NULL_REF:
						 return ImmediateNodePtr(new ImmediateNode(type, GetType("ref_void"), lexer_.GoNext().value_));*/
	default:
		break;
	}
	return ImmediateNodePtr(new ImmediateNode(type, symbol->To<LiteralSymbol>()->type_, symbol->To<LiteralSymbol>()));
}

ExprNodePtr Compiler::ParseArray(VariableNodePtr &&inherit) {
	if (lexer_.Current().type_ != OP_LEFT_BRACKET)
		return move(inherit);
	const VariableSymbol* ref = inherit->symbol_;
	assert(ref->type_->To<Reference>() != nullptr && "array must be reference type");
	vector<ExprNodePtr> exprs;
	const Type* ret_type = ref->type_->To<Reference>()->ref_type_;
	while (lexer_.Current().type_ == OP_LEFT_BRACKET) {
		lexer_.Consume(OP_LEFT_BRACKET);
		ExprNodePtr expr = ParseExpr();
		lexer_.Consume(OP_RIGHT_BRACKET);
		exprs.emplace_back(move(expr));

		ret_type = ret_type->To<Array>()->element_type_;
	}
	return ArrayNodePtr(new ArrayNode(NT_ARRAY, ret_type, ref, move(exprs)));
}

int64_t Compiler::ParseConstInt() {
	static const Type* ptr_int = dynamic_cast<const Type*>(global_scope_->Get("int"));
	if (lexer_.Current().type_ == INTEGER)
		return stoll(lexer_.GoNext().value_);
	else if (lexer_.Current().type_ == IDENTIFIER) {
		const ImmediateSymbol* sym = dynamic_cast<const ImmediateSymbol*>(current_scope_->Get(lexer_.GoNext().value_));
		assert(sym != nullptr && "Identifier is not ImmediateSymbol or not defined!");
		if (sym->type_->CouldPromoteTo(ptr_int)) {
			if (sym->type_->Is<Char>())
				return sym->literal_symbol_->value_.front();
			return stoll(sym->literal_symbol_->value_);
		}
	}
	assert(0 && "ParseConstInt failed!");
	return 0;
}

void Compiler::Gen() {
	for (int i = 0; i < all_functions_.size(); ++i)
		all_functions_[i]->body_->Gen(all_functions_[i], all_functions_[i]->scope_.get());
}

void ImmediateNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	switch (symbol_->type_->type_id_) {
	case Type::kBool:
		function->add_code(Instruction::kPutC, symbol_->value_ == "true");
		break;
	case Type::kChar:
		function->add_code(Instruction::kPutC, symbol_->value_.front());
		break;
	case Type::kInt:
		function->add_code(Instruction::kPutI, stoll(symbol_->value_));
		break;
	case Type::kDouble:
		function->add_code(Instruction::kPutD, stoll(symbol_->value_));
		break;
	case Type::kString:
		function->add_code(Instruction::kLdc, symbol_->index_);
	default:
		break;
	}
}

void VariableNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	if (symbol_->local_offset_ < 0) // it is a global static variable
		function->add_code(Instruction::kGetStatic, symbol_->index_);
	else
		switch (symbol_->type_->type_id_) {
		case Type::kBool:
		case Type::kChar:
			function->add_code(Instruction::kLoadC, symbol_->local_offset_);
			break;
		case Type::kInt:
			function->add_code(Instruction::kLoadI, symbol_->local_offset_);
			break;
		case Type::kDouble:
			function->add_code(Instruction::kLoadD, symbol_->local_offset_);
			break;
		case Type::kString:
		case Type::kReference:
			function->add_code(Instruction::kLoadR, symbol_->local_offset_);
			break;
		default:
			break;
	}
}

void AssignNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	switch (token_type_) {
	case TokenType::OP_ADD_ASSIGN:
	case TokenType::OP_MINUS_ASSIGN:
	case TokenType::OP_PRODUCT_ASSIGN:
	case TokenType::OP_DIVIDE_ASSIGN:
	case TokenType::OP_MOD_ASSIGN:
		assert(0 && "+= -= *= /= %= are not implement!");
	default:
		break;
	}
	right_expr_->Gen(function, local_scope);
	Type::TypeId type_id = left_var_->type_->type_id_;
	if (left_var_->token_type_ == NT_ARRAY) {
		left_var_->Gen(function, local_scope, false);
		if (type_id == Type::kBool || type_id == Type::kChar)
			function->add_code(Instruction::kAStoreC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kAStoreI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kAStoreD);
		else if (type_id == Type::kReference)
			function->add_code(Instruction::kAStoreR);
		else
			assert("array type error!");
	}
	else {
		const VariableSymbol* symbol = left_var_->To<VariableNode>()->symbol_;
		// global static variable
		if (symbol->local_offset_ < 0)
			function->add_code(Instruction::kStoreStatic, symbol->index_);
		else {
			if (type_id == Type::kBool || type_id == Type::kChar)
				function->add_code(Instruction::kStoreC, symbol->local_offset_);
			else if (type_id == Type::kInt)
				function->add_code(Instruction::kStoreI, symbol->local_offset_);
			else if (type_id == Type::kDouble)
				function->add_code(Instruction::kStoreD, symbol->local_offset_);
			else if (type_id == Type::kReference)
				function->add_code(Instruction::kStoreR, symbol->local_offset_);
			else
				assert("variable type error!");
		}
	}
}

void BinaryOpNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	left_expr_->Gen(function, local_scope);
	const Type* common_type = Type::Max(left_expr_->type_, right_expr_->type_);
	//if (type_->Is<Bool>())
	//	common_type = type_;
	if (left_expr_->type_ != common_type)
		function->add_code(Type::GetConvertOpcode(left_expr_->type_, common_type));
	right_expr_->Gen(function, local_scope);
	if (right_expr_->type_ != common_type)
		function->add_code(Type::GetConvertOpcode(right_expr_->type_, common_type));
	int type_id = common_type->type_id_;
	switch (token_type_) {
	case pswgoo::OP_ADD:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kAddC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kAddI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kAddD);
		return;
	case pswgoo::OP_MINUS:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kSubC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kSubI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kSubD);
		return;
	case pswgoo::OP_PRODUCT:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kMulC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kMulI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kMulD);
		return;
	case pswgoo::OP_DIVIDE:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kDivC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kDivI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kDivD);
		return;
	case pswgoo::OP_MOD:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kModC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kModI);
		return;
	case pswgoo::OP_LOGICAL_AND:
		function->add_code(Instruction::kAnd);
		return;
	case pswgoo::OP_LOGICAL_OR:
		function->add_code(Instruction::kOr);
		return;
	case pswgoo::OP_GREATER:
	case pswgoo::OP_LESS:
	case pswgoo::OP_EQUAL:
	case pswgoo::OP_NOT_EQUAL:
	case pswgoo::OP_GREATER_EQUAL:
	case pswgoo::OP_LESS_EQUAL:
		if (type_id == Type::kChar)
			function->add_code(Instruction::kCmpC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kCmpI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kCmpD);
		else
			assert(0 && "cmp type error!");
		if (token_type_ == OP_GREATER)
			function->add_code(Instruction::kGt);
		else if (token_type_ == OP_LESS)
			function->add_code(Instruction::kLt);
		else if (token_type_ == OP_EQUAL)
			function->add_code(Instruction::kEq);
		else if (token_type_ == OP_NOT_EQUAL)
			function->add_code(Instruction::kNe);
		else if (token_type_ == OP_GREATER_EQUAL)
			function->add_code(Instruction::kGe);
		else if (token_type_ == OP_LESS_EQUAL)
			function->add_code(Instruction::kLe);
		return;
	default:
		assert(0 && "binary operator not implemented");
		break;
	}
	assert(0 && "binary_node gen error, type error!");
}

void UnaryOpNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	expr_->Gen(function, local_scope);
	switch (token_type_) {
	case pswgoo::NT_TYPE_CAST: {
		Instruction::Opcode op = Type::GetConvertOpcode(expr_->type_, type_);
		//std::clog << "Gen TYPE CAST, " << expr_->type_->name() << " <  " << type_->name() << " " << kInstructionStr[op] << std::endl;
		if (op != Instruction::kNonCmd)
			function->add_code(op);
		else
			assert(expr_->type_ == type_ && "no matched type cast opcode");
		return;
	}
	case pswgoo::OP_ADD:
		return;
	case pswgoo::OP_MINUS:
		if (type_->Is<Char>())
			function->add_code(Instruction::kNegC);
		else if (type_->Is<Int>())
			function->add_code(Instruction::kNegI);
		else if (type_->Is<Double>())
			function->add_code(Instruction::kNegD);
		else
			assert("type error in unary minus");
		return;
	case pswgoo::OP_LOGICAL_NOT:
		function->add_code(Instruction::kNot);
		break;
	default:
		break;
	};
}

void ArrayNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	const Type* type = ref_->type_->To<Reference>()->ref_type_;
	int64_t unit_element_size = type->To<Array>()->UnitSize();
	function->add_code(Instruction::kPutI, 0);	// put base index 0.
	for (int i = 0; i < indices_.size(); ++i) {
		indices_[i]->Gen(function, local_scope);
		if (indices_[i]->type_->type_id_ != Type::kInt) {
			Instruction::Opcode op = Type::GetConvertOpcode(indices_[i]->type_, local_scope->Get("int")->To<Type>());
			assert(op != Instruction::kNonCmd && "array index cannot convert to int");
			function->add_code(op);
		}
		function->add_code(Instruction::kPutI, type->To<Array>()->element_type_->SizeOf() / unit_element_size);
		function->add_code(Instruction::kMulI);
		function->add_code(Instruction::kAddI);
		type = type->To<Array>()->element_type_;
	}
	if (ref_->local_offset_ < 0)
		function->add_code(Instruction::kGetStatic, ref_->index_);
	else
		function->add_code(Instruction::kLoadR, ref_->local_offset_);
	if (right_value) {
		Type::TypeId type_id = type_->type_id_;
		if (type_id == Type::kBool || type_id == Type::kChar)
			function->add_code(Instruction::kALoadC);
		else if (type_id == Type::kInt)
			function->add_code(Instruction::kALoadI);
		else if (type_id == Type::kDouble)
			function->add_code(Instruction::kALoadD);
		else if (type_id == Type::kReference)
			function->add_code(Instruction::kALoadR);
		else
			assert("array dereference type error!");
	}
}

void CallNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	for (int i = 0; i < params_.size(); ++i)
		params_[i]->Gen(function, local_scope);
	function->add_code(Instruction::kCall, function_->index_);
}

void StmtBlockNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const {
	for (int i = 0; i < stmts_.size(); ++i)
		stmts_[i]->Gen(function, local_scope, right_value);
}

void StmtBlockNode::Gen(FunctionSymbol * function, LocalScope * local_scope, std::vector<int64_t>& break_instrs, std::vector<int64_t>& continue_instrs, bool right_value) const {
	for (int i = 0; i < stmts_.size(); ++i)
		stmts_[i]->Gen(function, local_scope, break_instrs, continue_instrs, right_value);
}

void NewNode::Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const {
	const Type* ref_type = type_->To<Reference>()->ref_type_;
	if (ref_type->Is<Array>()) {
		function->add_code(Instruction::kPutI, ref_type->To<Array>()->SizeOf() / ref_type->To<Array>()->UnitSize());
		function->add_code(Instruction::kNewA, ref_type->index_);
	}
}

void ReturnNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {
	const Type* ret_type = function->type_->To<Function>()->ret_type_;
	if (expr_ != nullptr) {
		expr_->Gen(function, local_scope, right_value);
		assert(expr_->type_->CouldPromoteTo(ret_type) && "return type not match!");
		Instruction::Opcode op = Type::GetConvertOpcode(expr_->type_, ret_type);
		if (op != Instruction::kNonCmd)
			function->add_code(op);

		if (expr_->type_->type_id_ == Type::kBool || expr_->type_->type_id_ == Type::kChar)
			function->add_code(Instruction::kReturnC);
		else if (expr_->type_->type_id_ == Type::kInt)
			function->add_code(Instruction::kReturnI);
		else if (expr_->type_->type_id_ == Type::kDouble)
			function->add_code(Instruction::kReturnD);
		else if (expr_->type_->Is<Reference>())
			function->add_code(Instruction::kReturnR);
		else
			assert(0 && "Return type invalid!");
	}
	else {
		assert(ret_type == nullptr && "this function's return type is void!");
		function->add_code(Instruction::kReturn);
	}
}

void IfNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {
	vector<int64_t> break_instrs, continue_instrs;
	Gen(function, local_scope, break_instrs, continue_instrs,right_value);
}

void IfNode::Gen(FunctionSymbol * function, LocalScope * local_scope, std::vector<int64_t>& break_instrs, std::vector<int64_t>& continue_instrs, bool right_value) const {
	assert(condition_->type_->type_id_ == Type::kBool && "condition's type is not bool!");
	condition_->Gen(function, local_scope, right_value);
	function->add_code(Instruction::kIfFalse);
	int64_t false_instr = (int64_t)function->code_.size() - 1;
	then_->Gen(function, local_scope, break_instrs, continue_instrs, right_value);
	if (else_ != nullptr) {
		function->add_code(Instruction::kGoto);
		int64_t then_break_instr = (int)function->code_.size() - 1;
		function->code_[false_instr].param_ = (int64_t)function->code_.size(); // backpatch instr tag
		else_->Gen(function, local_scope, break_instrs, continue_instrs, right_value);
		function->code_[then_break_instr].param_ = (int64_t)function->code_.size(); // backpatch instr tag
	}
	else
		function->code_[false_instr].param_ = (int64_t)function->code_.size(); // backpatch instr tag
}

void WhileNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {

}

static void GenStmtNodeCode(FunctionSymbol* function, LocalScope* local_scope, bool right_value, vector<int64_t>& break_instrs, int64_t iter_instr, const StmtNodePtr& stmt) {
	if (stmt->Is<StmtBlockNode>()) {
		for (const StmtNodePtr& ptr : stmt->To<StmtBlockNode>()->stmts_)
			GenStmtNodeCode(function, local_scope, right_value, break_instrs, iter_instr, ptr);
	}
	else if (stmt->Is<StmtNode>()) {
		stmt->Gen(function, local_scope, right_value);
		if (stmt->Is<BreakNode>())
			break_instrs.push_back(function->code_.size() - 1);
		if (stmt->Is<ContinueNode>())
			function->code_.back().param_ = iter_instr;
	}
	else
		assert(0 && "Stmt type not defined!");
}

void ForNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {
	init_->Gen(function, local_scope, right_value);
	function->add_code(Instruction::kGoto);
	int64_t init_goto_condition_instr = function->code_.size() - 1;

	int64_t iter_instr = (int64_t)function->code_.size();
	iter_->Gen(function, local_scope, right_value);

	function->code_[init_goto_condition_instr].param_ = function->code_.size();

	condition_->Gen(function, local_scope, right_value);

	vector<int64_t> break_instrs;
	function->add_code(Instruction::kIfFalse);
	break_instrs.push_back(function->code_.size() - 1);

	vector<int64_t> continue_instrs;

	body_->Gen(function, local_scope, break_instrs, continue_instrs, right_value);
	function->add_code(Instruction::kGoto, iter_instr);

	// backpatch break_instrs
	for (int64_t instr : break_instrs)
		function->code_[instr].param_ = function->code_.size();
	// backpatch continue_instrs
	for (int64_t instr : continue_instrs)
		function->code_[instr].param_ = iter_instr;
}

void BreakNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {
	function->add_code(Instruction::kGoto);
}

void ContinueNode::Gen(FunctionSymbol * function, LocalScope * local_scope, bool right_value) const {
	function->add_code(Instruction::kGoto);
}

} // namespace pswgoo
