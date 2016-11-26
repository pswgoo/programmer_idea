#pragma once

#include <cstdint>

namespace pswgoo {

struct Instruction {
	enum Opcode {
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

		kAddL,
		kSubL,
		kMulL,
		kDivL,
		kModL,

		kAddF,
		kSubF,
		kMulF,
		kDivF,

		kAddD,
		kSubD,
		kMulD,
		kDivD,

		kCmpC,		//(byte1,byte2)->byte; �Ƚ�ջ������byte��ֵ. if byte1 > byte2, push 1; if byte1 < byte2, push -1; if byte1 == byte2, push 0.
		kCmpI,
		kCmpL,
		kCmpF,
		kCmpD,

		kEq,		// byte1 -> byte; If byte1 is 0, push 1; if byte1 is not 0, push 0.
		kLt,		// byte1 -> byte; If byte1 is -1, push 1, else push 0.
		kGt,		// byte1 -> byte; If byte1 is 1, push 1, else push 0.
		kLe,		// byte1 -> byte; If byte1 is 0 or -1, push 1, else push 0.
		kGe,		// byte1 -> byte; If byte1 is 0 or 1, push 1, else push 0.
		kNe,		// byte1 -> byte; If byte1 is not 0, push 1, else push 0.

		kIfTrue,	//#instr_offset; byte1 -> ; If byte1 is 1, then goto the instr_offset

		kCall,		//#function_symbol_index; (arg1, arg2...) -> result

		kC2I,		// byte1 -> 4_byte; char convert to int.
		kC2L,		// byte1 -> 8_byte; char convert to long.
		kC2F,
		kC2D,
		
		kI2C,		// 4_byte1 -> byte;
		kI2L,
		kI2F,
		kI2D,
		
		kL2C,
		kL2I,
		kL2F,
		kL2D,

		kF2C,
		kF2I,
		kF2L,
		kF2D,

		kD2C,
		kD2I,
		kD2L,
		kD2F,

		kAnd,		// (byte1,byte2) -> byte; if byte1 is 1 and byte2 is 1, push 1; else push 0.
		kOr,
		kNot,		// byte1 -> byte; if byte1 is 0, push 1, if byte1 is 1, push 0.

		kPutC,		//$char_literal; push a char literal value to operand stack.
		kPutI,		//$int_literal; push a int literal value to operand stack.
		kPutL,
		kPutF,
		kPutD,
		kPutN,		//$nullptr; push nullptr to operand stack.

		kLoadC,
		kLoadI,		//@local_stack_index; load a local int variable to operand stack.
		kLoadL,
		kLoadF,
		kLoadD,
		kLoadR,

		kStoreC,
		kStoreI,
		kStoreL,
		kStoreF,
		kStoreD,
		kStoreR,
	};

	Opcode op_;
	int64_t param;
};

} // namespace pswgoo