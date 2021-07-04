#pragma once

/// \file
/// Sets up DirectX 12 to be the graphics backend.

#include "directx12/commands.h"
#include "directx12/context.h"
#include "directx12/descriptors.h"
#include "directx12/device.h"
#include "directx12/swap_chain.h"

namespace lotus::graphics {
	namespace backend = backends::directx12;
}
