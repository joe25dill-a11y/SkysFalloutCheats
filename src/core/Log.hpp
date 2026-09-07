#pragma once
#include <string>
#include <cstdarg>

namespace sfc {

enum class LogLevel { Info, Warn, Error, Debug };

void LogInit(const std::string& path);
void LogWrite(LogLevel level, const char* fmt, ...);
void LogShutdown();

} // namespace sfc

#define SFC_LOG(fmt, ...)  ::sfc::LogWrite(::sfc::LogLevel::Info,  fmt, ##__VA_ARGS__)
#define SFC_WARN(fmt, ...) ::sfc::LogWrite(::sfc::LogLevel::Warn,  fmt, ##__VA_ARGS__)
#define SFC_ERR(fmt, ...)  ::sfc::LogWrite(::sfc::LogLevel::Error, fmt, ##__VA_ARGS__)
#define SFC_DBG(fmt, ...)  ::sfc::LogWrite(::sfc::LogLevel::Debug, fmt, ##__VA_ARGS__)
