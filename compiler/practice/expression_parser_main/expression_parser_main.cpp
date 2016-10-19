#include <iostream>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

/**
Grammer:

E -> (E)S | DE' | -T
S -> E' | epsilon
T -> D | (E)
E' -> +E | -E | *E | /E
D -> number
**/

class ExpressionTree {
public:
	void Parse(const std::string& expression) {

	}

private:
	void Parse(Lexer& lexer) {

	}

private:
	Lexer::Token value_;
	vector<ExpressionTree> children_;
};

int main(int argc, char** argv) {

	int a = 3- -2 / 2;
	cout << a << endl;
	system("pause");
	return 0;
}