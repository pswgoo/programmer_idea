#include <iostream>
#include <fstream>

#include "compiler/compiler_basic/lexer.h"
#include "compiler/compiler_basic/parser.h"
#include "compiler/compiler_basic/virtual_machine.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	string test_str0 = R"DELIM(

int fab(int n) {
	if (n <= 2)
		return 1;
	return fab(n-1) + fab(n - 2);
}

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
	c = add(b, c);

	int f;
	f = 0;
	for (a = 3; a < 100; a = a+ 1) {
		f = f + a;
		int g;
		g = f -b;
		b = g + a;
		if (f >= 100)
			break;
	}

	int e[10][50][30];
	e = new int[10][50][30];
	e[2][1][3] = 342;
	b = e[2][1][add(d, c)] + d;

	return fab(30);
}

	)DELIM";

	string test_str1 = R"DELIM(

int sum(int s, int e) {
	int i;
	int sum;
	sum = 0;
	i = 0;
	for (i = s; i <= e; i = i +1)
		sum = sum + i;

	return sum;
}

int main() {
	return sum(1, 100000);
}

	)DELIM";

	string test_str2 = R"DELIM(

int main() {
	int arr[100];
	int i;
	i = 0;
	arr = new int[100];
	arr[0] = 0;
	for (i = 1; i < 100; i = i+ 1)
		arr[i] = i + arr[i-1];

	return arr[100-1];
}

	)DELIM";

	try {
		Compiler parser;
		parser.Parse(test_str2);
		parser.Gen();
		ofstream fout("ast.txt");
		parser.Print(fout,"");

		VirtualMachine vm;
		vm.Init(parser.global_scope().get());
		vm.Print(fout, "");
		clog << "Main: " << vm.Run() << endl;

		//ofstream fout2("ast2.txt");

	} catch (exception e) {
		cerr << e.what() << endl;
	}

	clog << "Complete" << endl;

	return 0;
}
