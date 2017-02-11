#pragma once

#include "lexer.h"
#include "symbol.h"
#include "instruction.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace pswgoo {

struct FunctionNode {
	Function function_type_;
	std::vector<Instruction> code_;
	int local_stack_size_;
	std::vector<int> local_variable_offset_;
};

class ConstPool {
public:
	enum ConstPoolNodeType { kCharNode, kIntNode, kDoubleNode, kStringNode, kFunctionNode };
	struct ConstPoolNode {
		ConstPoolNodeType type_;
		int offset_;
	};

	std::vector<ConstPoolNode> all_constants_;

	std::vector<char> char_pool_;
	std::vector<int64_t> int_pool_;
	std::vector<double> double_pool_;
	std::vector<std::string> string_pool_;
	std::vector<FunctionNode> function_pool_;
};

class Frame {
public:
	

	int64_t next_instr_;
	std::vector<char> local_stack_;
};

class VirtualMachine {
public:

	void LoadConst();

	void Run();

private:
	void PushFrame();
	void PopFrame();

private:
	int64_t instr_pc_;
	std::vector<std::unique_ptr<Frame>> frame_stack_;
	std::vector<char> data_stack_;
	ConstPool const_pool_;
};

} // namespace pswgoo
