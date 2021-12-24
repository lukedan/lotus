#pragma once

/// \file
/// Sets up DirectX 12 to be the graphics backend.

#include "directx12/acceleration_structure.h"
#include "directx12/commands.h"
#include "directx12/context.h"
#include "directx12/descriptors.h"
#include "directx12/device.h"
#include "directx12/frame_buffer.h"
#include "directx12/pass.h"
#include "directx12/pipeline.h"
#include "directx12/resources.h"
#include "directx12/synchronization.h"

namespace lotus::graphics::backend {
	using namespace backends::directx12;

	constexpr static std::u8string_view backend_name = u8"DirectX 12"; ///< Name of this backend.
}
