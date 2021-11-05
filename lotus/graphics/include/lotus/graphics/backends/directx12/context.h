#pragma once

/// \file
/// DirectX 12 backend.

#include <utility>

#include <dxgi1_5.h>

#include "lotus/system/platforms/windows/window.h"
#include "lotus/graphics/common.h"
#include "details.h"
#include "device.h"

namespace lotus::graphics::backends::directx12 {
	/// A \p IDXGIFactory5 to access the DirectX 12 library.
	class context {
	protected:
		/// Initializes the DXGI factory.
		[[nodiscard]] static context create();

		/// Enumerates the list of adapters using .
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			for (UINT i = 0; ; ++i) {
				adapter adap = nullptr;
				if (_dxgi_factory->EnumAdapters1(i, &adap._adapter) == DXGI_ERROR_NOT_FOUND) {
					break;
				}
				if (!cb(std::move(adap))) {
					break;
				}
			}
		}
		/// Calls \p CreateSwapChainForHwnd to create a swap chain.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::platforms::windows::window&, device&, command_queue&, std::size_t, std::span<const format>
		);
	private:
		_details::com_ptr<IDXGIFactory5> _dxgi_factory; ///< The DXGI factory.
	};
}
