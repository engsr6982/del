#include "symbol_table.h"

namespace del {

void SymbolTable::Register(std::string name, CustomFunction fn) { functions_[std::move(name)] = std::move(fn); }

CustomFunction const* SymbolTable::Lookup(std::string_view name) const {
  auto it = functions_.find(std::string(name));
  if (it != functions_.end()) {
    return &it->second;
  }
  return nullptr;
}


} // namespace del