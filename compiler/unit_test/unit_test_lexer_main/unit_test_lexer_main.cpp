#include <iostream>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	string test_str = R"DELIM(
		if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	)DELIM";

	Lexer lexer;
	int ret = lexer.Tokenize(test_str);

	clog << "Total split " << ret << " tokens" << endl;
	while (!lexer.Current().Non()) {
		cout << lexer.Current().ToString() << endl;
		lexer.ToNext();
	}
	clog << "Complete" << endl;
	system("pause");
	return 0;
}