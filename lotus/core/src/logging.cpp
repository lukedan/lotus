#include "lotus/logging.h"

/// \file
/// Implementation of logging.

namespace lotus {
	logger &logger::instance() {
		static logger _instance;
		return _instance;
	}


	log_context log(std::source_location loc) {
		return log_context(loc, logger::instance());
	}
}
