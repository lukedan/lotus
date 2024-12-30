#include "lotus/gpu/backends/metal/context.h"

/// \file
/// Metal device management and creation code.

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "lotus/gpu/backends/metal/details.h"

namespace lotus::gpu::backends::metal {
	context context::create(context_options, _details::debug_message_callback) {
		// TODO honor the options and debug callback?
		return context();
	}

	std::vector<adapter> context::get_all_adapters() const {
		_details::metal_ptr<NS::Array> arr = _details::take_ownership(MTL::CopyAllDevices());

		const auto count = static_cast<std::size_t>(arr->count());
		std::vector<adapter> result;
		result.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			result.emplace_back(adapter(_details::share_ownership(arr->object<MTL::Device>(i))));
		}
		return result;
	}
}
