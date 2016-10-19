#include <iostream>
#include <fstream>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

/**
Grammer:

E -> (E)E' | DE' | -TE'
T -> D | (E)
E' -> +E | -E | *E | /E | epsilon
D -> number
**/

/*
expr ParseE() {
	switch next() {
	case '(':
		consume('(');
		expr e = ParseE();
		consume(')');
		return parseEs(e);
	case '-':

	}
}

expr ParseEs(expr l) {
	switch next() {
	case '+':
		consume('+');
		expr r = ParseE();
		return AddNode(l, r);
	default:
		return l;
	}
}
*/

class ExpressionTree {
public:
	ExpressionTree() :type_(TokenType::NON_TERMINAL, "E") {};
	ExpressionTree(const Lexer::Token& token) : type_(token) {}
	bool Parse(const std::string& expression) {
		Lexer lexer(expression);
		bool valid = ParseE(lexer);
		if (!lexer.Current().Non())
			valid = false;
		return valid;
	}
	
	void Print(std::ostream& os) {
		os << children_.size() << endl;
		for (const string& str : ToString())
			os << str << endl;
	}

	vector<string> ToString() const {
		if (type_.type_ != NON_TERMINAL)
			return{ type_.value_ };
		vector<string> ret_strings = { type_.value_ + " : {" };
		for (int i = 0; i < children_.size(); ++i) {
			for (const string& str : children_[i].ToString())
				ret_strings.emplace_back('\t' + str);
		}
		ret_strings.emplace_back("}");
		return ret_strings;
	}

	const Lexer::Token& value() const { return type_; }
	const vector<ExpressionTree>& children() const { return children_; }

	int Evaluate() const {
		return Evaluate(type_.value_);
	}

private:
	int Evaluate(const string &type, int prev = 0) const  {
		if (type_.type_ == INT)
			return stoi(type_.value_);
		if (type != type_.value_) {
			cerr << "TYPE not match " << type_.value_ << " " << type << endl;
			return 0;
		}
			
		if (type_.value_ == "E") {
			if (children_.size() == 0)
				return prev;
			if (children_[0].type_.type_ == OP_LEFT_PARENTHESIS)
				return children_[3].Evaluate("E'", children_[1].Evaluate("E"));
			else if (children_[0].type_.type_ == INT)
				return children_[1].Evaluate("E'", children_[0].Evaluate("INT"));
			else if (children_[0].type_.type_ == OP_MINUS) 
				return children_[2].Evaluate("E'", -children_[1].Evaluate("T"));
			else
				cerr << "Evaluate error in " << type_.value_;
		}
		else if (type_.value_ == "E'") {
			if (children_.size() == 0)
				return prev;
			switch (children_[0].type_.type_)
			{
			case OP_ADD:
				prev += children_[1].Evaluate("E");
				break;
			case OP_MINUS:
				prev -= children_[1].Evaluate("E");
				break;
			case OP_PRODUCT:
				prev *= children_[1].Evaluate("E");
				break;
			case OP_DIVIDE:
				if (children_[1].Evaluate("E") == 0)
					prev = numeric_limits<int>::max();
				else
					prev /= children_[1].Evaluate("E");
				break;
			default:
				break;
			}
			return prev;
		}
		else if (type_.value_ == "T") {
			if (children_[0].type_.type_ == INT)
				return children_[0].Evaluate("INT");
			else if (children_[0].type_.type_ == OP_LEFT_PARENTHESIS)
				return children_[1].Evaluate("E");
			else
				cerr << "Evaluate error in " << type_.value_;
		}
		return 0;
	}

