#pragma once

#include <cstdint>

namespace pswgoo {

struct Instruction {
	enum Opcode {
		kNonCmd,
		kRefA,		//�������飬ȡջ�� 8 byte ��Ϊ�±�
		kNewA,		//�������飬����һ��int64��Ϊdimention����. �������鶼��ƽ��Ϊһά����
		kNew,		//�������ã�����һ�� ���ű��±����

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

		kCmpC,		//(byte1,byte2)->byte; �Ƚ�ջ������byte��ֵ. if byte1 > byte2, push 1; if byte1 < byte2, push -1; if byte1 == byte2, push 0.
		kCmpI,
		kCmpD,

		kEq,		// byte1 -> byte; If byte1 is 0, push 1; if byte1 is not 0, push 0.
		kLt,		// byte1 -> byte; If byte1 is -1, push 1, else push 0.
		kGt,		// byte1 -> byte; If byte1 is 1, push 1, else push 0.
		kLe,		// byte1 -> byte; If byte1 is 0 or -1, push 1, else push 0.
		kGe,		// byte1 -> byte; If byte1 is 0 or 1, push 1, else push 0.
		kNe,		// byte1 -> byte; If byte1 is not 0, push 1, else push 0.

		kIfFalse,	//#instr_offset; byte1 -> ; If byte1 is 0, then goto the instr_offset

		kCall,		//#function_symbol_index; (arg1, arg2...) -> result

		kC2I,		// byte1 -> 8_byte; char convert to int.
		kC2D,
		
		kL2C,
		kL2D,

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

		kALoadC,	// (index, arrayref) -> value
		kALoadI,	
		kALoadD,
		kALoadR,

		kAStoreC,	// (index, arrayref, value) -> 
		kAStoreI,
		kAStoreD,
		kAStoreR,

		kLdc,		// #constant_pool_index, if it is a string index, push the run-time reference to the operand stack.

		kGetStatic, // #constant_pool_index, get static variable.
		kStoreStatic, // #constant_pool_index, store static variable.
	};

	Instruction(Opcode op, int64_t param) : op_(op), param_(param) {}

	Opcode op_;
	int64_t param_;
};

} // namespace pswgoo
