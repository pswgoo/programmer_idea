#include "lexer.h"

#include <cctype>
#include <iostream>

using namespace std;

namespace pswgoo {

const Lexer::Token Lexer::kNonToken_ = Lexer::Token();

// "true" and "false" are viewed as key words(with token type BOOLEAN)
const unordered_map<string, TokenType> Lexer::kKeyWords = {
	{ "if", KEY_IF }, { "else", KEY_ELSE }, { "while", KEY_WHILE }, { "for", KEY_FOR }, { "switch", KEY_SWITCH },
	{ "case", KEY_CASE }, { "struct", KEY_STRUCT }, { "class", KEY_CLASS }, { "const", KEY_CONST }, { "do", KEY_DO },
	{ "goto", KEY_GOTO }, { "bool", KEY_BOOL }, { "char", KEY_CHAR}, { "int", KEY_INT }, { "float", KEY_FLOAT }, 
	{ "double", KEY_DOUBLE }, { "null", KEY_NULL }, { "break", KEY_BREAK },
	{ "continue", KEY_CONTINUE }, { "default", KEY_DEFAULT }, { "return", KEY_RETURN }, { "void", KEY_VOID },
	{ "true", BOOLEAN }, { "false", BOOLEAN },
};

const unordered_map<string, TokenType> Lexer::kOperators = { 
	{ "+", OP_ADD }, { "-", OP_MINUS }, { "*", OP_PRODUCT }, { "/", OP_DIVIDE }, { "%", OP_MOD },
	{ "&", OP_BIT_AND }, { "|", OP_BIT_OR }, { "~", OP_BIT_NOT }, { "^", OP_BIT_XOR },
	{ "&&", OP_LOGICAL_AND }, { "||", OP_LOGICAL_OR }, { "!", OP_LOGICAL_NOT },
	{ "{", OP_LEFT_BRACE }, { "}", OP_RIGHT_BRACE }, { "[", OP_LEFT_BRACKET }, { "]", OP_RIGHT_BRACKET }, { "(", OP_LEFT_PARENTHESIS }, { ")", OP_RIGHT_PARENTHESIS },
	{ ",", OP_COMMON }, { ".", OP_DOT }, { ";", OP_SEMICOLON }, { "?", OP_QUESTION }, { ":", OP_COLON }, { "::", OP_DOUBLE_COLON },
	{ ">", OP_GREATER }, { "<", OP_LESS }, { "==", OP_EQUAL }, { "!=", OP_NOT_EQUAL }, { ">=", OP_GREATER_EQUAL }, { "<=", OP_LESS_EQUAL },
	{ "=", OP_ASSIGN }, { "+=", OP_ADD_ASSIGN }, { "-=", OP_MINUS_ASSIGN }, { "*=", OP_PRODUCT_ASSIGN }, { "/=", OP_DIVIDE_ASSIGN }, { "%=", OP_MOD_ASSIGN },
	{ "++", OP_INCREMENT }, { "--", OP_DECREMENT },
};

char EscapeCharacter(const string& ch) {
	unordered_map<string, int> table = {
		{ "\\a", 7 },
		{ "\\b", 8 },
		{ "\\f", 12 },
		{ "\\n", 10 },
		{ "\\r", 13 },
		{ "\\t", 9 },
		{ "\\v", 11 },
		{ "\\\\", 92 },
		{ "\\?", 63 },
		{ "\\'", 39 },
		{ "\\\"", 34 },
		{"\\0", 0 },
	};
	if (table.count(ch) == 0 && ch.size() != 1)
		return (char)-1;
	else if (table.count(ch) > 0)
		return (char)table[ch];
	return ch.front();
}

string EscapeString(const string& str, bool& valid) {
	string ret_str;
	valid = false;
	for (int i = 0; i < str.size(); ++i){
		if (str[i] != '\\')
			ret_str.push_back(str[i]);
		else if (i + 1 < str.size()){
			char ch = EscapeCharacter(str.substr(i, 2));
			if (ch < 0)
				return ret_str;
			ret_str.push_back(ch);
		}
		else
			return ret_str;
	}
	valid = true;
	return ret_str;
}

int Lexer::Tokenize(const string &sequence) {
	enum NodeType { kStart, kInteger, kReal, kCh, kString, kIdentifier, kOperator };
	int cur = 0;
	NodeType state = kStart;
	string buffer;
	try {
		while (cur <= sequence.size()) {
			char cur_char = cur < sequence.size() ? sequence[cur] : ' ';
			switch (state) {
			case kStart:
				if (isspace(cur_char))
					break;
				else if (isdigit(cur_char))
					state = kInteger;
				else if (cur_char == '\'') {
					state = kCh;
					break;
				}
				else if (cur_char == '.') {
					if (tokens_.empty() || (tokens_.back().type_ != OP_RIGHT_BRACKET && tokens_.back().type_ != OP_RIGHT_PARENTHESIS))
						state = kReal;
					else
						state = kOperator;
				}
				else if (cur_char == '"') {
					state = kString;
					break;
				}
				else if (isalpha(cur_char))
					state = kIdentifier;
				else
					state = kOperator;
				buffer += sequence[cur];
				break;
			case kInteger:
				if (isspace(cur_char)) {
					state = kStart;
					tokens_.emplace_back(TokenType::INTEGER, buffer);
					buffer.clear();
				}
				else if (isdigit(cur_char))
					buffer += cur_char;
				else if (cur_char == '.' || cur_char == 'e' || cur_char == 'E') {
					buffer += cur_char;
					state = kReal;
				}
				else if (isalpha(cur_char))
					throw runtime_error("alpha bet cannot next to digits: " + buffer + cur_char);
				else {
					state = kOperator;
					tokens_.emplace_back(TokenType::INTEGER, buffer);
					buffer = string(1, cur_char);
				}
				break;
			case kReal:
				if (isspace(cur_char)) {
					state = kStart;
					tokens_.emplace_back(TokenType::REAL, buffer);
					buffer.clear();
				}
				else if (isdigit(cur_char))
					buffer += cur_char;
				else if ((cur_char == '-' || cur_char == '+') && (buffer.size() == 0 || buffer.back() == 'e' || buffer.back() == 'E'))
					buffer += cur_char;
				else if (isalpha(cur_char))
					throw runtime_error("alpha bet cannot next to real number: " + buffer + cur_char);
				else {
					state = kOperator;
					tokens_.emplace_back(TokenType::REAL, buffer);
					buffer = string(1, cur_char);
				}
				break;
			case kCh:
				if (cur_char == '\'') {
					char ch = EscapeCharacter(buffer);
					if (ch < 0)
						throw runtime_error("invalid character: " + buffer);
					tokens_.emplace_back(TokenType::CHAR, string(1, ch));
					buffer.clear();
					state = kStart;
				}
				else
					buffer += cur_char;
				break;
			case kString:
				if (cur_char == '"' && (buffer.size() == 0 || buffer.back() != '\\')) {
					bool valid;
					string str = EscapeString(buffer, valid);
					if (!valid)
						throw runtime_error("invalid string: " + buffer);
					tokens_.emplace_back(TokenType::STRING, str);
					buffer.clear();
					state = kStart;
				}
				else
					buffer += cur_char;
				break;
			case kIdentifier:
				if (isspace(cur_char)) {
					state = kStart;
					if (kKeyWords.find(buffer) != kKeyWords.end())
						tokens_.emplace_back(kKeyWords.at(buffer), buffer);
					else
						tokens_.emplace_back(TokenType::IDENTIFIER, buffer);
					buffer.clear();
				}
				else if (isdigit(cur_char) || isalpha(cur_char) || cur_char == '_')
					buffer += cur_char;
				else {
					state = kOperator;
					if (kKeyWords.find(buffer) != kKeyWords.end())
						tokens_.emplace_back(kKeyWords.at(buffer), buffer);
					else
						tokens_.emplace_back(TokenType::IDENTIFIER, buffer);
					buffer = string(1, cur_char);
				}
				break;
			case kOperator: {
				string op;
				if (isspace(cur_char)) {
					state = kStart;
					op = buffer;
					buffer.clear();
				}
				else if (isdigit(cur_char)) {
					state = kInteger;
					op = buffer;
					buffer = string(1, cur_char);
				}
				else if (cur_char == '.' && buffer[0] != ')' && buffer[0] != ']') { //. after ) or ] be viewed as the OP_DOT
					state = kReal;
					op = buffer;
					buffer = string(1, cur_char);
				}
				else if (isalpha(cur_char)) {
					state = kIdentifier;
					op = buffer;
					buffer = string(1, cur_char);
				}
				else if (cur_char == '\''){
					state = kCh;
					op = buffer;
					buffer.clear();
				}
				else if (cur_char == '"'){
					state = kString;
					op = buffer;
					buffer.clear();
				}
				else {
					const static string kBeforeEqual("+-*/%<>!=");
					if (buffer.size() == 1 && cur_char == '&' && buffer[0] == '&')
						buffer += cur_char;
					else if (buffer.size() == 1 && cur_char == '|' && buffer[0] == '|')
						buffer += cur_char;
					else if (buffer.size() == 1 && cur_char == '+' && buffer[0] == '+')
						buffer += cur_char;
					else if (buffer.size() == 1 && cur_char == '-' && buffer[0] == '-')
						buffer += cur_char;
					else if (buffer.size() == 1 && cur_char == '=' && (kBeforeEqual.find(buffer[0]) != string::npos))
						buffer += cur_char;
					else {
						op = buffer;
						buffer = string(1, cur_char);
					}
				}
				if (!op.empty() && kOperators.find(op) != kOperators.end())
					tokens_.emplace_back(kOperators.at(op), op);
				else if (!op.empty()) 
					throw runtime_error("invalid operator: " + op);
				break;
			}
			default:
				break;
			}
			++cur;
		}
	}
	catch (const exception& e) {
		cerr << "Lexer::Tokenize Exception: " << e.what() << endl;
	}

	return (int)tokens_.size();
}

const Lexer::Token & Lexer::Current() const {
	if (cursor_ >= tokens_.size())
		return kNonToken_;
	else
		return tokens_[cursor_];
}

const Lexer::Token& Lexer::LookNext() const {
	if (cursor_ + 1 >= tokens_.size())
		return kNonToken_;
	else
		return tokens_[cursor_ + 1];
}

const Lexer::Token& Lexer::ToNext() {
	if (cursor_ >= tokens_.size())
		return kNonToken_;
	else 
		return tokens_[cursor_++];
}

} // namespace pswgoo
