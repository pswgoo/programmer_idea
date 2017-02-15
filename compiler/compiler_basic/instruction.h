#pragma once

#include <cstdint>
#include <string>
//
//#ifndef _CRT_SECURE_NO_WARNINGS
//#define _CRT_SECURE_NO_WARNINGS
//#endif

namespace pswgoo {

const std::string kInstructionStr[] = {
	"kNonCmd",
	"kNewA",	// #constant_pool_index, count -> ArrayRef; Create a array reference.
	"kNew",		//创建引用，接受一个 符号表下标参数

	"kAddC",
	"kSubC",
	"kMulC",
	"kDivC",
	"kModC",

	"kAddI",
	"kSubI",
	"kMulI",
	"kDivI",
	"kModI",

	"kAddD",
	"kSubD",
	"kMulD",
	"kDivD",

	"kCmpC",		//(byte1",byte2)->byte; 比较栈顶两个byte的值. if byte1 > byte2", push 1; if byte1 < byte2", push -1; if byte1 == byte2", push 0.
	"kCmpI",
	"kCmpD",

	"kEq",		// byte1 -> byte; If byte1 is 0", push 1; if byte1 is not 0", push 0.
	"kLt",		// byte1 -> byte; If byte1 is -1", push 1", else push 0.
	"kGt",		// byte1 -> byte; If byte1 is 1", push 1", else push 0.
	"kLe",		// byte1 -> byte; If byte1 is 0 or -1", push 1", else push 0.
	"kGe",		// byte1 -> byte; If byte1 is 0 or 1", push 1", else push 0.
	"kNe",		// byte1 -> byte; If byte1 is not 0", push 1", else push 0.

	"kIfFalse",	//#instr_offset; byte1 -> ; If byte1 is 0", then goto the instr_offset
	"kGoto",	// #instr_offset;

	"kCall",		//#function_symbol_index; (arg1", arg2...) -> result

	"kC2I",		// byte1 -> 8_byte; char convert to int.
	"kC2D",

	"kI2C",
	"kI2D",

	"kD2C",
	"kD2I",

	"kNegC",		// negate a char
	"kNegI",		// negate a int
	"kNegD",

	"kAnd",		// (byte1",byte2) -> byte; if byte1 is 1 and byte2 is 1", push 1; else push 0.
	"kOr",
	"kNot",		// byte1 -> byte; if byte1 is 0", push 1", if byte1 is 1", push 0.

	"kPutC",		// $char_literal; push a char literal value to operand stack.
	"kPutI",
	"kPutD",
	"kPutN",		// $nullptr; push nullptr to operand stack.

	"kLoadC",
	"kLoadI",     // @local_stack_index; load a local int variable to operand stack.
	"kLoadD",
	"kLoadR",

	"kStoreC",
	"kStoreI",	// (byte1",byte2",byte3",byte4) -> @local_stack_index; 
	"kStoreD",
	"kStoreR",

	"kALoadC",	// (arrayref", index) -> value
	"kALoadI",
	"kALoadD",
	"kALoadR",

	"kAStoreC",	// (arrayref", index", value) -> 
	"kAStoreI",
	"kAStoreD",
	"kAStoreR",

	"kLdc",		// #constant_pool_index", if it is a string index", push the run-time reference to the operand stack.

	"kGetStatic", // #constant_pool_index", get static variable.
	"kStoreStatic", // #constant_pool_index", store static variable.

	"kReturnC",	// return a byte
	"kReturnI",	// return a integer
	"kReturnD",	// return a double
	"kReturnR",	// return a reference
	"kReturn",	// return void
};

struct Instruction {
	enum Opcode {
		kNonCmd,
		kNewA,		// #constant_pool_index, count -> ArrayRef; Create a array reference.
		kNew,		//创建引用，接受一个 符号表下标参数

		kAddC,
		kSubC,
		kMulC,
		kDivC,
		kModC,

		kAddI,
		kSubI,
		kMulI,
		kDivI,
		kModI,

		kAddD,
		kSubD,
		kMulD,
		kDivD,

		kCmpC,		//(byte1,byte2)->byte; 比较栈顶两个byte的值. if byte1 > byte2, push 1; if byte1 < byte2, push -1; if byte1 == byte2, push 0.
		kCmpI,
		kCmpD,

		kEq,		// byte1 -> byte; If byte1 is 0, push 1; if byte1 is not 0, push 0.
		kLt,		// byte1 -> byte; If byte1 is -1, push 1, else push 0.
		kGt,		// byte1 -> byte; If byte1 is 1, push 1, else push 0.
		kLe,		// byte1 -> byte; If byte1 is 0 or -1, push 1, else push 0.
		kGe,		// byte1 -> byte; If byte1 is 0 or 1, push 1, else push 0.
		kNe,		// byte1 -> byte; If byte1 is not 0, push 1, else push 0.

		kIfFalse,	// #instr_offset; byte1 -> ; If byte1 is 0, then goto the instr_offset
		kGoto,		// #instr_offset;

		kCall,		// #function_symbol_index; (arg1, arg2...) -> result

		kC2I,		// byte1 -> 8_byte; char convert to int.
		kC2D,

		kI2C,
		kI2D,

		kD2C,
		kD2I,

		kNegC,		// negate a char
		kNegI,		// negate a int
		kNegD,

		kAnd,		// (byte1,byte2) -> byte; if byte1 is 1 and byte2 is 1, push 1; else push 0.
		kOr,
		kNot,		// byte1 -> byte; if byte1 is 0, push 1, if byte1 is 1, push 0.

		kPutC,		// $char_literal; push a char literal value to operand stack.
		kPutI,
		kPutD,
		kPutN,		// $nullptr; push nullptr to operand stack.

		kLoadC,
		kLoadI,     // @local_stack_index; load a local int variable to operand stack.
		kLoadD,
		kLoadR,

		kStoreC,
		kStoreI,	// (byte1,byte2,byte3,byte4) -> @local_stack_index; 
		kStoreD,
		kStoreR,

		kALoadC,	// (arrayref, index) -> value;  arrayref is on the stack top.
		kALoadI,	
		kALoadD,
		kALoadR,

		kAStoreC,	// (arrayref, index, value) -> ;   arrayref is on the stack top. 
		kAStoreI,
		kAStoreD,
		kAStoreR,

		kLdc,		// #constant_pool_index. For a string index, push the run-time reference to the operand stack.

		kGetStatic, // #constant_pool_index, get static variable.
		kStoreStatic, // #constant_pool_index, store static variable.

		kReturnC,	// return a byte
		kReturnI,	// return a integer
		kReturnD,	// return a double
		kReturnR,	// return a reference
		kReturn,	// return void
	};

	Instruction(Opcode op, int64_t param) : op_(op), param_(param) {}

	std::string ToString() const {
		return kInstructionStr[op_] + "\t" + std::to_string(param_);
	}

	Opcode op_;
	int64_t param_;
};

} // namespace pswgoo
