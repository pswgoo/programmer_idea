#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "lexer.h"
#include "symbol_table.h"

namespace pswgoo {

class ClAstNode {
public:
	ClAstNode() = default;
	ClAstNode(Lexer& lexer) { Parse(lexer, nullptr); };
	ClAstNode(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent) { Parse(lexer, move(parent)); };

	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& inherit) = 0;

protected:
	std::string value_;
	std::vector<std::unique_ptr<ClAstNode>> children_;
};

class ClProg1Node: public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClProg2Node : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClProg3Node : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClStmtNode : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClStmt1Node : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClFuncDefNode : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClFuncDef1Node : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class TypeNode : public ClAstNode {
public:
	TypeNode(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent):ClAstNode(lexer, move(parent)) {}
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);
};
class ClExprNode : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};
class ClDeclNode : public ClAstNode {
public:
	virtual std::unique_ptr<ClAstNode> Parse(Lexer& lexer, std::unique_ptr<ClAstNode>&& parent);

};

class ClParser {
public:
	

private:
	Lexer lexer_;
};

} // namespace pswgoo
