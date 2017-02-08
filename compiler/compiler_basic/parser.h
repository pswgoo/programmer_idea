#pragma once

#include "symbol.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace pswgoo {

struct StmtNode : public AstNode {
	StmtNode(TokenType token_type = NOT_DEFINED) : AstNode(token_type) {}
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const = 0;
};
typedef std::unique_ptr<StmtNode> StmtNodePtr;

struct ExprNode : public StmtNode {
	ExprNode(TokenType token_type, const Type* type) : StmtNode(token_type), type_(type) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override{
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << "}" << std::endl;
	}
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const = 0;

	const Type* type_;
};
typedef std::unique_ptr<ExprNode> ExprNodePtr;

struct ImmediateNode : public ExprNode {
	ImmediateNode(TokenType token_type, const Type* type, const LiteralSymbol* symbol) :
		ExprNode(token_type, type), symbol_(symbol) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << symbol_->value_ << "}" << std::endl;
	}
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	const LiteralSymbol* symbol_;
};
typedef std::unique_ptr<ImmediateNode> ImmediateNodePtr;

struct VariableNode : public ExprNode {
	VariableNode(TokenType token_type, const Type* type, const VariableSymbol* symbol) : 
		ExprNode(token_type, type), symbol_(symbol) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding + kIndent << symbol_->name() << std::endl;
		//if (expr_)
		//	expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	const VariableSymbol* symbol_;
	//ExprNodePtr expr_; // may be nullptr
};
typedef std::unique_ptr<VariableNode> VariableNodePtr;

