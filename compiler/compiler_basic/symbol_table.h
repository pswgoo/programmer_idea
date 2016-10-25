#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace pswgoo {

class VariableType {
public:
	enum PrimeType { kNotVariable, kBoolean, kChar, kInt, kFloat, kDouble };
	static const std::vector<int> kPrimeTypeWidth;

	VariableType(PrimeType type = kNotVariable) : prime_type_(type){ width_.push_back(kPrimeTypeWidth[prime_type_]); }

	bool IsPrimeType() const {
		return array_dim_.empty() && prime_type_ != kNotVariable;
	}
	bool IsArray() const {
		return !array_dim_.empty();
	}
	PrimeType prime_type() const { return prime_type_; }
	const std::vector<int>& width() const { return width_; }
private:
	PrimeType prime_type_;
	std::vector<int> width_;
	std::vector<int> array_dim_;
	bool is_ref_ = false;
};

VariableType MaxType(const VariableType& lhs, const VariableType& rhs) {
	if (lhs.IsPrimeType() && rhs.IsPrimeType()) {
		if (lhs.prime_type() > rhs.prime_type())
			return lhs;
		else
			return rhs;
	}
	else
		return VariableType();
}

struct SymbolNode {
	enum SymbolCategory {kFunction, kVariable};
	std::string id_;
	SymbolCategory category_;
	VariableType type_;
	std::vector<VariableType> param_types_;
	int64_t address_;
};

class SymbolTable {
public:
	SymbolTable(const SymbolTable* parent = nullptr) :parent_(parent), top_address_(parent_->top_address_){}

	SymbolNode* PutTemp(VariableType::PrimeType type) {
		SymbolNode node;
		node.id_ = "#" + std::to_string(temp_idx_);
		node.category_ = SymbolNode::kVariable;
		node.type_ = VariableType(type);
		node.address_ = Alloc(node.type_.width().back());
		if (Get(node.id_))
			throw std::runtime_error("Generated occured temporary vairable!");
		return Put(node);
	}

	SymbolNode* Put(const SymbolNode& node) {
		table_[node.id_] = node;
		return &table_[node.id_];
	}
	const SymbolNode* Get(const std::string& id) const {
		if (table_.find(id) != table_.end())
			return &table_.at(id);
		else
			return nullptr;
	}

	int64_t Alloc(int64_t length) {
		int64_t ret = top_address_;
		top_address_ += length;
		return ret;
	}

private:
	int temp_idx_ = 0;
	int64_t top_address_;
	std::unordered_map<std::string, SymbolNode> table_;
	const SymbolTable* parent_;
};

} // namespace pswgoo