	bool ParseE(Lexer& lexer) {
		#define set_error(message) {valid = false; cerr << (message) << endl; break;}
		bool valid = true;

		switch (lexer.Current().type_) {
			case TokenType::OP_LEFT_PARENTHESIS: {
				children_.emplace_back(lexer.Current());
				lexer.ToNext();
				ExpressionTree expression(Lexer::Token(TokenType::NON_TERMINAL, "E"));
				if (expression.ParseE(lexer))
					children_.emplace_back(move(expression));
				else
					set_error("ERROR: ParseE error, expression invalid after '('");
				if (lexer.Current().type_ == TokenType::OP_RIGHT_PARENTHESIS)
					children_.emplace_back(ExpressionTree(lexer.Current()));
				else
					set_error("ERROR: ParseE error, '(' has no matched ')', " + lexer.Current().type_);
				lexer.ToNext();
				ExpressionTree s(Lexer::Token(TokenType::NON_TERMINAL, "E'"));
				if (s.ParseEs(lexer)) {
					children_.emplace_back(move(s));
				}
				else
					set_error("ERROR: ParseE error, in parse E'");
				
				break;
			}
			case TokenType::INT: {
				children_.emplace_back(lexer.Current());
				lexer.ToNext();
				ExpressionTree expression(Lexer::Token(TokenType::NON_TERMINAL, "E'"));
				if (expression.ParseEs(lexer))
					children_.emplace_back(move(expression));
				else
					set_error("ERROR: ParseE error, in ParseEs");
				break;
			}
			case TokenType::OP_MINUS: {
				children_.emplace_back(lexer.Current());
				lexer.ToNext();
				ExpressionTree expression(Lexer::Token(TokenType::NON_TERMINAL, "T"));
				if (expression.ParseT(lexer))
					children_.emplace_back(move(expression));
				else
					set_error("ERROR: ParseE error, in ParseT");
				ExpressionTree es(Lexer::Token(TokenType::NON_TERMINAL, "E'"));
				if (es.ParseEs(lexer))
					children_.emplace_back(move(es));
				else
					set_error("ERROR: ParseE error, in ParseEs");
				break;
			}
			default:
				clog << "ERROR: ParseE error, invalid char" << endl;
				valid = false;
				break;
		}
		#undef set_error
		return valid;
	}

	bool ParseT(Lexer& lexer) {
		bool valid = true;
		if (lexer.Current().type_ == INT) {
			children_.emplace_back(lexer.Current());
			lexer.ToNext();
		}
		else if (lexer.Current().type_ == OP_LEFT_PARENTHESIS) {
			children_.emplace_back(lexer.Current());
			lexer.ToNext();
			ExpressionTree expression(Lexer::Token(TokenType::NON_TERMINAL, "E"));
			valid = expression.ParseE(lexer);
			if (valid)
				children_.emplace_back(move(expression));
			else
				cerr << "ERROR: ParseT error, in parse E" << endl;
			if (lexer.Current().type_ == OP_RIGHT_PARENTHESIS) {
				children_.emplace_back(lexer.Current());
			}
			else {
				valid = false;
				cerr << "ERROR: ParseT error, '(' has no matched ')'" << endl;
			}
			lexer.ToNext();
		}
		return valid;
	}

	bool ParseEs(Lexer& lexer) {
		bool valid = true;
		if (lexer.Current().type_ == OP_ADD || lexer.Current().type_ == OP_MINUS || lexer.Current().type_ == OP_PRODUCT || lexer.Current().type_ == OP_DIVIDE) {
			children_.emplace_back(lexer.Current());
			lexer.ToNext();
			ExpressionTree expression(Lexer::Token(TokenType::NON_TERMINAL, "E"));
			valid = expression.ParseE(lexer);
			if (valid)
				children_.emplace_back(move(expression));
			else
				cerr << "ERROR: ParseEs error, in parse E" << endl;
		}
		return valid;
	}

private:
	Lexer::Token type_;
	vector<ExpressionTree> children_;
};

int main(int argc, char** argv) {

	string expression = "-(2 + 34) /-(2 -54)*(-3/2) + ((3+-1)/(4-2)) ";
	cout << expression << endl;

	ExpressionTree tree;
	cout << "Parse: " << tree.Parse(expression) << endl;

	int b = - - -1;
	cout << b << endl;

	int A = -(2 + 34) / -(2 - 54)*(-3 / 2) + ((3 + -1) / (4 - 2));
	cout << "A = " << A << endl;
	cout << "B = " << -(2 - 54)*(-3 / 2) + ((3 + -1) / (4 - 2)) << endl;

	cout << "Evaluate: " << tree.Evaluate() << endl;

	ofstream fout("test.txt");
	tree.Print(fout);

	system("pause");
	return 0;
}
