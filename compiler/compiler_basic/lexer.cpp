#include "lexer.h"

#include <cctype>
#include <iostream>

using namespace std;

namespace pswgoo {
const Lexer::Token Lexer::kNonToken_ = Lexer::Token();

const unordered_map<string, TokenType> Lexer::kOperators = { 
	{ "+", OP_ADD }, { "-", OP_MINUS }, { "*", OP_PRODUCT }, 
	{ "/", OP_DIVIDE }, { "(", OP_LEFT_PARENTHESIS }, { ")", OP_RIGHT_PARENTHESIS},
	{ ">", OP_GREATER }, { "<", OP_LESS }, { "==", OP_EQUAL},
	{ ">=", OP_GREATER_EQUAL }, { "<=", OP_LESS_EQUAL }, {"=", OP_ASSIGN}, // priority of "==" is higher than "="
};

const unordered_map<string, TokenType> Lexer::kKeyWords = { 
	{ "if", KEY_IF }, { "else", KEY_ELSE }, { "int", KEY_INT } 
};

int Lexer::Tokenize(const string &sequence) {
	enum NodeType {kStart, kNumber, kIdentifier, kOperator};
	int cur = 0;
	NodeType state = kStart;
	string tmp_token;
	while (cur <= sequence.size()) {
		char cur_char = cur < sequence.size() ? sequence[cur] : ' ';
		switch (state) {
			case kStart:
				if (isspace(cur_char))
					break;
				else if (isdigit(cur_char))
					state = kNumber;
				else if (isalpha(cur_char))
					state = kIdentifier;
				else
					state = kOperator;
				tmp_token += sequence[cur];
				break;
			case kNumber:
				if (isspace(cur_char)) {
					state = kStart;
					tokens_.emplace_back(TokenType::INT, tmp_token);
					tmp_token.clear();
				}
				else if (isdigit(cur_char))
					tmp_token += cur_char;
				else if (isalpha(cur_char))
					cerr << "ERROR: alpha bet cannot next to digits" << endl;
				else {
					state = kOperator;
					tokens_.emplace_back(TokenType::INT, tmp_token);
					tmp_token = string(1, cur_char);
				}
				break;
			case kIdentifier:
				if (isspace(cur_char)) {
					state = kStart;
					if (kKeyWords.find(tmp_token) != kKeyWords.end())
						tokens_.emplace_back(kKeyWords.at(tmp_token), tmp_token);
					else
						tokens_.emplace_back(TokenType::NAME, tmp_token);
					tmp_token.clear();
				}
				else if (isdigit(cur_char) || isalpha(cur_char))
					tmp_token += cur_char;
				else {
					state = kOperator;
					if (kKeyWords.find(tmp_token) != kKeyWords.end())
						tokens_.emplace_back(kKeyWords.at(tmp_token), tmp_token);
					else
						tokens_.emplace_back(TokenType::NAME, tmp_token);
					tmp_token = string(1, cur_char);
				}
				break;
			case kOperator: {
				string op;
				if (isspace(cur_char)) {
					state = kStart;
					op = tmp_token;
					tmp_token.clear();
				}
				else if (isdigit(cur_char)) {
					state = kNumber;
					op = tmp_token;
					tmp_token = string(1, cur_char);
				}
				else if (isalpha(cur_char)) {
					state = kIdentifier;
					op = tmp_token;
					tmp_token = string(1, cur_char);
				}
				else {
					if (cur_char == '=' && (tmp_token == "<" || tmp_token == ">" || tmp_token == "="))
						tmp_token += cur_char;
					else {
						state = kOperator;
						op = tmp_token;
						tmp_token = string(1, cur_char);
					}
				}
				if (kOperators.find(op) != kOperators.end())
					tokens_.emplace_back(kOperators.at(op), op);
				else if (!op.empty())
					cerr << "ERROR: invalid operator: " << op << endl;
				break;
			}
			default:
				break;
		}
		++cur;
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