#pragma once

/// \file
/// Interface to graphics contexts.

#include <utility>

#include "lotus/system/window.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "device.h"
#include "swap_chain.h"

namespace lotus::graphics {
	/// Represents a generic interface to the underlying graphics library.
	class context : public backend::context {
	public:
		/// Initializes the underlying context.
		context() : backend::context() {
		}
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;

		/// Enumerates over all adapters. The callback function will be called for every adapter, and may return a
		/// boolean indicating whether or not to continue enumeration.
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			using _result_t = std::invoke_result_t<Callback&&, adapter>;
			backend::context::_enumerate_adapters([&cb](backend::adapter adap) {
				if constexpr (std::is_same_v<_result_t, bool>) {
					return cb(adapter(std::move(adap)));
				} else {
					static_assert(std::is_same_v<_result_t, void>, "Callback must return bool or nothing");
					cb(adapter(std::move(adap)));
					return true;
				}
			});
		}

		/// Creates a swap chain for the given window.
		[[nodiscard]] swap_chain create_swap_chain_for_window(
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count, pixel_format format
		) {
			return backend::context::create_swap_chain_for_window(wnd, dev, q, frame_count, format);
		}
	};
}
