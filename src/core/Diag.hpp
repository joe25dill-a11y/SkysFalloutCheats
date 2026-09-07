#pragma once
#include <cstdint>

namespace sfc {

// Throttled forensic diagnostics for crash/freeze investigation.
// Enable via config.json → "diagnostics": { "enabled": true }.
// Logs: [DIAG] seq=N op=... start|ok|fail ms=... detail=...

bool DiagEnabled();
void DiagSetEnabled(bool on);
std::uint64_t DiagNextSeq();

void DiagEvent(const char* op, const char* detail = nullptr);
void DiagBegin(std::uint64_t seq, const char* op, const char* detail = nullptr);
void DiagEnd(std::uint64_t seq, const char* op, bool ok, unsigned durationMs, const char* detail = nullptr);

// At most one line per key every minIntervalMs (when diagnostics enabled).
void DiagThrottle(const char* key, unsigned minIntervalMs, const char* fmt, ...);

// RAII start/end for a single operation.
class DiagScope {
public:
	explicit DiagScope(const char* op, const char* detail = nullptr);
	~DiagScope();
	void Fail(const char* detail = nullptr);
	void Ok(const char* detail = nullptr);
	std::uint64_t Seq() const { return seq_; }

private:
	const char* op_;
	std::uint64_t seq_ = 0;
	unsigned startMs_ = 0;
	bool finished_ = true;
	bool ok_ = true;
};

} // namespace sfc
