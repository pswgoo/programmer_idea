#include <iostream>
#include <fstream>
#include <memory>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

/**
Origin Grammer:
E -> E+F | E-F | F
F -> F/G | F*G | G
G -> (E) | D | -G

Eliminate left recursion:
E -> FE'
E' -> +FE' | -FE' | epsilon
F -> GF'
F' -> /GF' | *GF' | epsilon
G -> (E) | D | -G
**/

class AstNode {
public:
	virtual int Value() const = 0;
	
	void Print(std::ostream& os) {
		for (const string& str : ToStrings())
			os << str << endl;
	}

	vector<string> ToStrings() const {
		if (children_.empty())
			return{ value_ };
		vector<string> ret_strings = { value_ + "{" };
		for (int i = 0; i < children_.size(); ++i) {
			for (const string& str : children_[i]->ToStrings())
				ret_strings.emplace_back('\t' + str);
		}
		ret_strings.emplace_back("}");
		return ret_strings;
	}

protected:
	std::string value_;
	std::vector < std::unique_ptr<AstNode>> children_;
};

class NumberNode : public AstNode {
public:
	NumberNode(const std::string& value) { value_ = value; }
	virtual int Value() const {
		return stoi(value_);
	}
};

class AddNode : public AstNode {
public:
	AddNode(std::unique_ptr<AstNode>&& lhs, std::unique_ptr<AstNode>&& rhs) {
		children_.emplace_back(move(lhs));
		children_.emplace_back(move(rhs));
		value_ = '+';
	}

	virtual int Value() const {
		return children_[0]->Value() + children_[1]->Value();
	}
};

class MinusNode : public AstNode {
public:
	MinusNode(std::unique_ptr<AstNode>&& lhs, std::unique_ptr<AstNode>&& rhs) {
		children_.emplace_back(move(lhs));
		children_.emplace_back(move(rhs));
		value_ = '-';
	}

	virtual int Value() const {
		return children_[0]->Value() - children_[1]->Value();
	}
};

class ProductNode : public AstNode {
public:
	ProductNode(std::unique_ptr<AstNode>&& lhs, std::unique_ptr<AstNode>&& rhs) {
		children_.emplace_back(move(lhs));
		children_.emplace_back(move(rhs));
		value_ = '*';
	}

	virtual int Value() const {
		return children_[0]->Value() * children_[1]->Value();
	}
};

class DivideNode : public AstNode {
public:
	DivideNode(std::unique_ptr<AstNode>&& lhs, std::unique_ptr<AstNode>&& rhs) {
		children_.emplace_back(move(lhs));
		children_.emplace_back(move(rhs));
		value_ = '/';
	}

	virtual int Value() const {
		return children_[0]->Value() / children_[1]->Value();
	}
};

class NegateNode : public AstNode {
public:
	NegateNode(unique_ptr<AstNode>&& lhs) {
		children_.emplace_back(move(lhs));
		value_ = '-';
	}
	virtual int Value() const {
		return -children_[0]->Value();
	}
};

class IntExpression :public AstNode {
public:
	IntExpression() = default;
	IntExpression(const std::string& expression) { lexer_.Tokenize(expression); };
	bool Parse(const std::string& expression) {
		bool success = true;
		try {
			lexer_.Tokenize(expression);
			children_.resize(1);
			children_.front() = ParseE();
			if (!lexer_.Current().Non()) {
				success = false;
				throw("Expression invalid, has additional token!");
			}
		}
		catch (const exception &e) {
			clog << "Exception: " << e.what() << endl;
			success = false;
		}
		return success;
	}

	virtual int Value() const {
		if (children_.empty())
			return 0;
		return children_.front()->Value();
	}

private:
	std::unique_ptr<AstNode> ParseE() {
		unique_ptr<AstNode> left = ParseF();
		return ParseEs(move(left));
	}
	std::unique_ptr<AstNode> ParseEs(unique_ptr<AstNode>&& lhs) {
		if (lexer_.Current().type_ == TokenType::OP_ADD) {
			lexer_.ToNext();
			unique_ptr<AddNode> left(new AddNode(move(lhs), ParseF()));
			return ParseEs(move(left));
		}
		else if (lexer_.Current().type_ == TokenType::OP_MINUS) {
			lexer_.ToNext();
			unique_ptr<MinusNode> left(new MinusNode(move(lhs), ParseF()));
			return ParseEs(move(left));
		}
		return move(lhs);
	}
	std::unique_ptr<AstNode> ParseF() {
		unique_ptr<AstNode> left = ParseG();
		return ParseFs(move(left));
	}
	std::unique_ptr<AstNode> ParseFs(unique_ptr<AstNode>&& lhs) {
		if (lexer_.Current().type_ == TokenType::OP_PRODUCT) {
			lexer_.ToNext();
			unique_ptr<ProductNode> left(new ProductNode(move(lhs), ParseG()));
			return ParseFs(move(left));
		}
		else if (lexer_.Current().type_ == TokenType::OP_DIVIDE) {
			lexer_.ToNext();
			unique_ptr<DivideNode> left(new DivideNode(move(lhs), ParseG()));
			return ParseFs(move(left));
		}
		return move(lhs);
	}
	std::unique_ptr<AstNode> ParseG() {
		switch (lexer_.Current().type_)
		{
		case TokenType::INT:
			return unique_ptr<NumberNode>(new NumberNode(lexer_.ToNext().value_));
		case TokenType::OP_LEFT_PARENTHESIS:{
			lexer_.ToNext();
			unique_ptr<AstNode> left = ParseE();
			if (!lexer_.Consume(TokenType::OP_RIGHT_PARENTHESIS))
				throw("Has no matched ')' for '(' in ParseG");
			return left;
		}
		case TokenType::OP_MINUS:
			lexer_.ToNext();
			return unique_ptr<NegateNode>(new NegateNode(ParseG()));

		default:
			break;
		}
		throw("ParseG error, not matched rule for :" + lexer_.Current().value_);
		return nullptr;
	}

private:
	Lexer lexer_;
};

int main(int argc, char** argv) {

	string eA = "-(2 + 34) /-(2 -54)*(-3/2) + ((3+-1)/(4-2)) ";
	string eB = "-(2 - 54)*(-3 / 2) + ((3 + -1) / (4 - 2))";
	string eC = "6 / 3 * (2 + 3)";
	cout << eA << endl;

	IntExpression tree;
	cout << "Parse: " << tree.Parse(eA) << endl;

	ofstream fout("testA.txt");
	tree.Print(fout);

	int b = - - -1;
	cout << b << endl;

	int A = -(2 + 34) / -(2 - 54)*(-3 / 2) + ((3 + -1) / (4 - 2));
	cout << "A = " << A << endl;
	cout << "B = " << -(2 - 54)*(-3 / 2) + ((3 + -1) / (4 - 2)) << endl;
	cout << "C = " << 6 / 3 * (2 + 3) << endl;

	cout << "Evaluate A: " << tree.Value() << endl;
	tree.Parse(eB);
	cout << "Evaluate B: " << tree.Value() << endl;
	tree.Parse(eC);
	cout << "Evaluate C: " << tree.Value() << endl;

	system("pause");
	return 0;
}
