#include <iostream>

#include "compiler/compiler_basic/lexer.h"
#include "compiler/compiler_basic/cl_parser.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	//if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	string test_str = R"DELIM(
	
	int xxxx;
	xxxx = 12 * 4 % 3;	

	double a;
	int b;
	b = 12;
	a = 1e-10;
	double c;
	a = c = 12 * 4 % 3+ (b / 2 * 2) + a;
	double d;
	d += c + a;
	bool e;
	e = d > c;

	)DELIM";

	Lexer lexer;
	int ret = lexer.Tokenize(test_str);

	clog << "Total split " << ret << " tokens" << endl;
	while (!lexer.Current().Non()) {
		cout << lexer.Current().ToString() << endl;
		lexer.ToNext();
	}

	try {
		ClParser parser;
		parser.Parse(test_str);
		parser.Print(cout);
	} catch (const exception& e) {
		cerr << e.what() << endl;
	}
	clog << "Complete" << endl;
	system("pause");
	return 0;
}
