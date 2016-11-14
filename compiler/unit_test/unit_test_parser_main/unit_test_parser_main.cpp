#include <iostream>
#include <fstream>

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

int main() {
	int a;
	a = 2;
	int b;
	b = a + 3;
	int c;
	c = 5;
	char d;
	d = (char)(c+a);
	c = add(main(), c);
	
	int e[10][50][30];
	e[2][1][3] = 342;
	b = e[2][1][3] + d;
}

	)DELIM";

	try {
		Compiler parser;
		parser.Parse(test_str);
		ofstream fout("ast.txt");
		parser.Print(fout,"");
	} catch (exception e) {
		cerr << e.what() << endl;
	}

	clog << "Complete" << endl;
	system("pause");
	return 0;
}
