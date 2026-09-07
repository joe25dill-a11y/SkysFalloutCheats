#include "core/Log.hpp"
#include <windows.h>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <ctime>

namespace sfc {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}

void LogInit(const std::string& path)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_file) {
		std::fclose(g_file);
		g_file = nullptr;
	}
	g_file = std::fopen(path.c_str(), "a");
	if (g_file) {
		std::fputs("\n==== Sky's Fallout Cheats log session ====\n", g_file);
		std::fflush(g_file);
	}
}

void LogWrite(LogLevel level, const char* fmt, ...)
{
	char body[2048];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(body, sizeof(body), fmt, args);
	va_end(args);

	const char* tag = "INFO";
	switch (level) {
	case LogLevel::Warn:  tag = "WARN"; break;
	case LogLevel::Error: tag = "ERR "; break;
	case LogLevel::Debug: tag = "DBG "; break;
	default: break;
	}

	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};
	localtime_s(&tm, &t);
	char stamp[32];
	std::snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

	char line[2300];
	std::snprintf(line, sizeof(line), "[%s][%s] %s\n", stamp, tag, body);

	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_file) {
		std::fputs(line, g_file);
		std::fflush(g_file);
	}
	OutputDebugStringA(line);
}

void LogShutdown()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_file) {
		std::fclose(g_file);
		g_file = nullptr;
	}
}

} // namespace sfc
