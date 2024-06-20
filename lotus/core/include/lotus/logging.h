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

#include "memory/stack_allocator.h"
#include "utils/strings.h"

namespace lotus {
	/// Class for logging.
	class logger {
	public:
		/// Initializes this logger.
		logger() : _startup(std::chrono::high_resolution_clock::now()) {
		}

		/// Logs a debug entry.
		void debug(std::source_location loc, std::string_view fmt, std::format_args args) {
			_do_log_fmt(loc, u8"DEBUG", console::color::dark_gray, fmt, std::move(args));
		}
		/// Logs an info entry.
		void info(std::source_location loc, std::string_view fmt, std::format_args args) {
			_do_log_fmt(loc, u8"INFO", console::color::white, fmt, std::move(args));
		}
		/// Logs a warning entry.
		void warn(std::source_location loc, std::string_view fmt, std::format_args args) {
			_do_log_fmt(loc, u8"WARNING", console::color::orange, fmt, std::move(args));
		}
		/// Logs an error entry.
		void error(std::source_location loc, std::string_view fmt, std::format_args args) {
			_do_log_fmt(loc, u8"ERROR", console::color::red, fmt, std::move(args));
		}

		/// Returns the global logger instance.
		[[nodiscard]] static logger &instance();
	protected:
		/// Formats the given log entry and calls \ref _do_log() to log it.
		void _do_log_fmt(
			std::source_location loc, const char8_t *type, console::color c,
			std::string_view fmt, std::format_args args
		) {
			std::lock_guard<std::mutex> lock(_lock);

			auto bookmark = get_scratch_bookmark();
			auto text = bookmark.create_string();
			auto it = std::back_inserter(text);
			std::vformat_to(it, fmt, std::move(args));

			auto time = std::chrono::high_resolution_clock::now() - _startup;
			_do_log(time, loc, reinterpret_cast<const char*>(type), c, text.c_str());
		}
		/// Logs the given string to the console.
		void _do_log(
			std::chrono::high_resolution_clock::duration, std::source_location,
			const char *type, console::color c, const char *text
		);

		std::mutex _lock; ///< Only one thread can log at any given time.
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
		template <typename ...Args> void debug(std::format_string<Args...> fmt, Args &&...args) {
			_logger.debug(_loc, fmt.get(), std::make_format_args(args...));
		}
		/// Logs an info entry.
		template <typename ...Args> void info(std::format_string<Args...> fmt, Args &&...args) {
			_logger.info(_loc, fmt.get(), std::make_format_args(args...));
		}
		/// Logs a warning entry.
		template <typename ...Args> void warn(std::format_string<Args...> fmt, Args &&...args) {
			_logger.warn(_loc, fmt.get(), std::make_format_args(args...));
		}
		/// Logs an error entry.
		template <typename ...Args> void error(std::format_string<Args...> fmt, Args &&...args) {
			_logger.error(_loc, fmt.get(), std::make_format_args(args...));
		}
	private:
		/// Initializes all fields of this struct.
		log_context(std::source_location loc, logger &l) : _loc(loc), _logger(l) {
		}

		std::source_location _loc; ///< Location where this context is created.
		logger &_logger; ///< Logger.
	};
}
