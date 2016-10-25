#include <iostream>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	//if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	string test_str = R"DELIM(
	
	double a = 1e-10;

	Lexer lexer; 
	int ret = lexer.Tokenize(test_str);

	clog << "Total split " << ret << " tokens" << endl;
	while (!lexer.Current() . Non()) {
		cout << lexer.Current().ToString() << endl;
		lexer.ToNext();
	}
	clog << "Complete" << endl;
	system("pause");
	return 0;
}

if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
.343 =.43
int a= 3+++,--i;

	)DELIM";
	
	int a = 0;
	double d = a+=4 - 3;

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
