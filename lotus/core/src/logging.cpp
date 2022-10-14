#include "lotus/logging.h"

/// \file
/// Implementation of logging.

namespace lotus {
	logger &logger::instance() {
		static logger _instance;
		return _instance;
	}

	void logger::_do_log(
		std::chrono::high_resolution_clock::duration time, std::source_location loc,
		const char *type, console::color c, const char *text
	) {
		FILE *fout = stdout;
		console::set_foreground_color(console::color::white, fout);
		std::fprintf(fout, "[%6.2f]", std::chrono::duration<double>(time).count());
		console::set_foreground_color(console::color::blue, fout);
		std::fprintf(fout, " %s:%d:%d", loc.file_name(), loc.line(), loc.column());
		console::reset_color(fout);
		std::fprintf(fout, "|");
		console::set_foreground_color(console::color::blue, fout);
		std::fprintf(fout, "%s ", loc.function_name());
		console::set_foreground_color(c, fout);
		std::fprintf(fout, "[%s]", type);
		console::reset_color(fout);
		std::fprintf(fout, " %s\n", text);
	}


	log_context log(std::source_location loc) {
		return log_context(loc, logger::instance());
	}
}
