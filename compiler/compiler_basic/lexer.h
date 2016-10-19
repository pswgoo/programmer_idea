#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace pswgoo {

	enum TokenType				 { NON_TERMINAL, KEY_IF, KEY_ELSE, KEY_INT, NAME, INT, OP_MINUS, OP_ADD, OP_PRODUCT, OP_DIVIDE, OP_LEFT_PARENTHESIS, OP_RIGHT_PARENTHESIS, OP_GREATER, OP_LESS, OP_EQUAL, OP_GREATER_EQUAL, OP_LESS_EQUAL, OP_ASSIGN };
	const std::vector<std::string> kTokenTypeStr = { "NON_TERMINAL", "KEY_IF", "KEY_ELSE", "KEY_INT", "NAME", "INT", "OP_MINUS", "OP_ADD", "OP_PRODUCT", "OP_DIVIDE", "OP_LEFT_PARENTHESIS", "OP_RIGHT_PARENTHESIS", "OP_GREATER", "OP_LESS", "OP_EQUAL", "OP_GREATER_EQUAL", "OP_LESS_EQUAL", "OP_ASSIGN" };

class Lexer {
public:
	static const std::unordered_map<std::string, TokenType> kKeyWords;
	static const std::unordered_map<std::string, TokenType> kOperators;
	
	struct Token {
		Token() = default;
		Token(TokenType type, const std::string& value) : type_(type), value_(value) {};
		TokenType type_;
		std::string value_;
		std::string ToString() const {
			return "{" + kTokenTypeStr[type_] + "," + value_ + "}";
		}
		bool Non() const {
			return this == &kNonToken_;
		}
	};

	int Tokenize(const std::string &sequence);
	const Token& Current() const;
	const Token& Next() const;
	const Token& ToNext();

private:
	static const Token kNonToken_;
	int cursor_ = 0;
	std::vector<Token> tokens_;
};

} // namespace pswgoo
