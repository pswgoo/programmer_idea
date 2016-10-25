#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace pswgoo {

class VariableType {
public:
	enum PrimeType { kNotVariable, kNull, kBoolean, kChar, kInt, kFloat, kDouble };
	static const std::vector<int> kPrimeTypeWidth;

	VariableType(PrimeType type = kNotVariable, bool is_ref = false) : prime_type_(type), is_ref_(is_ref) { width_.push_back(kPrimeTypeWidth[prime_type_]); }

	bool IsPrimeType() const {
		return array_dim_.empty() && prime_type_ != kNotVariable;
	}
	bool IsArray() const {
		return !array_dim_.empty();
	}
	PrimeType prime_type() const { return prime_type_; }
	const std::vector<int64_t>& width() const { return width_; }
	const std::vector<int64_t>& array_dim() const { return array_dim_; }
	void InsertDim(int64_t width) {
		array_dim_.insert(array_dim_.begin(), width);
		width_.insert(width_.begin(), width*(*width_.begin()));
	}
	void set_is_rvalue(bool rvalue) { is_rvalue_ = rvalue; }
	bool is_rvalue() const { return is_rvalue_; }
	void set_is_const(bool is_const) { is_const_ = is_const; }
	bool is_const() const { return is_const_; }

private:
	PrimeType prime_type_;
	std::vector<int64_t> width_;
	std::vector<int64_t> array_dim_;
	bool is_ref_ = false;
	bool is_rvalue_ = false;
	bool is_const_ = false;
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

	SymbolNode() = default;
	SymbolNode(const std::string& id, SymbolCategory category, const VariableType& type, int64_t address) :
		id_(id), category_(category), type_(type), address_(address) {
	};

	std::string name() const { return id_; };

	std::string id_;
	SymbolCategory category_;
	VariableType type_;
	std::vector<VariableType> param_types_;
	int64_t address_;
	std::string value_; // for const value
};

class SymbolTable {
	static const int64_t kStartAddress = 1;
public:
	SymbolTable(const SymbolTable* parent = nullptr) :parent_(parent), top_address_(parent_->top_address_){}

	SymbolNode* PutTemp(VariableType::PrimeType type) {
		SymbolNode node;
		node.id_ = "#" + std::to_string(temp_idx_++);
		node.category_ = SymbolNode::kVariable;
		node.type_ = VariableType(type);
		node.address_ = Alloc(node.type_.width().front());
		if (Get(node.id_))
			throw std::runtime_error("Generated occured temporary vairable!");
		return Put(node);
	}

	SymbolNode* Put(const SymbolNode& node) {

		table_[node.id_] = node;
		return &table_[node.id_];
	}
	const SymbolNode* Get(const std::string& id) const {
		const SymbolTable *ptr = this;
		while (ptr) {
			if (ptr->table_.find(id) != ptr->table_.end())
				return &ptr->table_.at(id);
			ptr = ptr->parent_;
		}
		return nullptr;
	}

	int64_t Alloc(int64_t length) {
		int64_t ret = top_address_;
		top_address_ += length;
		return ret;
	}

private:
	int temp_idx_ = 0; // temp variable index.
	int64_t top_address_;
	std::unordered_map<std::string, SymbolNode> table_;
	const SymbolTable* parent_;
};

} // namespace pswgoo
