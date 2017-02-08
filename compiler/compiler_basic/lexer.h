#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <iostream>

namespace pswgoo {

enum TokenType {
	NOT_DEFINED,
	NT_TYPE_CAST, NT_CALL, NT_ARRAY,
	NT_IF, NT_FOR, NT_WHILE, NT_STMT_BLOCK,

	KEY_IF, KEY_ELSE, KEY_WHILE, KEY_FOR, KEY_SWITCH, 
	KEY_CASE, KEY_STRUCT, KEY_CLASS, KEY_CONST, KEY_DO, 
	KEY_GOTO, KEY_BREAK, KEY_CONTINUE, KEY_DEFAULT, 
	KEY_RETURN, KEY_VOID, KEY_NEW,

	OP_ADD, OP_MINUS, OP_PRODUCT, OP_DIVIDE, OP_MOD, 
	OP_BIT_AND, OP_BIT_OR, OP_BIT_NOT, OP_BIT_XOR, 
	OP_LOGICAL_AND, OP_LOGICAL_OR, OP_LOGICAL_NOT, 
	OP_LEFT_BRACE, OP_RIGHT_BRACE, OP_LEFT_BRACKET, OP_RIGHT_BRACKET, OP_LEFT_PARENTHESIS, OP_RIGHT_PARENTHESIS, 
	OP_COMMA, OP_DOT, OP_SEMICOLON, OP_QUESTION, OP_COLON, OP_DOUBLE_COLON, 
	OP_GREATER, OP_LESS, OP_EQUAL, OP_NOT_EQUAL, OP_GREATER_EQUAL, OP_LESS_EQUAL,
	OP_ASSIGN, OP_ADD_ASSIGN, OP_MINUS_ASSIGN, OP_PRODUCT_ASSIGN, OP_DIVIDE_ASSIGN, OP_MOD_ASSIGN, 
	OP_INCREMENT, OP_DECREMENT, 

	IDENTIFIER, BOOLEAN, CHAR, INTEGER, REAL, STRING, NULL_REF,
};

const std::vector<std::string> kTokenTypeStr = {
	"NOT_DEFINED",
	"NT_TYPE_CAST", "NT_CALL", "NT_ARRAY",
	"NT_IF", "NT_FOR", "NT_WHILE", "NT_STMT_BLOCK",

	"KEY_IF", "KEY_ELSE", "KEY_WHILE", "KEY_FOR", "KEY_SWITCH", 
	"KEY_CASE", "KEY_STRUCT", "KEY_CLASS", "KEY_CONST", "KEY_DO", 
	"KEY_GOTO", "KEY_BREAK", "KEY_CONTINUE", "KEY_DEFAULT", 
	"KEY_RETURN", "KEY_VOID", "KEY_NEW",

	"OP_ADD", "OP_MINUS", "OP_PRODUCT", "OP_DIVIDE", "OP_MOD",
	"OP_BIT_AND", "OP_BIT_OR", "OP_BIT_NOT", "OP_BIT_XOR",
	"OP_LOGICAL_AND", "OP_LOGICAL_OR", "OP_LOGICAL_NOT",
	"OP_LEFT_BRACE", "OP_RIGHT_BRACE", "OP_LEFT_BRACKET", "OP_RIGHT_BRACKET", "OP_LEFT_PARENTHESIS", "OP_RIGHT_PARENTHESIS",
	"OP_COMMA", "OP_DOT", "OP_SEMICOLON", "OP_QUESTION", "OP_COLON", "OP_DOUBLE_COLON",
	"OP_GREATER", "OP_LESS", "OP_EQUAL", "OP_NOT_EQUAL", "OP_GREATER_EQUAL", "OP_LESS_EQUAL",
	"OP_ASSIGN", "OP_ADD_ASSIGN", "OP_MINUS_ASSIGN", "OP_PRODUCT_ASSIGN", "OP_DIVIDE_ASSIGN", "OP_MOD_ASSIGN",
	"OP_INCREMENT", "OP_DECREMENT",

	"IDENTIFIER", "BOOLEAN", "CHAR", "INTEGER", "REAL", "STRING", "NULL_REF",
};

class Lexer {
public:
	static const std::unordered_map<std::string, TokenType> kKeyWords; // "true" and "false" are viewed as key words(with token type BOOLEAN)
	static const std::unordered_map<std::string, TokenType> kOperators;
	struct Token;

	Lexer() = default;
	Lexer(const std::string& sequence) { Tokenize(sequence); }
	int Tokenize(const std::string &sequence);
	const Token& Current() const;
	const Token& LookNext() const;
	// @return next token, and cursor_ += 1;
	const Token& ToNext();
	// @return current token first, then cursor_ += 1;
	const Token& GoNext();
	bool Consume(TokenType token) {
		//if (Current().type_ != token)
		//	std::clog << "Error " << Current().value_ << " not match, need " << kTokenTypeStr[token] << std::endl;
		assert(Current().type_ == token && "Token Consume not match");
		++cursor_;
		return true;
	}
	void set_cursor(int cursor) { cursor_ = cursor; }

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
private:
	static const Token kNonToken_;
	int cursor_ = 0;
	std::vector<Token> tokens_;
};

} // namespace pswgoo
