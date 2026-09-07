#include "core/SearchIndex.hpp"
#include "core/FeatureRegistry.hpp"
#include <algorithm>
#include <cctype>

namespace sfc {
namespace {
std::string Lower(std::string s)
{
	for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

int Score(const SearchEntry& e, const std::string& q)
{
	if (q.empty()) return 1;
	const std::string title = Lower(e.title);
	const std::string keys = Lower(e.keywords);
	const std::string id = Lower(e.id);
	if (title == q) return 100;
	if (title.find(q) == 0) return 80;
	if (title.find(q) != std::string::npos) return 60;
	if (id.find(q) != std::string::npos) return 50;
	if (keys.find(q) != std::string::npos) return 40;
	// fuzzy: all chars in order
	size_t at = 0;
	for (char c : q) {
		at = title.find(c, at);
		if (at == std::string::npos) return 0;
		++at;
	}
	return 15;
}
}

SearchIndex& SearchIndex::Get()
{
	static SearchIndex instance;
	return instance;
}

void SearchIndex::Rebuild()
{
	entries_.clear();
	for (auto& f : FeatureRegistry::Get().All()) {
		f->CollectSearch(entries_);
		entries_.push_back(SearchEntry{
			f->Id(),
			f->Name(),
			std::string(CategoryName(f->Category())) + " " + f->Id(),
			f->Category(),
			nullptr
		});
	}
}

std::vector<SearchEntry> SearchIndex::Query(const std::string& text, int limit) const
{
	const std::string q = Lower(text);
	std::vector<std::pair<int, const SearchEntry*>> scored;
	for (auto& e : entries_) {
		int s = Score(e, q);
		if (s > 0) scored.push_back({s, &e});
	}
	std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });
	std::vector<SearchEntry> out;
	for (int i = 0; i < static_cast<int>(scored.size()) && i < limit; ++i)
		out.push_back(*scored[i].second);
	return out;
}

} // namespace sfc
