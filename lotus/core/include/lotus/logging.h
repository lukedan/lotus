#pragma once

/// \file
/// Logging utilities.

#include <cstdio>
#include <iterator>
#include <string_view>
#include <mutex>
#include <chrono>
#include <format>
#include <source_location>

#include "utils/stack_allocator.h"
#include "utils/strings.h"

namespace lotus {
	/// Class for logging.
	class logger {
	public:
		/// Initializes this logger.
		logger() : _locale("en_US.UTF-8"), _startup(std::chrono::high_resolution_clock::now()) {
		}

		/// Logs a debug entry.
		template <string::constexpr_string fmt, typename ...Args> void debug(
			std::source_location loc, Args &&...args
		) {
			_do_log_fmt<fmt>(loc, u8"DEBUG", std::forward<Args>(args)...);
		}
		/// Logs an info entry.
		template <string::constexpr_string fmt, typename ...Args> void info(
			std::source_location loc, Args &&...args
		) {
			_do_log_fmt<fmt>(loc, u8"INFO", std::forward<Args>(args)...);
		}
		/// Logs a warning entry.
		template <string::constexpr_string fmt, typename ...Args> void warn(
			std::source_location loc, Args &&...args
		) {
			_do_log_fmt<fmt>(loc, u8"WARNING", std::forward<Args>(args)...);
		}
		/// Logs an error entry.
		template <string::constexpr_string fmt, typename ...Args> void error(
			std::source_location loc, Args &&...args
		) {
			_do_log_fmt<fmt>(loc, u8"ERROR", std::forward<Args>(args)...);
		}

		/// Returns the global logger instance.
		[[nodiscard]] static logger &instance();
	protected:
		/// Formats the given log entry and calls \ref _do_log() to log it.
		template <string::constexpr_string fmt, typename ...Args> void _do_log_fmt(
			std::source_location loc, const char8_t *type, Args &&...args
		) {
			std::lock_guard<std::mutex> lock(_lock);

			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto text = bookmark.create_string();
			auto it = std::back_inserter(text);
			constexpr string::constexpr_string fmt_char = fmt.as<char>();
			std::format_to(it, _locale, fmt_char, std::forward<Args>(args)...);

			auto time = std::chrono::high_resolution_clock::now() - _startup;
			_do_log(time, loc, reinterpret_cast<const char*>(type), text.c_str());
		}
		/// Logs the given string to the console.
		void _do_log(
			std::chrono::high_resolution_clock::duration time, std::source_location loc,
			const char *type, const char *text
		) {
			std::fprintf(
				stdout, "[%6.2f] %s:%d:%d|%s [%s] %s\n",
				std::chrono::duration<double>(time).count(),
				loc.file_name(), loc.line(), loc.column(), loc.function_name(),
				type, text
			);
		}

		std::mutex _lock; ///< Only one thread can log at any given time.
		std::locale _locale; ///< Locale used for formatting text.
		std::chrono::high_resolution_clock::time_point _startup; ///< Time when this instance is created.
	};

	struct log_context;
	/// Creates a new \ref log_context object using the global logger.
	[[nodiscard]] log_context log(std::source_location = std::source_location::current());
	/// Context for when a log entry is created.
	struct log_context {
		friend log_context log(std::source_location);
	public:
		/// Logs a debug entry.
		template <string::constexpr_string fmt, typename ...Args> void debug(Args &&...args) {
			_logger.debug<fmt, Args...>(_loc, std::forward<Args>(args)...);
		}
		/// Logs an info entry.
		template <string::constexpr_string fmt, typename ...Args> void info(Args &&...args) {
			_logger.info<fmt, Args...>(_loc, std::forward<Args>(args)...);
		}
		/// Logs a warning entry.
		template <string::constexpr_string fmt, typename ...Args> void warn(Args &&...args) {
			_logger.warn<fmt, Args...>(_loc, std::forward<Args>(args)...);
		}
		/// Logs an error entry.
		template <string::constexpr_string fmt, typename ...Args> void error(Args &&...args) {
			_logger.error<fmt, Args...>(_loc, std::forward<Args>(args)...);
		}
	private:
		/// Initializes all fields of this struct.
		log_context(std::source_location loc, logger &l) : _loc(loc), _logger(l) {
		}

		std::source_location _loc; ///< Location where this context is created.
		logger &_logger; ///< Logger.
	};
}
