#include <iostream>

#include "compiler/compiler_basic/lexer.h"

using namespace std;
using namespace pswgoo;

int main(int argc, char** argv) {
	//if a<=b>cd==<(=== = = else cc3 =a1 + bb2/2432*3 + 2 - (-c+d)*((bc2+c)/(342- 4))+psw
	string test_str0 = R"DELIM(
	
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
	
	string test_str1 = R"DELIM(

int add(int a, int b) {
	int ret = a + b;
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


	int a = 0,c=1;
	double d = a+=c+=a;

	Lexer lexer; 
	int ret = lexer.Tokenize(test_str1);

	clog << "Total split " << ret << " tokens" << endl;
	while (!lexer.Current().Non()) {
		cout << lexer.Current().ToString() << endl;
		lexer.ToNext();
	}
	clog << "Complete" << endl;
	system("pause");
	return 0;
}
