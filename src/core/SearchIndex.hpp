#pragma once
#include "core/Feature.hpp"
#include <vector>
#include <string>

namespace sfc {

class SearchIndex {
public:
	static SearchIndex& Get();
	void Rebuild();
	std::vector<SearchEntry> Query(const std::string& text, int limit = 20) const;
	const std::vector<SearchEntry>& All() const { return entries_; }

private:
	std::vector<SearchEntry> entries_;
};

} // namespace sfc
