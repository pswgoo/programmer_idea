#include <iostream>

#include "compiler/compiler_basic/lexer.h"
#include "compiler/compiler_basic/cl_parser.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	//if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	string test_str = R"DELIM(
	
	a = 1e-10


	)DELIM";

	int a = 0, c = 1;
	double d = a += c += a;

	Lexer lexer;
	int ret = lexer.Tokenize(test_str);

	clog << "Total split " << ret << " tokens" << endl;
	while (!lexer.Current().Non()) {
		cout << lexer.Current().ToString() << endl;
		lexer.ToNext();
	}

	lexer.set_cursor(0);
	try {
		SymbolTable symbol_table;
		ClExprNode expr_node(lexer, symbol_table);
		expr_node.Print(cout);
	} catch (const exception& e) {
		cerr << e.what() << endl;
	}
	clog << "Complete" << endl;
	system("pause");
	return 0;
}
