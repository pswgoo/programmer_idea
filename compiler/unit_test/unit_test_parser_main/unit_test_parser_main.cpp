#include <iostream>

#include "compiler/compiler_basic/lexer.h"
#include "compiler/compiler_basic/parser.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	//if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	string test_str = R"DELIM(

int add(int a, int b) {
	int ret;
	ret = a + b;
}

int main(int argc, char argv) {
	int a;
	a = 2;
	b = a + 3;
	c = 5;
	char d;
	d = 'c';
	c = add(a, c);
	
	int e[10][50][30];
	b = e[2][1][3] + a;
}

	)DELIM";

	

	try {
		Compiler parser;
		parser.Parse(test_str);
		parser.Print(cout,"");
	} catch (exception e) {
		cerr << e.what() << endl;
	}

	clog << "Complete" << endl;
	system("pause");
	return 0;
}
