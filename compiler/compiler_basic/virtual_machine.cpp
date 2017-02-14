#include "virtual_machine.h"

using namespace std;

namespace pswgoo {

vector<int> kSizeOf = { 1, 1, 8, 8, 0, 8, 8, 8, 8, 8 };

int ConstPool::AddSymbol(const Symbol* symbol) {
	if ((int64_t)all_constants_.size() <= symbol->index_)
		all_constants_.resize(symbol->index_ + 1);
	if (symbol->Is<LiteralSymbol>() || symbol->Is<ImmediateNode>()) {
		Type::TypeId type_id = symbol->To<ConstSymbol>()->type_->type_id_;
		const string& value = symbol->Is<LiteralSymbol>() ? symbol->To<LiteralSymbol>()->value_ : symbol->To<ImmediateSymbol>()->literal_symbol_->value_;
		if (type_id == Type::kChar) {
			all_constants_[symbol->index_] = { kCharNode, (int)char_pool_.size() };
			char_pool_.push_back(value.front());
		}
		else if (type_id == Type::kInt) {
			all_constants_[symbol->index_] = { kIntNode, (int)int_pool_.size() };
			int_pool_.push_back(stoll(value));
		}
		else if (type_id == Type::kDouble) {
			all_constants_[symbol->index_] = { kDoubleNode, (int)double_pool_.size() };
			double_pool_.push_back(stod(value));
		}
		else if (type_id == Type::kString) {
			all_constants_[symbol->index_] = { kStringNode, (int)string_pool_.size() };
			string_pool_.push_back(value);
		}
		return 1;
	}
	else if (symbol->Is<FunctionSymbol>()) {
		const FunctionSymbol* func_symbol = symbol->To<FunctionSymbol>();
		FunctionNode function;
		function.name_ = func_symbol->name();
		function.function_type_ = func_symbol->type_->To<Function>();
		function.code_ = func_symbol->code_;
		function.local_stack_size_ = func_symbol->scope_->max_stack_size();
		vector<Symbol*> local_symbols = func_symbol->scope_->GetLocalVariables();
		for (Symbol* var : local_symbols)
			function.local_variable_offset_.push_back(var->local_offset_);
		all_constants_[symbol->index_] = { kFunctionNode, (int)function_pool_.size() };
		function_pool_.emplace_back(move(function));
		return 1;
	}
	return 0;
}

void ConstPool::Print(std::ostream& oa, const std::string& padding) const {
	static const vector<string> kNodeDesc = { "NonNode", "CharNode", "IntNode", "DoubleNode", "StringNode", "FunctionNode" };

	for (int i = 0; i < all_constants_.size(); ++i) {
		if (all_constants_[i].type_ == kNonNode)
			continue;

		oa << padding << i << "\t" << kNodeDesc[all_constants_[i].type_];
		if (all_constants_[i].type_ == kCharNode)
			oa << "\t" << char_pool_[all_constants_[i].offset_] << endl;
		if (all_constants_[i].type_ == kIntNode)
			oa << "\t" << int_pool_[all_constants_[i].offset_] << endl;
		if (all_constants_[i].type_ == kDoubleNode)
			oa << "\t" << double_pool_[all_constants_[i].offset_] << endl;
		if (all_constants_[i].type_ == kStringNode)
			oa << "\t" << string_pool_[all_constants_[i].offset_] << endl;
		if (all_constants_[i].type_ == kFunctionNode)
			oa << "\t" << function_pool_[all_constants_[i].offset_].name_ << endl;
	}
}

std::vector<char> VirtualMachine::ToBuffer(char ch) {
	return{ ch };
}
std::vector<char> VirtualMachine::ToBuffer(int64_t num) {
	int64_t* ptr = &num;
	vector<char> ret;
	for (int i = 0; i < sizeof(int64_t); ++i)
		ret.push_back(reinterpret_cast<char*>(ptr)[i]);
	return ret;
}
std::vector<char> VirtualMachine::ToBuffer(double num) {
	double* ptr = &num;
	vector<char> ret;
	for (int i = 0; i < sizeof(double); ++i)
		ret.push_back(reinterpret_cast<char*>(ptr)[i]);
	return ret;
}
int64_t VirtualMachine::ToInt(const std::vector<char>& buffer) {
	if (sizeof(int64_t) != buffer.size())
		assert(0 && "buffer size not match int64_t");
	return *(reinterpret_cast<const int64_t*>(&buffer[0]));
}
double VirtualMachine::ToDouble(const std::vector<char>& buffer) {
	if (sizeof(double) != buffer.size())
		assert(0 && "buffer size not match double");
	return *(reinterpret_cast<const double*>(&buffer[0]));
}

void VirtualMachine::Init(const Scope* scope) {
	const unordered_map<string, SymbolPtr>& symbols = scope->symbol_table();

	vector<Symbol*> sorted_symbols;
	for (const pair<const string, SymbolPtr>& pr : symbols)
		sorted_symbols.push_back(pr.second.get());
	sort(sorted_symbols.begin(), sorted_symbols.end(), [](const Symbol* p1, const Symbol* p2) { return p1->index_ < p2->index_; });

	for (int i = 0; i < sorted_symbols.size(); ++i)
		const_pool_.AddSymbol(sorted_symbols[i]);

	for (int i = 0; i < const_pool_.all_constants_.size(); ++i) {
		const ConstPool::ConstPoolNode& node = const_pool_.all_constants_[i];
		if (node.type_ == ConstPool::kFunctionNode && const_pool_.function_pool_[node.offset_].name_ == "main") {
			PushFrame(const_pool_.function_pool_[node.offset_]);
			break;
		}
	}
}

void VirtualMachine::Print(std::ostream& oa, const std::string& padding) const {
	oa << "Const Pool: " << endl;
	const_pool_.Print(oa, padding + kIndent);

}

void VirtualMachine::PushFrame(const FunctionNode& function) {
	unique_ptr<Frame> frame(new Frame);
	frame->next_instr_ = instr_pc_;
	instr_pc_ = 0;
	frame->ptr_const_pool_ = &const_pool_;
	frame->ptr_function_ = &function;
	int param_num = (int)function.function_type_->param_types_.size();
	int param_stack_size = (int)function.local_stack_size_;
	if (function.local_variable_offset_.size() > param_num)
		param_stack_size = (int)function.local_variable_offset_[param_num];

	// copy function params to local_stack
	vector<char> params = frame_stack_.back()->Pop(param_stack_size);
	frame->local_stack_.resize(function.local_stack_size_);
	for (int i = 0; i < params.size(); ++i)
		frame->local_stack_[i] = params[i];

	frame_stack_.emplace_back(move(frame));
	ptr_code_ = &(frame->ptr_function_->code_);
}

void VirtualMachine::PopFrame() {
	int ret_size = 0;
	if (frame_stack_.back()->ptr_function_->function_type_->ret_type_)
		ret_size = (int)frame_stack_.back()->ptr_function_->function_type_->ret_type_->SizeOf();

	vector<char> ret_value = frame_stack_.back()->Pop(ret_size);
	int64_t next_instr = frame_stack_.back()->next_instr_;
	frame_stack_.pop_back();
	instr_pc_ = next_instr;
	frame_stack_.back()->Push(ret_value);
	ptr_code_ = &(frame_stack_.back()->ptr_function_->code_);
}

void VirtualMachine::Run() {
	while (true) {
		if ((int64_t)ptr_code_->size() <= instr_pc_ && frame_stack_.size() > 1)
			PopFrame();
		else if ((int64_t)ptr_code_->size() <= instr_pc_ && frame_stack_.size() <= 1)
			break;

		switch (Instruction::Opcode op = (*ptr_code_)[instr_pc_].op_) {
		case Instruction::kNewA:
		case Instruction::kNew:

		case Instruction::kAddC:
		case Instruction::kMulC:
		case Instruction::kDivC:
		case Instruction::kModC:
		case Instruction::kSubC: {
			char lhs = Pop(kSizeOf[Type::kChar]).front();
			char rhs = Pop(kSizeOf[Type::kChar]).front();
			char result = 0;
			if (op == Instruction::kAddC)
				result = lhs + rhs;
			if (op == Instruction::kMulC)
				result = lhs * rhs;
			if (op == Instruction::kDivC)
				result = lhs / rhs;
			if (op == Instruction::kModC)
				result = lhs % rhs;
			if (op == Instruction::kSubC)
				result = lhs - rhs;
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kAddI:
		case Instruction::kSubI:
		case Instruction::kMulI:
		case Instruction::kDivI:
		case Instruction::kModI:{
			int64_t lhs = ToInt(Pop(kSizeOf[Type::kInt]));
			int64_t rhs = ToInt(Pop(kSizeOf[Type::kInt]));
			int64_t result = 0;
			if (op == Instruction::kAddI)
				result = lhs + rhs;
			if (op == Instruction::kMulI)
				result = lhs * rhs;
			if (op == Instruction::kDivI)
				result = lhs / rhs;
			if (op == Instruction::kModI)
				result = lhs % rhs;
			if (op == Instruction::kSubI)
				result = lhs - rhs;
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kAddD:
		case Instruction::kSubD:
		case Instruction::kMulD:
		case Instruction::kDivD:{
			double lhs = ToDouble(Pop(kSizeOf[Type::kDouble]));
			double rhs = ToDouble(Pop(kSizeOf[Type::kDouble]));
			double result = 0;
			if (op == Instruction::kAddD)
				result = lhs + rhs;
			if (op == Instruction::kMulD)
				result = lhs * rhs;
			if (op == Instruction::kDivD)
				result = lhs / rhs;
			if (op == Instruction::kSubD)
				result = lhs - rhs;
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kCmpC: {
			char lhs = Pop(kSizeOf[Type::kChar]).front();
			char rhs = Pop(kSizeOf[Type::kChar]).front();
			char result = char(lhs > rhs ? 1 : (lhs < rhs ? -1 : 0));
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kCmpI: {
			int64_t lhs = ToInt(Pop(kSizeOf[Type::kInt]));
			int64_t rhs = ToInt(Pop(kSizeOf[Type::kInt]));
			char result = char(lhs > rhs ? 1 : (lhs < rhs ? -1 : 0));
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kCmpD: {
			double lhs = ToDouble(Pop(kSizeOf[Type::kDouble]));
			double rhs = ToDouble(Pop(kSizeOf[Type::kDouble]));
			char result = char(lhs > rhs ? 1 : (lhs < rhs ? -1 : 0));
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kEq: {
			char lhs = Pop(kSizeOf[Type::kChar]).front();
			char result = char(lhs == 0 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kLt: {
			char lhs = Pop(kSizeOf[Type::kChar]).front();
			char result = char(lhs == -1 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kGt: {
			char lhs = Pop(kSizeOf[Type::kInt]).front();
			char result = char(lhs == 1 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kLe:{
			char lhs = Pop(kSizeOf[Type::kInt]).front();
			char result = char(lhs == -1 || lhs == 0 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kGe:{
			char lhs = Pop(kSizeOf[Type::kInt]).front();
			char result = char(lhs == 1 || lhs == 0 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		case Instruction::kNe: {
			char lhs = Pop(kSizeOf[Type::kInt]).front();
			char result = char(lhs != 0 ? 1 : 0);
			Push(ToBuffer(result));
			break;
		}
		default:
			break;
		}
	}
}

} // namespace pswgoo
