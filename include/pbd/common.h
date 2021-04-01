#pragma once

/// \file
/// Common types and functions.

namespace pbd {
	/// A type indicating a specific object should not be initialized.
	struct uninitialized_t {
	};
	/// A type indicating a specific object should be zero-initialized.
	struct zero_t {
	};
	constexpr inline uninitialized_t uninitialized; ///< An instance of \ref uninitialized_t.
	constexpr inline zero_t zero; ///< An instance of \ref zero_t.
}
