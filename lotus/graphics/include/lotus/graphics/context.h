#pragma once

/// \file
/// Interface to graphics contexts.

#include <utility>

#include "lotus/system/window.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "common.h"
#include "device.h"
#include "frame_buffer.h"

namespace lotus::graphics {
	/// The name of this backend.
	constexpr static std::u8string_view backend_name = backend::backend_name;

	/// Represents a generic interface to the underlying graphics library.
	class context : public backend::context {
	public:
		/// No default construction.
		context() = delete;
		/// Creates a new context object.
		[[nodiscard]] inline static context create() {
			return backend::context::create();
		}
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;

		/// Enumerates over all adapters. The callback function will be called for every adapter, and may return a
		/// boolean indicating whether or not to continue enumeration.
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			using _result_t = std::invoke_result_t<Callback&&, adapter>;
			backend::context::enumerate_adapters([&cb](backend::adapter adap) {
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
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count, format format
		) {
			return backend::context::create_swap_chain_for_window(wnd, dev, q, frame_count, format);
		}
	protected:
		/// Initializes the base class.
		context(backend::context base) : backend::context(std::move(base)) {
		}
	};
}
