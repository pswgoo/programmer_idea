#include "lexer.h"

#include <cctype>
#include <iostream>

using namespace std;

namespace pswgoo {
const Lexer::Token Lexer::kNonToken_ = Lexer::Token();
const std::string kOperators = "+-*/()";

const std::vector<std::string> Lexer::kKeyWords = {"if", "else", "int"};

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
			else if (kOperators.find(cur_char) != string::npos)
				state = kOperator;
			else {
				cerr << "ERROR: invalid char occured: " << cur_char << endl;
				break;
			}
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
			else if (kOperators.find(cur_char) != string::npos)
				cerr << "ERROR: operators cannot next to digits" << endl;
			else
				cerr << "ERROR: invalid char occured: " << cur_char << endl;
			break;
		case kIdentifier:
			if (isspace(cur_char)) {
				state = kStart;
				tokens_.emplace_back(TokenType::NAME, tmp_token);
				tmp_token.clear();
			}
			else if (isdigit(cur_char) || isalpha(cur_char))
				tmp_token += cur_char;
			else if (kOperators.find(cur_char) != string::npos) {
				state = kOperator;
				tokens_.emplace_back(TokenType::NAME, tmp_token);
				tmp_token = string(1, cur_char);
			}
			else
				cerr << "ERROR: invalid char occured: " << cur_char << endl;
			break;
		case kOperator:
			if (isspace(cur_char)) {
				state = kStart;
				tokens_.emplace_back(TokenType(kOperators.find(tmp_token.front())), tmp_token);
				tmp_token.clear();
			}
			else if (isdigit(cur_char)) {
				state = kNumber;
				tokens_.emplace_back(TokenType(kOperators.find(tmp_token.front())), tmp_token);
				tmp_token = string(1, cur_char);
			}
			else if (isalpha(cur_char)) {
				state = kNumber;
				tokens_.emplace_back(TokenType(kOperators.find(tmp_token.front())), tmp_token);
				tmp_token = string(1, cur_char);
			}
			else if (kOperators.find(cur_char) != string::npos) {
				state = kOperator;
				tokens_.emplace_back(TokenType(kOperators.find(tmp_token.front())), tmp_token);
				tmp_token = string(1, cur_char);
			}
			else
				cerr << "ERROR: invalid char occured: " << cur_char << endl;
			break;
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

const Lexer::Token& Lexer::Next() const {
	if (cursor_ + 1 >= tokens_.size())
		return kNonToken_;
	else
		return tokens_[cursor_ + 1];
}

const Lexer::Token& Lexer::ToNext() {
	if (cursor_ >= tokens_.size())
		return kNonToken_;
	else 
		return tokens_[++cursor_];
}



} // namespace pswgoo