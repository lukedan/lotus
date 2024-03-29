#pragma once

/// \file
/// Sets up Vulkan to be the graphics backend.

#include "vulkan/acceleration_structure.h"
#include "vulkan/commands.h"
#include "vulkan/context.h"
#include "vulkan/descriptors.h"
#include "vulkan/device.h"
#include "vulkan/frame_buffer.h"
#include "vulkan/pipeline.h"
#include "vulkan/resources.h"
#include "vulkan/synchronization.h"

namespace lotus::gpu::backend {
	using namespace backends::vulkan;

	constexpr static backend_type type = backend_type::vulkan; ///< Backend type.
}
