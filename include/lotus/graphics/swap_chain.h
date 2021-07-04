#pragma once

/// \file
/// Interface to swap chains.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class context;

	/// Generic interface to a swap chain.
	class swap_chain : public backend::swap_chain {
		friend context;
	public:
		/// No copy construction.
		swap_chain(const swap_chain&) = delete;
		/// No copy assignment.
		swap_chain &operator=(const swap_chain&) = delete;
	protected:
		/// Initializes the backend swap chain.
		swap_chain(backend::swap_chain s) : backend::swap_chain(std::move(s)) {
		}
	};
}
