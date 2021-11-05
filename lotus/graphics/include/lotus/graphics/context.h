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
		///
		/// \param wnd The window to create the swap chain for.
		/// \param dev Device that can present to the swap chain.
		/// \param q Command queue that can present to the swap chain.
		/// \param frame_count The requested number of frames in the swap chain. The actual count may be different
		///                    and can be queried using \ref swap_chain::get_image_count().
		/// \param formats List of desired formats for the swap chain, ordered from most favorable to least
		///                favorable. If none of them is available, a random format will be chosen.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count,
			std::span<const format> formats
		) {
			auto [swapchain, f] = backend::context::create_swap_chain_for_window(wnd, dev, q, frame_count, formats);
			return { swap_chain(std::move(swapchain)), f };
		}
		/// \overload
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window &wnd, device &dev, command_queue &q, std::size_t frame_count,
			std::initializer_list<format> formats
		) {
			return create_swap_chain_for_window(wnd, dev, q, frame_count, { formats.begin(), formats.end() });
		}
	protected:
		/// Initializes the base class.
		context(backend::context base) : backend::context(std::move(base)) {
		}
	};
}
