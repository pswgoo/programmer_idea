#include <iostream>
#include <fstream>

#include "compiler/compiler_basic/lexer.h"
#include "compiler/compiler_basic/parser.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	string test_str = R"DELIM(

int add(int a, int b) {
	return a + b;
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
	
	int f;
	f = 0;
	for (a = 3; a < c; a += 1)
		f += a;

	int e[10][50][30];
	e = new int[10][50][30];
	e[2][1][3] = 342;
	b = e[2][1][add(d, c)] + d;
}

	)DELIM";

	try {
		Compiler parser;
		parser.Parse(test_str);
		parser.Gen();
		ofstream fout("ast.txt");
		parser.Print(fout,"");
		//ofstream fout2("ast2.txt");

	} catch (exception e) {
		cerr << e.what() << endl;
	}

	clog << "Complete" << endl;

	return 0;
}