struct AssignNode : public ExprNode {
	AssignNode(TokenType token_type, const Type* type, ExprNodePtr&& left_var, ExprNodePtr&& right_expr) : 
		ExprNode(token_type, type), left_var_(std::move(left_var)), right_expr_(std::move(right_expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		left_var_->Print(oa, padding + kIndent);
		right_expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	ExprNodePtr left_var_;
	ExprNodePtr right_expr_;
};

struct BinaryOpNode : public ExprNode {
	BinaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& left_expr, ExprNodePtr&& right_expr) : 
		ExprNode(token_type, type), left_expr_(std::move(left_expr)), right_expr_(std::move(right_expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		left_expr_->Print(oa, padding + kIndent);
		right_expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	ExprNodePtr left_expr_;
	ExprNodePtr right_expr_;
};

struct UnaryOpNode : public ExprNode {
	UnaryOpNode(TokenType token_type, const Type* type, ExprNodePtr&& expr) : 
		ExprNode(token_type, type), expr_(std::move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	ExprNodePtr expr_;
};
struct ArrayNode : public ExprNode {
	ArrayNode(TokenType token_type, const Type* type, const VariableSymbol* ref, std::vector<ExprNodePtr>&& indices) :
		ExprNode(token_type, type), ref_(ref), indices_(std::move(indices)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		for (const ExprNodePtr& index : indices_)
			index->Print(oa, padding + kIndent);
		oa << padding + kIndent + "{" << "(" << type_->name() << ")" << ref_->name_ << "}" << std::endl;
		oa << padding + "}" << std::endl;
	}
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	const VariableSymbol *ref_;				// array base address
	std::vector<ExprNodePtr> indices_;  // array index, type must can be prompt to Int
};
typedef std::unique_ptr<ArrayNode> ArrayNodePtr;

struct CallNode : public ExprNode {
	CallNode(TokenType token_type, const Type* type, const FunctionSymbol* function, const std::string& func_name, std::vector<ExprNodePtr>&& params) :
		ExprNode(token_type, type), func_name_(func_name), function_(function), params_(std::move(params)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << func_name_ << std::endl;
		oa << padding << "params" << std::endl;
		for (const ExprNodePtr& ptr : params_)
			ptr->Print(oa, padding + kIndent);
		/*oa << padding << "body:" << std::endl;
		dynamic_cast<const FunctionSymbol*>(scope_->Get(func_name_))->body_->Print(oa, padding + kIndent);*/
		oa << padding + "}" << std::endl;
	}

	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

	std::string func_name_;
	const FunctionSymbol* function_;
	std::vector<ExprNodePtr> params_;
};
typedef std::unique_ptr<CallNode> CallNodePtr;

struct DefNode : public ExprNode {
	DefNode(TokenType token_type, const Type* type, const std::string& var_name, ExprNodePtr&& expr) : 
		ExprNode(token_type, type), var_name_(var_name), expr_(std::move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << var_name_ << std::endl;
		expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	std::string var_name_;
	ExprNodePtr expr_;
};

struct NewNode : public ExprNode {
	NewNode(TokenType token_type, const Type* type): ExprNode(token_type, type){}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << "new" << std::endl;
		oa << padding << type_->To<Reference>()->ref_type_->name() << std::endl;
		oa << padding + "}" << std::endl;
	}

	void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const override;
};

struct ReturnNode : public ExprNode {
	ReturnNode(TokenType token_type, const Type* type, ExprNodePtr&& expr) : ExprNode(token_type, type), expr_(move(expr)) {}

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << "(" << type_->name() << ")" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << "return" << std::endl;
		expr_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const override;
	ExprNodePtr expr_;
};

struct StmtBlockNode : public StmtNode {
	StmtBlockNode() :StmtNode(NT_STMT_BLOCK) {};

	void AddStmt(StmtNodePtr&& stmt) { if (stmt) stmts_.emplace_back(std::move(stmt)); }

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << kTokenTypeStr[token_type_] << " " << stmts_.size() << std::endl;
		for (const StmtNodePtr& ptr : stmts_)
			ptr->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}
	
	virtual void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value = true) const override;

private:
	std::vector<StmtNodePtr> stmts_;
};

struct IfNode : public StmtNode {
	IfNode(ExprNodePtr&& condition, StmtNodePtr&& then, StmtNodePtr&& els) : 
		StmtNode(NT_IF), condition_(std::move(condition)), then_(std::move(then)), else_(std::move(els)) {};

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << "condition" << std::endl;
		condition_->Print(oa, padding + kIndent);
		oa << padding << "then" << std::endl;
		then_->Print(oa, padding + kIndent);
		oa << padding << "else" << std::endl;
		else_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const override;
	ExprNodePtr condition_;
	StmtNodePtr then_;
	StmtNodePtr else_;
};

struct ForNode : public StmtNode {
	ForNode(StmtNodePtr&& init, ExprNodePtr&& condition, ExprNodePtr&& iter, StmtNodePtr&& body) :
		StmtNode(NT_FOR), init_(move(init)), condition_(move(condition)), iter_(move(iter)), body_(move(body)) {};

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		oa << padding + "{" << kTokenTypeStr[token_type_] << std::endl;
		oa << padding << "init" << std::endl;
		init_->Print(oa, padding + kIndent);
		oa << padding << "condition" << std::endl;
		condition_->Print(oa, padding + kIndent);
		oa << padding << "iter" << std::endl;
		iter_->Print(oa, padding + kIndent);
		oa << padding << "body" << std::endl;
		body_->Print(oa, padding + kIndent);
		oa << padding + "}" << std::endl;
	}

	void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const override;
	StmtNodePtr init_;
	ExprNodePtr condition_;
	ExprNodePtr iter_;
	StmtNodePtr body_;
};

struct WhileNode : public StmtNode {
	WhileNode() : StmtNode(NT_WHILE) {};

	void Gen(FunctionSymbol* function, LocalScope* local_scope, bool right_value) const override;
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
		global_scope_->Put(std::unique_ptr<Double>(new Double(global_scope_.get())));
		current_scope_ = global_scope_.get();
	}

	void Parse(const std::string& program);

	void Gen();

	virtual void Print(std::ostream& oa, const std::string& padding) const override {
		for (const FunctionSymbol* func : all_functions_) {
			func->Print(oa, padding);
			oa << std::endl;
			/*const Function* func_type = dynamic_cast<const Function*>(func->type_);
			oa << padding + "{Function: " << func->name() << std::endl;
			oa << padding << "return: " << func_type->ret_type_->name()<< std::endl;
			oa << padding << "params: ";
			for (int i = 0; i < func->param_names_.size(); ++i) {
				oa << (i > 0 ? "," : "") << "(" << func_type->param_types_[i]->name() << ")" << func->param_names_[i];
			}
			oa << std::endl << padding << "body:" << std::endl;
			func->body_->Print(oa, padding + kIndent);
			oa << padding + "}" << std::endl;*/
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
	ExprNodePtr ParseArray(VariableNodePtr &&inherit);
	
	const Type* GetType(const std::string& type_name) const {
		return dynamic_cast<const Type*>(current_scope_->Get(type_name));
	}

private:
	Lexer lexer_;
	Scope* current_scope_;
	ScopePtr global_scope_;
	std::vector<FunctionSymbol*> all_functions_;
};


} // namespace pswgoo
