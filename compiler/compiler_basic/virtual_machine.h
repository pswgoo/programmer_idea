#pragma once

#include "lexer.h"
#include "symbol.h"
#include "instruction.h"
#include "parser.h"

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace pswgoo {

struct FunctionNode {
	std::string name_;
	const Function* function_type_;
	std::vector<Instruction> code_;
	int64_t local_stack_size_;
	std::vector<int64_t> local_variable_offset_;
};

class ConstPool {
public:
	enum ConstPoolNodeType { kNonNode, kCharNode, kIntNode, kDoubleNode, kStringNode, kFunctionNode, kTypeNode };
	struct ConstPoolNode {
		ConstPoolNodeType type_;
		int offset_;
	};

	char GetChar(int64_t offset) {
		assert(all_constants_[offset].type_ == kCharNode);
		return char_pool_[all_constants_[offset].offset_];
	}
	int64_t GetInt(int64_t offset) {
		assert(all_constants_[offset].type_ == kIntNode);
		return int_pool_[all_constants_[offset].offset_];
	}
	double GetDouble(int64_t offset) {
		assert(all_constants_[offset].type_ == kDoubleNode);
		return double_pool_[all_constants_[offset].offset_];
	}
	const Type* GetType(int64_t offset) {
		assert(all_constants_[offset].type_ == kTypeNode);
		return type_pool_[all_constants_[offset].offset_];
	}
	void StoreChar(int64_t offset, char value) {
		assert(all_constants_[offset].type_ == kCharNode);
		char_pool_[all_constants_[offset].offset_] = value;
	}
	void StoreInt(int64_t offset, int64_t value) {
		assert(all_constants_[offset].type_ == kIntNode);
		int_pool_[all_constants_[offset].offset_] = value;
	}
	void StoreDouble(int64_t offset, double value) {
		assert(all_constants_[offset].type_ == kDoubleNode);
		double_pool_[all_constants_[offset].offset_] = value;
	}

	int AddSymbol(const Symbol* symbol);

	void Print(std::ostream& oa, const std::string& padding) const;

	std::vector<ConstPoolNode> all_constants_;

	std::vector<char> char_pool_;
	std::vector<int64_t> int_pool_;
	std::vector<double> double_pool_;
	std::vector<std::string> string_pool_;
	std::vector<FunctionNode> function_pool_;
	std::vector<const Type*> type_pool_;
};

class Frame {
public:
	
	void Push(const std::vector<char>& buffer) {
		data_stack_.insert(data_stack_.end(), buffer.begin(), buffer.end());
	}
	std::vector<char> Pop(int n) {
		if (!n)
			return{};
		std::vector<char>::iterator s = data_stack_.begin() + (data_stack_.size() - n);
		std::vector<char> ret(s, data_stack_.end());
		data_stack_.resize(data_stack_.size() - n);
		return ret;
	}

	ConstPool* ptr_const_pool_;
	const FunctionNode *ptr_function_;
	std::vector<char> data_stack_;
	int64_t next_instr_;
	std::vector<char> local_stack_;
};

class VirtualMachine {
public:
	void Init(const Scope* scope);

	int64_t Run();

	void Print(std::ostream& oa, const std::string& padding) const;

private:
	void Push(const std::vector<char>& buffer) {
		frame_stack_.back()->Push(buffer);
	}
	std::vector<char> Pop(int n) {
		return frame_stack_.back()->Pop(n);
	}
	char GetLocalChar(int64_t offset);
	int64_t GetLocalInt(int64_t offset);
	double GetLocalDouble(int64_t offset);
	void StoreLocalChar(int64_t offset, char value);
	void StoreLocalInt(int64_t offset, int64_t value);
	void StoreLocalDouble(int64_t offset, double value);

	void PushFrame(const FunctionNode& function);
	void PopFrame();

	static std::vector<char> ToBuffer(char ch);
	static std::vector<char> ToBuffer(int64_t num);
	static std::vector<char> ToBuffer(double num);
	static int64_t ToInt(const std::vector<char>& buffer);
	static double ToDouble(const std::vector<char>& buffer);

private:
	int64_t instr_pc_;
	const std::vector<Instruction>* ptr_code_;
	std::vector<std::unique_ptr<Frame>> frame_stack_;
	ConstPool const_pool_;
};

} // namespace pswgoo
