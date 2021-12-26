#pragma once

/// \file
/// Sets up Vulkan to be the graphics backend.

#include "vulkan/acceleration_structure.h"
#include "vulkan/commands.h"
#include "vulkan/context.h"
#include "vulkan/descriptors.h"
#include "vulkan/device.h"
#include "vulkan/frame_buffer.h"
#include "vulkan/pass.h"
#include "vulkan/pipeline.h"
#include "vulkan/resources.h"
#include "vulkan/synchronization.h"

namespace lotus::graphics::backend {
	using namespace backends::vulkan;

	constexpr static std::u8string_view backend_name = u8"Vulkan"; ///< Backend name.
}
