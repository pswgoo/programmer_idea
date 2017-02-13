#include "virtual_machine.h"

using namespace std;

namespace pswgoo {

void VirtualMachine::Init(const Scope* scope) {
	const unordered_map<string, SymbolPtr>& symbols = scope->symbol_table();

	vector<Symbol*> sorted_symbols;
	for (const pair<const string, SymbolPtr>& pr : symbols)
		sorted_symbols.push_back(pr.second.get());
	sort(sorted_symbols.begin(), sorted_symbols.end(), [](const Symbol* p1, const Symbol* p2) { return p1->index_ < p2->index_; });

	for (int i = 0; i < sorted_symbols.size(); ++i) {
		if (sorted_symbols[i])
	}
}

} // namespace pswgoo