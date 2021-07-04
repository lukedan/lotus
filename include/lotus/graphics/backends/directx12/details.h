#pragma once

/// \file
/// Common typedefs and functions for DirectX 12.

#include <cstdlib>
#include <iostream>

#include <wrl/client.h>

namespace lotus::graphics::backends::directx12::_details {
	template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>; ///< Reference-counted pointer to a COM object.

	/// Aborts if the given \p HRESULT does not indicate success.
	inline void assert_dx(HRESULT hr) {
		if (FAILED(hr)) {
			std::cerr << "DirectX error: " << hr << std::endl;
			std::abort();
		}
	}
}
