#include "symbol_table.h"

namespace pswgoo {

//enum ValueType									   { kNotVariable, kNull, kBoolean, kChar, kInt, kFloat, kDouble, kPointer };
const std::vector<int> VariableType::kPrimeTypeWidth = { 1,                1, 1,        1,     4,    4,      8,       8 };

}// namespace pswgoo