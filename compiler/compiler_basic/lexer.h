#pragma once

#include <vector>
#include <string>

namespace pswgoo {

class Lexer {
public:
	static const std::vector<std::string> kKeyWords;
	static const std::string kOperators;
	enum TokenType { KEY_IF, KEY_ELSE, KEY_INT, NAME, INT, OP_MINUS, OP_ADD, OP_PRODUCT, OP_DIVIDE, OP_LEFT_PARENTHESIS, OP_RIGHT_PARENTHESIS};
	struct Token {
		Token() = default;
		Token(TokenType type, const std::string& value) : type_(type), value_(value) {};
		TokenType type_;
		std::string value_;
	};

	int Tokenize(const std::string &sequence);
	const Token& Current() const;
	const Token& Next() const;
	const Token& ToNext();

private:
	static const Token kNonToken_;
	int cursor_;
	std::vector<Token> tokens_;
};

} // namespace pswgoo
